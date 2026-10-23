#ifndef GORBE_SCRIPT_PLATFORM_HPP
#define GORBE_SCRIPT_PLATFORM_HPP

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Az alakzatkód fordításának és betöltésének platformfüggő része — CSAK itt van
// #ifdef. A program többi része (script/Compiler.hpp, a vezérlő) csak ezeket
// használja:
//
//     find_compiler(err)                    a fordító környezete (üres: nincs)
//     compile_command(env, sources, ...)    a fordító parancssora
//     Process                               külső folyamat: indítás, várás, leállítás
//     Module                                DLL / .so: betöltés, szimbólum, elengedés
//     open_path(p, err)                     fájl a hozzárendelt programmal / mappa
//
// Az útvonalak std::filesystem::path-ok: Windowson a széles (UTF-16) API-t hívjuk,
// így az ékezetes mappanevek (pl. "5. félév") is működnek.
//
// ponytail: a Linux-ág még csonk (hibát ad); a g++ -shared -fPIC + fork/exec +
// dlopen ugyanezt a felületet kapja meg.
// ---------------------------------------------------------------------------

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifdef APIENTRY    // a glad már definiálta; a windows.h ugyanígy (__stdcall) teszi
#    undef APIENTRY
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <cstdio>
#  undef near        // a windows.h régi makrói: elrontanák a near/far nevű függvényeket
#  undef far
#endif

// A DLL C-futtatókönyvtára a programéval egyezzen (a CMake adja meg: Debug buildben
// /MDd, egyébként /MD).
#ifndef GORBE_SCRIPT_CRT
#define GORBE_SCRIPT_CRT "/MD"
#endif

namespace Script::Platform {

    namespace fs = std::filesystem;

#if defined(_WIN32)

    inline std::wstring widen(std::string const& s) { return fs::path(s).wstring(); }

    // A Visual Studio C++ fordítójának környezete: a vcvars64.bat útvonala, a
    // vswhere.exe-vel megkeresve (ez minden VS 2017+ telepítésnél ott van). Egyszer
    // keressük, utána a tárolt eredményt adjuk.
    inline fs::path find_compiler(std::string& err) {
        static fs::path cached;
        static std::string cached_err;
        static bool done = false;
        if (!done) {
            done = true;
            wchar_t pf[MAX_PATH] = L"";
            GetEnvironmentVariableW(L"ProgramFiles(x86)", pf, MAX_PATH);
            fs::path const vswhere = fs::path(pf) / L"Microsoft Visual Studio" / L"Installer" / L"vswhere.exe";
            std::wstring const cmd = L"\"\"" + vswhere.wstring() + L"\" -latest -products * "
                L"-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath\"";
            std::string out;
            if (FILE* p = _wpopen(cmd.c_str(), L"r")) {
                char buf[512];
                while (std::fgets(buf, sizeof(buf), p)) out += buf;
                _pclose(p);
            }
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            fs::path const bat = fs::path(out) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
            std::error_code ec;
            if (!out.empty() && fs::is_regular_file(bat, ec)) cached = bat;
            else cached_err = "nem talalhato a Visual Studio C++ fordito (vswhere: " + vswhere.string() +
                              "); telepitsd a \"Desktop development with C++\" csomagot";
        }
        err = cached_err;
        return cached;
    }

    // A fordító parancssora: minden forrás egy DLL-be. A kimenet (hibák,
    // figyelmeztetések) a `log` fájlba kerül; a VSLANG=1033 angol (tehát ASCII)
    // üzeneteket kér, hogy a panelen ne legyen kódlap-zagyvaság.
    inline std::wstring compile_command(fs::path const& env, std::vector<fs::path> const& sources,
                                        fs::path const& include_dir, fs::path const& out_dll,
                                        fs::path const& obj_dir, fs::path const& log) {
        auto q = [](fs::path const& p) { return L"\"" + p.wstring() + L"\""; };
        // A záró \" előtti \ a cl-nek "\\"-ként kell, különben az idézőjelet escape-eli.
        std::wstring cmd = L"cmd.exe /s /c \"" + q(env) + L" >nul 2>&1 && set \"VSLANG=1033\" && "
            L"cl /nologo /LD /W3 /O2 /std:c++20 /EHsc /utf-8 " + widen(GORBE_SCRIPT_CRT) +
            L" /I" + q(include_dir) + L" /Fo\"" + obj_dir.wstring() + L"\\\\\" /Fe" + q(out_dll);
        for (auto const& s : sources) cmd += L" " + q(s);
        cmd += L" > " + q(log) + L" 2>&1\"";
        return cmd;
    }

