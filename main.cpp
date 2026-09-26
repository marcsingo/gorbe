#include "App.hpp"

#include "app/Controller.hpp"
#include "scene/Time.hpp"
#include "scene/Validate.hpp"
#include "ui/HelpPanel.hpp"
#include "ui/Layout.hpp"
#include "ui/MenuBar.hpp"
#include "ui/ParamsPanel.hpp"
#include "ui/ProgressPopup.hpp"
#include "ui/PropsPanel.hpp"
#include "ui/Requests.hpp"
#include "ui/ShapesPanel.hpp"
#include "ui/ViewportPanel.hpp"

// A frontend MVC szerint:
//   MODEL      scene/  — az adatok (Document) és a tiszta logika (Build, Validate):
//                        GL és ImGui nélkül, tesztelhető (tests/test_build.cpp)
//   VIEW       ui/     — a panelek; az értékeket élőben írják, minden mást KÉRNEK
//   CONTROLLER app/    — a jelenetek gazdája; a kéréseket a frame végén hajtja végre
int main() {
    App app{1200, 800, "Particle sampling"};
    Controller ctl;
    Ui::UiState ui;

    // A rajzolás mindig az AKTUÁLIS fül tartalmát mutatja, a saját kamerájával.
    app.set_draw([&] {
        app.get_axes().draw(ctl.cur().camera);
        ctl.cur().draw();
    });

    app.set_gui([&] {
        // A `t` beépített idő frissítése. A kifejezésfa ennek a floatnak a CÍMÉT
        // tárolja, ezért elég az értéket átírni — nem kell újraparseolni.
        // Hosszú munka (pl. fénykép) alatt az idő áll: a sávok ugyanazt a pillanatot lássák.
        if (!ctl.job()) SceneTime::value = static_cast<float>(glfwGetTime());

        Scene& sc = ctl.cur();
        Problems const pr = validate(ctl.program_params, sc);
        Ui::Requests rq;

        // A menüsor ELŐSZÖR: a magasságát az ImGui a munkaterületből vonja le,
        // az elrendezés pedig már ebből számol.
        Ui::menu_bar(ctl.project_path, ui, rq);
        Ui::Geometry const g = Ui::compute_geometry(ui.layout);

        Ui::shapes_panel(sc, pr, ctl.photo, ctl.file_status, ui, rq, g);
        Ui::props_panel(sc, ctl.program_params, pr, ui, rq, g);
        Ui::help_panel(sc.camera, app.get_axes(), ui.layout, g);
        Ui::params_panel(sc, ctl.program_params, pr, rq, ui.layout, g);
        Ui::viewport_panel(app, ctl.scenes, ui, rq, g);
        Ui::splitters(ui.layout, g);
        Ui::progress_popup(ctl.job(), rq);

        ctl.apply(rq);
    });

    app.run();

    // A jelenetek (bennük a Model-ek: VAO/VBO) felszabadítása MÉG élő GL-kontextussal,
    // csak utána az ablak és a shader-cache.
    ctl.shutdown();
    app.shutdown();
    return 0;
}
