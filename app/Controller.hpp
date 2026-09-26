#ifndef GORBE_APP_CONTROLLER_HPP
#define GORBE_APP_CONTROLLER_HPP

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../scene/Build.hpp"
#include "../scene/Names.hpp"
#include "../scene/ProjectFile.hpp"
#include "../ui/Requests.hpp"
#include "../utils/Parallel.hpp"
#include "Job.hpp"
#include "Photo.hpp"
#include "Scene.hpp"

// Az MVC VEZÉRLŐJE: a jelenetek (fülek), az aktuális fül és a program-szintű
// paraméterek gazdája. A nézetek kéréseit (Ui::Requests) a frame végén hajtja végre.
class Controller {
public:
    // PROGRAM-szintű paraméterek: minden jelenet (fül) látja őket. Ez a legkülső
    // hatókör; a jelenet- és az alakzat-szintű nevek elfedhetik (mint C++-ban).
    std::list<Param> program_params;

    // A jelenetek (fülek). std::list, mert a Scene címe stabil kell legyen: a
    // mintavételezők és a kamera eseménykezelői rá mutatnak.
    std::list<Scene> scenes;

    Photo::Settings photo;

    // A megnyitott/mentett projektfájl (üres = még nincs mentve), és az utolsó
    // fájlművelet eredménye a panelre.
    std::string project_path;
    std::string file_status;

    Controller() {
        auto& s0 = scenes.emplace_back();
        std::snprintf(s0.name, sizeof(s0.name), "Jelenet 1");
        cur_ = &s0;
        cur_->set_active(true);

        // A szimuláció órája: frame-enként egyszer, az aktuális fül esedékes
        // objektumait PÁRHUZAMOSAN lépteti (a háttérben lévők úgysem futnak).
        sim_sub = Window::Subscription(Window::add_time_passed_event([this](auto ev) {
            if (job_) return;      // hosszú munka közben a szimuláció szünetel
            float const dt = static_cast<float>(ev.dt);
            drag_controls(dt);     // előbb a kontrollpontok mozgatják a felületet,
            step_objects(dt);      // utána lépnek a részecskék
        }));
        btn_sub = Window::Subscription(Window::add_mouse_button_event([this](auto e) {
            on_mouse_button(e.button, e.action, e.mods);
        }));
    }

    // --- kontrollpontok (a jelenet szintjén: egy kattintás EGY objektumot érint) ----
    //
    //   Shift + bal kattintás az alakzaton : új kontrollpont oda (a legközelebbi
    //                                        részecskére, tehát a felületre)
    //   bal gomb + húzás egy kockán         : a pont mozog, a felület követi
    //   Ctrl + bal kattintás egy kockán     : a pont törlése
    // (Alt + bal gomb a kameráé.)

    void on_mouse_button(int button, int action, int mods) {
        if (!cur_ || job_ || button != GLFW_MOUSE_BUTTON_LEFT || (mods & GLFW_MOD_ALT)) return;
        Scene& sc = *cur_;
        if (action == GLFW_RELEASE) { end_drag(sc); return; }
        if (action != GLFW_PRESS) return;
        if (sc.editor) { editor_mouse(sc, mods); return; }

        if (mods & GLFW_MOD_SHIFT) { add_control_at_mouse(sc); return; }

        auto [obj, idx] = pick_control(sc);
        if (obj < 0) return;
        if (mods & GLFW_MOD_CONTROL) {
            auto& cs = shape_at(sc, obj).controls;
            cs.erase(cs.begin() + idx);
        } else {
            sc.drag_obj = obj;
            sc.drag_ctrl = idx;
        }
    }

    // A húzott pont követi az egeret (a kamera nézőirányára merőleges síkban), és a
    // megoldó ehhez igazítja az alakzat paramétereit (ParticleSystem::solve_controls).
    void drag_controls(float dt) {
        if (!cur_ || cur_->drag_obj < 0) return;
        Scene& sc = *cur_;
        if (sc.editor) { drag_constraint(sc); return; }
        auto& model = sc.pool[static_cast<std::size_t>(sc.drag_obj)]->model();
        auto const* cs = model.controls();
        if (!cs || sc.drag_ctrl >= static_cast<int>(cs->size())) { end_drag(sc); return; }
        glm::vec3 const c = (*cs)[static_cast<std::size_t>(sc.drag_ctrl)];
        glm::vec3 const target = sc.camera.get_mouse_pos_on_plane(c, sc.camera.get_front());
        model.solve_controls(sc.drag_ctrl, target, dt);
    }

