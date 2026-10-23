#ifndef GORBE_UTILS_LOG_HPP
#define GORBE_UTILS_LOG_HPP

#include <cstddef>
#include <deque>
#include <string>
#include <utility>

// A program konzolja (ui/ConsolePanel.hpp): soronként szint, idő, forrás és szöveg.
// Ide írnak az alakzatok kódjai (gorbe::ShapeScript::print / error), a fordítás
// eredménye és a futás hibái.
//
// Egymás utáni EGYFORMA sor (ugyanaz a szint, forrás és szöveg) nem ismétlődik, csak
// a számlálója nő: egy minden ciklusban logoló update() így nem árasztja el a konzolt.
// A legrégebbi sorok MAX_LINES fölött kiesnek.
//
// ponytail: zár nélkül, mert csak a fő szálról írnak bele (az update(), a Job lépései
// és a GUI); ha egy háttérszál is logolna, ide mutex kell.
namespace Log {

    enum class Level { Info, Warning, Error };

    struct Line {
        Level       level = Level::Info;
        float       t = 0.0f;          // a program ideje (a képletek `t`-je), az első előforduláskor
        std::string source;            // pl. az alakzat neve; üres = a program maga
        std::string text;
        int         repeat = 1;        // ennyiszer jött egymás után
    };

    inline constexpr std::size_t MAX_LINES = 2000;

    inline std::deque<Line> lines;

    inline void add(Level level, std::string source, std::string text, float t) {
        if (!lines.empty()) {
            Line& last = lines.back();
            if (last.level == level && last.source == source && last.text == text) {
                ++last.repeat;
                return;
            }
        }
        lines.push_back({level, t, std::move(source), std::move(text), 1});
        while (lines.size() > MAX_LINES) lines.pop_front();
    }

    inline void clear() { lines.clear(); }

}

#endif //GORBE_UTILS_LOG_HPP
