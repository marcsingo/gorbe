#ifndef GORBE_APP_PARALLEL_HPP
#define GORBE_APP_PARALLEL_HPP

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

namespace Parallel {

    namespace detail {

        // Igaz a szálkészlet szálain: az onnan jövő (beágyazott) hívás sorosan fut.
        inline thread_local bool in_pool = false;

        struct Job {
            std::size_t n;
            std::function<void(std::size_t)> const& fn;
            std::atomic<std::size_t> next{0};
            std::exception_ptr error;
            std::mutex error_mutex;

            void work() {
                for (std::size_t i; (i = next.fetch_add(1)) < n;) {
                    try {
                        fn(i);
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!error) error = std::current_exception();
                    }
                }
            }
        };

        // Tartós szálkészlet (magok száma - 1 szál; a hívó a maradék egy).
        //
        // Egyszerre EGY feladatot dolgoz fel. Ha foglalt (pl. az objektumok már
        // párhuzamosan lépnek, és egy objektum a saját lépésén belül is párhuzamosítana),
        // a run() hamissal tér vissza, és a hívó sorosan fut: így nincs holtpont, és a
        // magok sincsenek túlterhelve.
        class Pool {
            std::vector<std::thread> workers;
            std::mutex m;
            std::condition_variable wake, finished_cv;
            Job* current = nullptr;
            unsigned gen = 0;
            std::size_t wanted = 0, started = 0, finished = 0;
            bool busy = false, stop = false;

            void loop() {
                in_pool = true;
                std::unique_lock<std::mutex> lk(m);
                unsigned seen = gen;
                for (;;) {
                    wake.wait(lk, [&] { return stop || (gen != seen && current && started < wanted); });
                    if (stop) return;
                    seen = gen;
                    ++started;
                    Job* job = current;
                    lk.unlock();
                    job->work();
                    lk.lock();
                    ++finished;
                    finished_cv.notify_all();
                }
            }

        public:
            Pool() {
                unsigned const hw = std::max(1u, std::thread::hardware_concurrency());
                for (unsigned k = 0; k + 1 < hw; ++k) workers.emplace_back([this] { loop(); });
            }
            ~Pool() {
                { std::lock_guard<std::mutex> lk(m); stop = true; }
                wake.notify_all();
                for (auto& t : workers) t.join();
            }

            static Pool& get() { static Pool p; return p; }
            std::size_t size() const { return workers.size(); }

            // A feladat lefuttatása legfeljebb `helpers` segítővel (a hívó is dolgozik).
            // Hamis, ha a szálkészlet épp foglalt — ilyenkor semmi nem futott le.
            bool run(Job& job, std::size_t helpers) {
                std::unique_lock<std::mutex> lk(m);
                if (busy) return false;
                busy = true;
                current = &job;
                wanted = helpers;
                started = finished = 0;
                ++gen;
                lk.unlock();
                wake.notify_all();

                job.work();

                lk.lock();
                wanted = started;                    // ezután már senki nem csatlakozik
                finished_cv.wait(lk, [&] { return finished == started; });
                current = nullptr;
                busy = false;
                return true;
            }
        };
    }

    // Az elérhető szálak száma (a hívóval együtt).
    inline std::size_t threads() { return detail::Pool::get().size() + 1; }

    // fn(i) minden i-re a [0, n) tartományban, legfeljebb `max_threads` szálon (a hívó
    // is dolgozik). Visszatéréskor MINDEN hívás befejeződött; ha valamelyik kivételt
    // dob, az első kivétel a hívóhoz jut. Beágyazott hívás, vagy ha a szálkészlet
    // foglalt: sorosan fut.
    template<class Fn>
    void for_each(std::size_t n, Fn&& fn,
                  std::size_t max_threads = std::numeric_limits<std::size_t>::max()) {
        if (n == 0) return;
        auto& pool = detail::Pool::get();
        std::size_t const helpers = std::min({pool.size(), n - 1,
                                              max_threads > 0 ? max_threads - 1 : 0});
        std::function<void(std::size_t)> const f = [&](std::size_t i) { fn(i); };
        detail::Job job{n, f};
        if (helpers == 0 || detail::in_pool || !pool.run(job, helpers)) {
            job.work();      // sorosan (ugyanazzal a kivétel-kezeléssel)
        }
        if (job.error) std::rethrow_exception(job.error);
    }

}

#endif //GORBE_APP_PARALLEL_HPP