    // Egy külső folyamat. A Windows "job object"-be kerül, így a destruktor (pl. a
    // felhasználó megszakítja az Indítást, vagy kilép a program) a teljes folyamatfát
    // leállítja — a cmd.exe-t ÉS az általa indított cl.exe-t is.
    class Process {
    public:
        Process() = default;
        Process(Process const&) = delete;
        Process& operator=(Process const&) = delete;
        ~Process() {
            if (job_) CloseHandle(job_);       // KILL_ON_JOB_CLOSE: a fa leáll
            if (proc_) CloseHandle(proc_);
        }

        bool start(std::wstring cmd, fs::path const& cwd, std::string& err) {
            job_ = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
            li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (job_) SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &li, sizeof(li));
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            // Felfüggesztve indul, hogy a gyereke se szökhessen ki a job object elől.
            if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                                cwd.wstring().c_str(), &si, &pi)) {
                err = "nem indithato a fordito (CreateProcess hiba " + std::to_string(GetLastError()) + ")";
                return false;
            }
            if (job_) AssignProcessToJobObject(job_, pi.hProcess);
            ResumeThread(pi.hThread);
            CloseHandle(pi.hThread);
            proc_ = pi.hProcess;
            return true;
        }

        // Legfeljebb `ms` ezredmásodpercig vár; igaz, ha a folyamat véget ért.
        bool wait(int ms) {
            return !proc_ || WaitForSingleObject(proc_, static_cast<DWORD>(ms)) == WAIT_OBJECT_0;
        }

        int exit_code() const {
            DWORD c = 1;
            if (proc_) GetExitCodeProcess(proc_, &c);
            return static_cast<int>(c);
        }

    private:
        HANDLE proc_ = nullptr;
        HANDLE job_  = nullptr;
    };

    // Egy betöltött DLL. A Windows a betöltött DLL-t nem engedi felülírni, ezért
    // újrafordítás ELŐTT unload() kell — és előtte a belőle jött példányok törlése.
    class Module {
    public:
        Module() = default;
        Module(Module const&) = delete;
        Module& operator=(Module const&) = delete;
        ~Module() { unload(); }

        bool load(fs::path const& p, std::string& err) {
            unload();
            h_ = LoadLibraryW(p.wstring().c_str());
            if (!h_) err = "nem toltheto be: " + p.string() + " (hiba " + std::to_string(GetLastError()) + ")";
            return h_ != nullptr;
        }
        void unload() {
            if (h_) FreeLibrary(h_);
            h_ = nullptr;
        }
        bool loaded() const { return h_ != nullptr; }
        void* symbol(std::string const& name) const {
            return h_ ? reinterpret_cast<void*>(GetProcAddress(h_, name.c_str())) : nullptr;
        }

    private:
        HMODULE h_ = nullptr;
    };

    // Egy fájl megnyitása a hozzárendelt programmal (a .cpp-hez beállított
    // szerkesztővel), vagy egy mappa a fájlkezelőben. Ha a .cpp-hez nincs program
    // rendelve, a Jegyzettömb nyitja meg.
    inline bool open_path(fs::path const& p, std::string& err) {
        auto const r = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (r > 32) return true;
        if (r == SE_ERR_NOASSOC) {
            std::wstring const arg = L"\"" + p.wstring() + L"\"";
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"notepad.exe", arg.c_str(),
                                                        nullptr, SW_SHOWNORMAL)) > 32)
                return true;
        }
        err = "nem nyithato meg: " + p.string();
        return false;
    }

#else  // ----------------------------------------------------------- csonk (Linux)

    inline fs::path find_compiler(std::string& err) {
        err = "az alakzatkod forditasa ezen a platformon meg nincs megvalositva";
        return {};
    }
    inline std::wstring compile_command(fs::path const&, std::vector<fs::path> const&, fs::path const&,
                                        fs::path const&, fs::path const&, fs::path const&) { return {}; }
    class Process {
    public:
        bool start(std::wstring const&, fs::path const&, std::string& err) {
            err = "nincs megvalositva";
            return false;
        }
        bool wait(int) { return true; }
        int  exit_code() const { return 1; }
    };
    class Module {
    public:
        bool  load(fs::path const&, std::string& err) { err = "nincs megvalositva"; return false; }
        void  unload() {}
        bool  loaded() const { return false; }
        void* symbol(std::string const&) const { return nullptr; }
    };
    inline bool open_path(fs::path const&, std::string& err) { err = "nincs megvalositva"; return false; }

#endif

}

#endif //GORBE_SCRIPT_PLATFORM_HPP