    // Az esedékes objektumok egy-egy lépése, párhuzamosan (utils/Parallel.hpp).
    //
    // Biztonságos: minden objektumnak saját részecskéi, lefordított programjai (a
    // munkaterületükkel) és véletlenszám-generátora van; a közös adatokat
    // (paraméterek, a `t`, a kifejezésfák) a lépés csak OLVASSA, és a GUI csak a
    // lépések befejezése után írhat újra — ez a hívás megvárja az összeset.
    void step_objects(float dt) {
        if (!cur_) return;
        std::vector<std::pair<SpaceObject*, float>> due;
        for (auto& p : cur_->pool)
            if (float const s = p->controller().advance(dt); s > 0.0f) due.emplace_back(p.get(), s);
        Parallel::for_each(due.size(), [&](std::size_t i) {
            due[i].first->model().step(due[i].second);
        });
    }

    Scene& cur() { return *cur_; }

    // A folyamatban lévő hosszú munka (a folyamatjelzőnek), vagy nullptr.
    Job const* job() const { return job_.get(); }

    // A frame végén, a GUI felépítése UTÁN hívandó. A sorrend számít: előbb a
    // törlések (drop-pal), utána az építés és a fénykép az AKTUÁLIS fülön, és csak
    // legvégül a fülváltás/bezárás — az utóbbi érvénytelenítheti a `cur`-t.
    //
    // Amíg egy hosszú munka tart, csak az halad (a folyamatjelző elnyeli a bevitelt,
    // tehát kérés úgysem jön). Az ebben a frame-ben indult munka első szelete a
    // végén fut le — a gyors munka így rögtön be is fejeződik, jelző nélkül.
    //
    // A frame sorrendje: GUI (benne a jelző) -> ez a hívás -> kirajzolás. Egy lépés
    // tehát a MÁR felépített frame kirajzolása ELŐTT fut. Ezért amikor a jelző először
    // kerül a frame-be, abban a frame-ben nem dolgozunk: előbb ki is rajzolódjon,
    // különben egy egy-két nagy lépésből álló munkánál csak a végén villanna fel.
    void apply(Ui::Requests const& rq) {
        if (job_) {
            if (rq.cancel_job) { cancel_job(); return; }
            if (!job_->shown) { job_->shown = true; return; }
            run_job();
            return;
        }
        apply_requests(rq);
        if (job_ && !job_->slow) run_job();
    }

private:
    void apply_requests(Ui::Requests const& rq) {
        Scene& sc = *cur_;

        // A paramétert az alakzat ELŐTT: a listája lehet épp a törlendő alakzat lokálisa.
        if (rq.erase_param) {
            // Program-szintű változás MINDEN jelenetet érint: a már beparseolt fák a
            // törölt float CÍMÉT tartják, ezért mindenhol el kell dobni őket.
            if (rq.erase_from == &program_params)
                for (auto& other : scenes) other.drop();
            else
                sc.drop();
            rq.erase_from->remove_if([&](Param const& p) { return &p == rq.erase_param; });
        }
        if (rq.erase_shape) {
            if (sc.selected == rq.erase_shape) sc.selected = nullptr;
            sc.drop();              // a törölt alakzat lokálisaira mutathatnak fák
            sc.shapes.remove_if([&](Shape const& s) { return &s == rq.erase_shape; });
        }
        if (rq.drop) sc.drop();

        sc.sync_pool();
        if (rq.build) build(sc, true, "Inditas");
        if (rq.photo) { Job& j = job("Fenykep"); j.slow = true; Photo::queue(j, sc, photo); }
        Scene* const opened = rq.edit_shape ? open_editor(sc, *rq.edit_shape) : nullptr;

        switch_scenes(rq, opened);

        // A projekt-műveletek a legvégén: a betöltés és az új projekt MINDEN jelenetet
        // lecserél, tehát a fenti hivatkozások (sc, cur_) utána érvénytelenek.
        if (!rq.save_path.empty()) save_project(rq.save_path);
        if (!rq.load_path.empty()) load_project(rq.load_path);
        if (rq.new_project)        new_project();
    }

public:
    // --- projekt (scene/ProjectFile.hpp) ------------------------------------------

