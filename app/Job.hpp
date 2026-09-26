#ifndef GORBE_APP_JOB_HPP
#define GORBE_APP_JOB_HPP

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Hosszú művelet (felépítés, fénykép, betöltés) LÉPÉSEKRE bontva. A vezérlő
// képkockánként egy kis időkeretnyi lépést futtat, közben a UI kirajzolja a
// folyamatjelzőt (ui/ProgressPopup.hpp).
//
// Szándékosan nem háttérszál: a lépések a fő szálon futnak, tehát nincs
// versenyhelyzet a szimulációval, a rajzolással és a GUI-val. Amíg a munka tart, a
// szimuláció és a bevitel szünetel, így a félkész állapothoz senki nem nyúl.
struct Job {
    using Clock = std::chrono::steady_clock;

    std::string title;                                               // pl. "Fenykep"
    std::vector<std::pair<std::string, std::function<void()>>> steps;
    std::function<void()> on_done;                                   // ha végigfutott
    // Megszakításkor, a felvétel sorrendjében: mindegyik a saját részét hozza
    // következetes állapotba (pl. a félbe maradt felépítésű jelenetet leállítja).
    std::vector<std::function<void()>> on_cancel;
    std::size_t next = 0;
    Clock::time_point started = Clock::now();

    // Biztosan lassú (pl. fénykép): már az első lépés ELŐTT kirajzoljuk a folyamatjelzőt.
    bool slow = false;
    // Volt-e már a képernyőn a folyamatjelző (lásd Controller::apply).
    bool shown = false;

    explicit Job(std::string t) : title(std::move(t)) {}

    void add(std::string label, std::function<void()> fn) {
        steps.emplace_back(std::move(label), std::move(fn));
    }

    bool  finished() const { return next >= steps.size(); }
    float progress() const {
        return steps.empty() ? 1.0f : static_cast<float>(next) / static_cast<float>(steps.size());
    }
    std::string const& current() const {
        static std::string const none;
        return finished() ? none : steps[next].first;
    }
    double elapsed() const { return std::chrono::duration<double>(Clock::now() - started).count(); }

    // A hátralévő lépések kihagyása (pl. egy hiba után). A felhasználói megszakítás
    // ennél több: az on_cancel kezelőket is lefuttatja (Controller::cancel_job).
    void cancel() { next = steps.size(); }

    // A következő lépés. A függvényt előbb kivesszük a tömbből, így a lépés a tömböt
    // is bővítheti (egy átfoglalás nem a futó függvényt szabadítja fel).
    void run_step() {
        auto fn = std::move(steps[next].second);
        ++next;
        fn();
    }

    // Lépések, amíg az időkeret tart (legalább egy): a gyors munka így egyetlen
    // képkockán belül lefut, és folyamatjelző sem villan fel.
    void run_for(double seconds) {
        auto const t0 = Clock::now();
        do run_step();
        while (!finished() && std::chrono::duration<double>(Clock::now() - t0).count() < seconds);
    }
};

#endif //GORBE_APP_JOB_HPP
