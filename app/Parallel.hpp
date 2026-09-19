#ifndef GORBE_APP_PARALLEL_HPP
#define GORBE_APP_PARALLEL_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace Parallel {

    // fn(i) minden i-re a [0, n) tartományban, legfeljebb annyi szálon, ahány
    // processzormag van; a hívó szál is dolgozik. Visszatéréskor MINDEN hívás
    // befejeződött. Ha valamelyik kivételt dob, az első kivétel a hívóhoz jut.
    //
    // ponytail: szálak hívásonként, nem állandó szálkészlet — egy lépés (ms-ok) mellett
    // a szálindítás (~tíz µs) elhanyagolható; ha sok apró feladat lesz, pool kell.
    template<class Fn>
    void for_each(std::size_t n, Fn&& fn) {
        if (n == 0) return;
        std::size_t const hw = std::max(1u, std::thread::hardware_concurrency());
        std::size_t const threads = std::min(n, hw);
        if (threads == 1) {
            for (std::size_t i = 0; i < n; ++i) fn(i);
            return;
        }

        std::atomic<std::size_t> next{0};
        std::exception_ptr error;
        std::mutex error_mutex;
        auto work = [&] {
            for (std::size_t i; (i = next.fetch_add(1)) < n;) {
                try {
                    fn(i);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (!error) error = std::current_exception();
                }
            }
        };

        std::vector<std::thread> pool;
        pool.reserve(threads - 1);
        for (std::size_t k = 0; k + 1 < threads; ++k) pool.emplace_back(work);
        work();
        for (auto& t : pool) t.join();
        if (error) std::rethrow_exception(error);
    }

}

#endif //GORBE_APP_PARALLEL_HPP