    void new_project() {
        ProjectFile::Project pr;
        std::snprintf(pr.scenes.emplace_back().name, 32, "Jelenet 1");
        replace_all(std::move(pr));
        project_path.clear();
        file_status = "Uj projekt";
    }

    bool save_project(std::string const& path) {
        // A futó kamerák állása a dokumentumba, hogy a nézőpont is mentődjön.
        for (auto& s : scenes) {
            s.view.eye    = s.camera.get_position();
            s.view.target = s.view.eye + s.camera.get_front();
            s.view.fov    = s.camera.get_fov_deg();
        }
        // A szerkesztő fülek nem mentődnek: az alakzatuk a forrás-jelenetben van.
        std::vector<std::reference_wrapper<Scene const>> saved;
        for (auto const& s : scenes)
            if (!s.editor) saved.emplace_back(s);
        std::ofstream f(path, std::ios::binary);
        if (f) f << ProjectFile::to_text(program_params, saved);
        if (!f) {
            file_status = "HIBA: nem sikerult menteni: " + path;
            return false;
        }
        project_path = path;
        file_status = "Mentve: " + path;
        return true;
    }

    bool load_project(std::string const& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            file_status = "HIBA: nem sikerult megnyitni: " + path;
            return false;
        }
        std::stringstream text;
        text << f.rdbuf();
        std::vector<std::string> warnings;
        try {
            replace_all(ProjectFile::from_text(text.str(), warnings));
        } catch (std::exception const& e) {
            file_status = std::string("HIBA: ") + e.what();   // a régi projekt megmarad
            return false;
        }
        project_path = path;
        file_status = "Megnyitva: " + path;
        for (auto const& w : warnings) file_status += "\n  figyelmeztetes: " + w;
        return true;
    }

    // A jelenetek (bennük a Model-ek: VAO/VBO) felszabadítása MÉG élő GL-kontextussal.
    void shutdown() { job_.reset(); scenes.clear(); cur_ = nullptr; }

