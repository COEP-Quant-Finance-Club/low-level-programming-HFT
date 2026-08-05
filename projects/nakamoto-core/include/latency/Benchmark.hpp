#pragma once

// -----------------------------------------------------------------------------
// Benchmark.hpp — Nakamoto Core benchmarking framework, V1.
//
// This header is deliberately narrow in scope: it answers "how long did N
// calls to this callable take" and nothing else. Percentiles, warmup passes,
// CSV export, and multi-benchmark aggregation are explicitly out of scope
// for V1 and will build on top of this, not inside it.
// -----------------------------------------------------------------------------

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nkm {

// Plain aggregate, not an encapsulated class. This result is produced and
// consumed immediately by the caller — there's no invariant to protect, so
// getters/setters would just be ceremony.
//
// `name` is a string_view, not std::string, so constructing a BenchmarkResult
// never allocates. In every realistic call site `name` is a string literal
// (e.g. "OrderBook::insert"), so the caller trivially outlives the view.
struct BenchmarkResult
{
    std::string_view name;
    std::uint64_t    iterations;
    std::uint64_t    total_ns;
    double           avg_ns;

    void print() const
    {
        std::cout << "[benchmark] " << name << '\n'
                   << "  iterations   : " << iterations << '\n'
                   << "  total time   : " << total_ns << " ns\n"
                   << "  average time : " << avg_ns    << " ns/iter\n";
    }
};

// Constrains Fn to "callable with no arguments". A concept gives a readable
// compiler error at the call site instead of a wall of SFINAE noise if
// someone passes something non-invocable — no reason to reach for
// pre-C++20 idioms here.
template <typename Fn>
concept NullaryCallable = std::is_invocable_v<Fn&>;

// Times `iterations` calls to `fn`. Returns the result; does not print.
//
// Fn&& is a forwarding reference, not std::function<void()>. That's the
// whole point of this design:
//   - std::function type-erases the callable behind a vtable-like indirect
//     call, and its small-buffer optimization is not guaranteed — a capture
//     that overflows it heap-allocates. Both are exactly the kind of hidden
//     cost a latency benchmark must not introduce into itself.
//   - A template parameter keeps Fn's concrete type visible to the compiler
//     at the call site, so `callable()` in the loop below is a direct,
//     inlinable call — for a small lambda, typically inlined to nothing
//     more than its body, with zero dispatch overhead.
template <NullaryCallable Fn>
[[nodiscard]] BenchmarkResult run_benchmark(std::string_view name,
                                             std::uint64_t iterations,
                                             Fn&& fn)
    noexcept(std::is_nothrow_invocable_v<Fn&>)
{
    // Perfect forwarding, applied once: Fn deduces to `T&` for an lvalue
    // argument and to `T` for an rvalue/temporary. std::forward<Fn>(fn)
    // therefore copies a named lvalue callable and moves a temporary one,
    // matching the caller's intent exactly. We do this decay-copy up front
    // and bind it to a local, so the hot loop below calls a plain stack
    // object (`callable`) rather than going through the parameter's
    // reference on every single iteration.
    std::remove_reference_t<Fn> callable(std::forward<Fn>(fn));

    // steady_clock over system_clock: steady_clock is monotonic — it cannot
    // jump backward due to NTP adjustment or manual clock changes. That's
    // the only property that matters when measuring an elapsed duration;
    // wall-clock time-of-day is irrelevant here.
    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();

    for (std::uint64_t i = 0; i < iterations; ++i)
    {
        callable();
    }

    const Clock::time_point end = Clock::now();

    // duration_cast performs the unit conversion using Clock::period at
    // compile time, rather than us hand-rolling a multiplier that would be
    // silently wrong if the platform's steady_clock resolution ever
    // differs from what we assumed.
    const auto total_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

    const double avg_ns = (iterations > 0)
        ? static_cast<double>(total_ns) / static_cast<double>(iterations)
        : 0.0;

    return BenchmarkResult{name, iterations, total_ns, avg_ns};
}

} // namespace nkm