private:
    Scene* cur_ = nullptr;
    Window::Subscription sim_sub, btn_sub;

    static Shape& shape_at(Scene& sc, int i) {
        auto it = sc.shapes.begin();
        std::advance(it, i);
        return *it;
    }

    // Az egér alatti sugár (a kamerából a kurzor felé).
    static void mouse_ray(Scene const& sc, glm::vec3& eye, glm::vec3& dir) {
        eye = sc.camera.get_position();
        glm::vec3 const front = sc.camera.get_front();
        dir = glm::normalize(sc.camera.get_mouse_pos_on_plane(eye + front, front) - eye);
    }

    // Az egér alatti LEGKÖZELEBBI kocka: (objektum, pont), vagy (-1, -1).
    std::pair<int, int> pick_control(Scene& sc) const {
        glm::vec3 eye, dir;
        mouse_ray(sc, eye, dir);
        std::pair<int, int> best{-1, -1};
        float best_t = 1e30f;
        int obj = 0;
        for (auto& s : sc.shapes) {
            if (s.visible)
                for (std::size_t k = 0; k < s.controls.size(); ++k) {
                    glm::vec3 const v = s.controls[k] - eye;
                    float const t = glm::dot(v, dir);
                    if (t > 0.0f && t < best_t &&
                        glm::length(glm::cross(v, dir)) < ParticleSystem::CONTROL_RADIUS) {
                        best = {obj, static_cast<int>(k)};
                        best_t = t;
                    }
                }
            ++obj;
        }
        return best;
    }

    // Új kontrollpont a felületre: az egér alatti legközelebbi (látható) részecske
    // helyére, annak az objektumnak, amelyikhez a részecske tartozik.
    void add_control_at_mouse(Scene& sc) {
        int best_obj = -1;
        glm::vec3 best_p{0.0f};
        if (!pick_particle(sc, best_obj, best_p)) return;
        Shape& s = shape_at(sc, best_obj);
        bool const first = s.controls.empty();
        s.controls.push_back(best_p);
        // Az első pontnál újra kell építeni: a pozíció csak így kerül paraméterként a
        // képletbe (Build::place). A részecskék nem indulnak újra.
        if (first) { build(sc, false, "Ujraepites"); run_job(); }   // gyors: jelző nélkül lefut
    }

    // Az egér alatti legközelebbi (látható) részecske: melyik objektumé, és hol van.
    bool pick_particle(Scene& sc, int& best_obj, glm::vec3& best_p) const {
        glm::vec3 eye, dir;
        mouse_ray(sc, eye, dir);
        best_obj = -1;
        float best_t = 1e30f;
        int obj = 0;
        for (auto& s : sc.shapes) {
            if (s.visible && s.tree)
                for (auto const& p : sc.pool[static_cast<std::size_t>(obj)]->model().particles()) {
                    if (Domain::is_outside(p.dom_dist, p.sigma)) continue;
                    glm::vec3 const v = p.p - eye;
                    float const t = glm::dot(v, dir);
                    // Egy korongra kattintva (a sugara σ/2, de legalább egy kicsi hely).
                    float const hit = std::max(0.5f * p.sigma, 0.1f);
                    if (t > 0.0f && t < best_t && glm::length(glm::cross(v, dir)) < hit) {
                        best_obj = obj;
                        best_p = p.p;
                        best_t = t;
                    }
                }
            ++obj;
        }
        return best_obj >= 0;
    }

    // --- variációs szerkesztő (Turk–O'Brien) ------------------------------------
    //
    // A szerkesztő fülön a kontrollpontok az alakzat HATÁRKÉNYSZEREI: húzáskor maga a
    // pont mozog, és a (8) egyenletrendszert újra megoldjuk — nincs közvetett megoldó.
    //   Shift + bal kattintás az alakzaton : új határkényszer (a felület nem változik)
    //   bal gomb + húzás egy kockán         : a kényszer mozog, a felület követi
    //   Ctrl + bal kattintás egy kockán     : a kényszer törlése

    // Egy képletes alakzat átalakítása variációssá (ha még nem az), és a szerkesztő
    // fül megnyitása. A megnyitandó fület adja vissza (nullptr, ha nem sikerült).
    Scene* open_editor(Scene& sc, Shape& s) {
        for (auto& e : scenes)
            if (e.editor && s.vari && !e.shapes.empty() && e.shapes.front().vari == s.vari) {
                e.focus_tab = true;
                return &e;
            }

        int idx = 0;
        for (auto& o : sc.shapes) { if (&o == &s) break; ++idx; }
        auto const& ps = sc.pool[static_cast<std::size_t>(idx)]->model().particles();

        // A variációs függvény: a meglévőt, vagy most, a részecskékből (gyors: a
        // megoldás ~5 ms). A forrás-alakzatba csak a szerkesztő felépítése UTÁN kerül
        // bele (lásd lent), hogy a megszakítás előtt semmi ne változzon.
        std::shared_ptr<Variational> v = s.vari;
        bool const convert = !v;
        glm::vec3 center{s.xform.pos[0], s.xform.pos[1], s.xform.pos[2]};
        if (convert) {
            center = Variational::centroid(ps);
            float const eps = 0.03f * effective_d(s, sc);
            v = std::make_shared<Variational>(Variational::from_particles(ps, center, eps));
            if (v->centers.size() < 20 || !v->solve()) {
                sc.error = std::string(s.name) + ": a szerkeszteshez elobb inditsd el, "
                           "es varj, amig a reszecskek bevonjak a feluletet";
                return nullptr;
            }
        }

        // A forrás részecskéi már a felületen vannak: a szerkesztő ezeket veszi át (az
        // alakzat saját koordinátáiba tolva), így nem kell újra szétterülniük — az
        // 8 részecskéből ~4-5 s volna. Forgatott/méretezett alakzatnál ez nem egy
        // egyszerű eltolás, ott marad az újraindulás.
        std::vector<Particle> moved;
        bool const plain = s.xform.rot[0] == 0.0f && s.xform.rot[1] == 0.0f && s.xform.rot[2] == 0.0f &&
                           s.xform.scale[0] == 1.0f && s.xform.scale[1] == 1.0f && s.xform.scale[2] == 1.0f;
        if (convert || plain)
            for (Particle p : ps) { p.p -= center; moved.push_back(p); }

        auto& ns = scenes.emplace_back();
        ns.editor = true;
        std::snprintf(ns.name, sizeof(ns.name), "Szerk: %s", s.name);
        Shape& e = ns.shapes.emplace_back();
        std::snprintf(e.name, sizeof(e.name), "%s", s.name);
        e.vari = v;
        e.color_idx = s.color_idx;
        e.material_idx = s.material_idx;
        e.own_d = true;
        e.d = effective_d(s, sc);
        ns.selected = &e;
        ns.sync_pool();
        float R = 1.0f;
        for (glm::vec3 c : v->centers) R = std::max(R, glm::length(c));
        ns.camera.look_at(glm::normalize(App::DEFAULT_EYE) * (3.5f * R + 2.0f), glm::vec3(0.0f));
        ns.set_active(false);

        Job& j = job("Szerkeszto megnyitasa");
        j.slow = true;          // a jelző már az első lépés előtt látsszon
        auto const ns_built = build(ns, true, "Szerkeszto megnyitasa");
        j.add("Reszecskek atvetele", [&ns, moved = std::move(moved)] {
            if (moved.size() >= 8) ns.pool.front()->model().particles() = moved;
        });

        // Az átalakítás beírása a forrásba, és a forrás-jelenet újraépítése. A forrás
        // felépítése csak a beírás után "számít": előtte megszakítva a forrás érintetlen.
        auto committed = std::make_shared<bool>(!convert);
        if (convert) {
            auto src = std::make_shared<BuildState>();
            src->active = false;
            j.add("Atalakitas", [&s, v, center, committed, src] {
                // A részecskék világbeli helyéből készült, tehát a warpok, a tartomány és
                // a forgatás/méret már "bele van sütve": csak az eltolás marad (a középpont).
                s.vari = v;
                s.xform.reset();
                for (int i = 0; i < 3; ++i) s.xform.pos[i] = center[i];
                s.warps.clear();
                s.domain[0] = '\0';
                s.controls.clear();
                *committed = true;
                src->active = true;
            });
            build(sc, false, "Szerkeszto megnyitasa", src);
        }

        // Megszakításkor: ha a szerkesztő nem épült fel, vagy a forrás még nem kapta meg
        // a függvényt, a szerkesztő fül bezárul (a forrás érintetlen).
        j.on_cancel.push_back([this, &ns, &sc, ns_built, committed] {
            if (ns_built->done && *committed) return;
            if (cur_ == &ns) { cur_ = &sc; sc.set_active(true); }
            sc.focus_tab = true;
            scenes.remove_if([&](Scene const& x) { return &x == &ns; });
        });
        return &ns;
    }

    // Az egér alatti határkényszer (a legelöl lévő): (objektum, kényszer-index).
    std::pair<int, int> pick_constraint(Scene& sc) const {
        glm::vec3 eye, dir;
        mouse_ray(sc, eye, dir);
        std::pair<int, int> best{-1, -1};
        float best_t = 1e30f;
        int obj = 0;
        for (auto& s : sc.shapes) {
            if (s.visible && s.vari)
                for (std::size_t k = 0; k < s.vari->centers.size(); ++k) {
                    if (s.vari->values[k] != 0.0f) continue;
                    glm::vec3 const v = s.vari->centers[k] - eye;
                    float const t = glm::dot(v, dir);
                    // A kockák sűrűn ülnek: az elkapás sugara csak kicsit nagyobb a kockánál.
                    if (t > 0.0f && t < best_t && glm::length(glm::cross(v, dir)) < 0.2f) {
                        best = {obj, static_cast<int>(k)};
                        best_t = t;
                    }
                }
            ++obj;
        }
        return best;
    }

    // Új vagy törölt kényszer után elég a solve(): a fák a Variational objektumot
    // olvassák (RbfNode), nem az elemek címét, tehát nem kell újraépíteni.
    void editor_mouse(Scene& sc, int mods) {
        if (mods & GLFW_MOD_SHIFT) {
            int obj = -1;
            glm::vec3 p{0.0f};
            if (!pick_particle(sc, obj, p)) return;
            Shape& s = shape_at(sc, obj);
            if (!s.vari) return;
            s.vari->add(p, 0.0f);
            if (!s.vari->solve()) { s.vari->remove(s.vari->centers.size() - 1); s.vari->solve(); }
            return;
        }
        auto [obj, idx] = pick_constraint(sc);
        if (obj < 0) return;
        if (mods & GLFW_MOD_CONTROL) {
            Variational& v = *shape_at(sc, obj).vari;
            Variational const backup = v;
            v.remove(static_cast<std::size_t>(idx));
            if (!v.solve()) { v = backup; v.solve(); }   // túl kevés maradt: nem töröljük
        } else {
            sc.drag_obj = obj;
            sc.drag_ctrl = idx;
        }
    }

    // A húzott határkényszer az egérrel (a nézősíkban) mozog, a normálkényszer-párja
    // vele együtt; utána a (8) újra megoldva. A részecskék a PHI·F visszacsatolással
    // követik a felületet.
    void drag_constraint(Scene& sc) {
        Shape& s = shape_at(sc, sc.drag_obj);
        if (!s.vari || sc.drag_ctrl >= static_cast<int>(s.vari->centers.size())) { end_drag(sc); return; }
        Variational& v = *s.vari;
        auto const i = static_cast<std::size_t>(sc.drag_ctrl);
        glm::vec3 const c = v.centers[i];
        glm::vec3 const delta = sc.camera.get_mouse_pos_on_plane(c, sc.camera.get_front()) - c;
        if (glm::length(delta) < 1e-5f) return;
        int const p = v.partner(i);
        v.centers[i] += delta;
        if (p >= 0) v.centers[static_cast<std::size_t>(p)] += delta;
        if (!v.solve()) {                          // szinguláris állás: visszalépünk
            v.centers[i] -= delta;
            if (p >= 0) v.centers[static_cast<std::size_t>(p)] -= delta;
        }
    }

    void end_drag(Scene& sc) {
        if (sc.drag_obj >= 0 && sc.drag_obj < static_cast<int>(sc.pool.size()))
            sc.pool[static_cast<std::size_t>(sc.drag_obj)]->model().end_drag();
        sc.drag_obj = sc.drag_ctrl = -1;
    }

    // Minden jelenet lecserélése egy betöltött projektre, és a felépítésük, hogy
    // rögtön lássuk is. SORREND: előbb a régi jelenetek szűnnek meg (a fáik a régi
    // program-paraméterek címére mutatnak), csak utána cserélődnek a paraméterek.
    void replace_all(ProjectFile::Project&& pr) {
        job_.reset();          // a félbe maradt munka a régi jelenetekre mutatna
        scenes.clear();
        cur_ = nullptr;
        program_params = std::move(pr.program_params);
        for (auto& doc : pr.scenes) {
            auto& ns = scenes.emplace_back();
            static_cast<SceneDoc&>(ns) = std::move(doc);
            ns.camera.look_at(ns.view.eye, ns.view.target);
            ns.camera.set_fov_deg(ns.view.fov);
            ns.sync_pool();
            build(ns, true, "Projekt betoltese");
            ns.set_active(false);
        }
        cur_ = &scenes.front();
        cur_->set_active(true);
    }

    // Az összes alakzat beparseolása (scene/Build.hpp) és a kész fák átadása a
    // mintavételezőknek — a hosszú munka (Job) lépéseiként: előbb a parseolás (gyors),
    // utána alakzatonként a deriválás és a fordítás (ez a drága: egy ~300 kényszeres
    // variációs alakzat ~185 ms).
    // `restart`: a részecskék újraindulnak-e (az Indításnál igen; egy kontrollpont
    // lerakásakor nem — a felület ugyanaz, csak a képlet kap új paramétert).
    //
    // Megszakításkor a félbe maradt jelenet leáll (drop), hogy ne maradjon benne régi
    // és új felület keverve — vagy a variációs kényszerek átfoglalása után a régi
    // címekre mutató program. Ha `st->active` hamis, a megszakítás nem nyúl hozzá
    // (a hívó dönti el, mikortól "számít" ez a felépítés; lásd open_editor).
    struct BuildState { bool done = false; bool active = true; };

    std::shared_ptr<BuildState> build(Scene& sc, bool restart, char const* title,
                                      std::shared_ptr<BuildState> st = nullptr) {
        if (!st) st = std::make_shared<BuildState>();
        Job& j = job(title);
        auto ok = std::make_shared<bool>(true);
        j.add("Kepletek feldolgozasa", [this, &sc, ok] {
            std::string err = Build::build(sc, program_params);
            if (err.empty()) { sc.error.clear(); return; }
            sc.drop();
            sc.error = err;
            *ok = false;                 // a jelenet többi lépése kimarad
        });
        std::size_t i = 0;
        for (auto const& s : sc.shapes) {
            j.add(std::string(s.name) + ": derivalas es forditas", [&sc, i, restart, ok] {
                if (!*ok) return;
                Shape& s = shape_at(sc, static_cast<int>(i));
                auto& p = *sc.pool[i];
                auto& surf = p.model().surface();
                surf.set_tree(s.tree);
                if (s.dom_tree) surf.set_domain(s.dom_tree);
                else            surf.clear_domain();
                p.model().bind_controls(&s.controls, Build::control_params(s));
                if (restart) p.start();     // saját kezdő részecskék + futó állapot
            });
            ++i;
        }
        j.add("Kesz", [st] { st->done = true; });
        j.on_cancel.push_back([&sc, st] {
            if (st->done || !st->active) return;
            sc.drop();
            sc.error = "Megszakitva: nyomj Inditot";
        });
        return st;
    }

    // A felhasználó megszakította a munkát: a hátralévő lépések kimaradnak, a
    // kezelők (a felvétel sorrendjében) következetes állapotba hozzák, amit érintett.
    void cancel_job() {
        auto handlers = std::move(job_->on_cancel);
        job_.reset();
        cur_->camera.input_enabled = true;
        for (auto& h : handlers) h();
    }

    // --- hosszú munka (app/Job.hpp) ------------------------------------------------

    std::unique_ptr<Job> job_;

    // A folyamatban lévő munka, vagy egy új. Ha egy frame-ben több is indul (pl.
    // Indítás és fénykép), egymás után, ugyanabba a munkába kerülnek.
    Job& job(char const* title) {
        if (!job_) job_ = std::make_unique<Job>(title);
        return *job_;
    }

    // Egy frame-nyi szelet (~30 ms). Amíg tart, a kamera sem kap bevitelt.
    void run_job() {
        job_->run_for(0.03);
        if (!job_->finished()) { cur_->camera.input_enabled = false; return; }
        auto done = std::move(job_->on_done);
        job_.reset();
        cur_->camera.input_enabled = true;
        if (done) done();
    }

    // `opened`: az épp megnyitott szerkesztő fül — elsőbbsége van a fülsáv kiválasztásával szemben.
    void switch_scenes(Ui::Requests const& rq, Scene* opened = nullptr) {
        Scene* want = opened ? opened : rq.want_scene ? rq.want_scene : cur_;
        if (want != cur_ || rq.close_scene || rq.new_scene) end_drag(*cur_);

        // Új jelenet (a "+" fülről). Csak itt, a fülsáv ciklusa után: a ciklus
        // minden frame-ben a KIVÁLASZTOTT fülre állítja a want_scene-t.
        if (rq.new_scene) {
            auto& ns = scenes.emplace_back();
            next_name(scenes, "Jelenet ", ns.name, sizeof(ns.name));
            ns.set_active(false);
            want = &ns;
        }

        if (rq.close_scene) {
            bool const closing_current = (rq.close_scene == cur_);
            scenes.remove_if([&](Scene const& s) { return &s == rq.close_scene; });
            if (closing_current || want == rq.close_scene) {
                cur_ = &scenes.front();
                cur_->set_active(true);
            }
        } else if (want != cur_) {
            cur_->set_active(false);      // a részecskék állapota megmarad
            cur_ = want;
            cur_->set_active(true);
        }
    }
};

#endif //GORBE_APP_CONTROLLER_HPP
