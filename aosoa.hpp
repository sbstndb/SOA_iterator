#pragma once
#include <array>
#include <tuple>
#include <vector>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <iterator>

// AVX2 is required for the opt-in hand-written reductions (sum_all_f32_avx2
// and compute_all_f32_avx2). Everything else is portable C++20.
#if defined(__AVX2__)
  #include <immintrin.h>
  #define AOSOA_HAS_AVX2 1
#else
  #define AOSOA_HAS_AVX2 0
#endif

// Block: one SOA tile of fixed capacity B, stored inline.
template<size_t B, typename... Ts>
struct alignas(64) Block {
    std::tuple<std::array<Ts, B>...> data;
    template<size_t I> auto& array()             { return std::get<I>(data); }
    template<size_t I> const auto& array() const { return std::get<I>(data); }
};

// AoSoA: array of SOA blocks.
//
// The primary API is functional: for_each / for_each_indexed / for_each_field /
// reduce / filter. Each takes a lambda that receives sizeof...(Ts) references
// (one per field) and is called once per element. The library drives a clean
// block-then-index loop internally so GCC can unroll + auto-vectorize the hot
// path. This is what the original element-level random-access Iterator could
// not do — its modulo-B branch in operator++ defeated the vectorizer.
//
// for_each_block is the power-user escape hatch: you get the raw Block and a
// valid-element count, and can write any custom traversal (e.g. split arrays
// into per-field locals, hand-fuse loops) without giving up the container.
template<size_t B, typename... Ts>
class AoSoA {
    static_assert(B > 0, "Block size must be positive");
public:
    using BlockT = Block<B, Ts...>;
    std::vector<BlockT> blocks;
    size_t size_ = 0;

    static constexpr size_t block_size()  { return B; }
    static constexpr size_t field_count() { return sizeof...(Ts); }

    AoSoA() = default;
    explicit AoSoA(size_t n) { resize(n); }

    size_t size() const       { return size_; }
    size_t num_blocks() const { return blocks.size(); }

    void resize(size_t n) {
        const size_t needed = (n + B - 1) / B;
        blocks.resize(needed);
        size_ = n;
    }

    void reserve(size_t n) {
        blocks.reserve((n + B - 1) / B);
    }

    template<typename... Args>
    void push_back(Args&&... args) {
        const size_t off = size_ % B;
        if (off == 0) blocks.emplace_back();
        write_at(blocks.back(), off,
                 std::forward_as_tuple(std::forward<Args>(args)...),
                 std::index_sequence_for<Ts...>{});
        ++size_;
    }

    // ========================================================================
    // Primary API: functional, lambda-driven.
    //
    // The lambda receives one reference per field. Because the library expands
    // the block traversal internally and B is a compile-time constant, the
    // inner loop has no branches and the compiler can fully SIMD-ize it.
    // ========================================================================

    // Apply f(refs...) to every element.
    template<class F>
    void for_each(F&& f) {
        for_each_impl(std::forward<F>(f), std::index_sequence_for<Ts...>{});
    }
    template<class F>
    void for_each(F&& f) const {
        for_each_const_impl(std::forward<F>(f), std::index_sequence_for<Ts...>{});
    }

    // Unrolled variant: processes UNROLL blocks per outer iteration. The
    // compiler sees UNROLL back-to-back copies of the inner element loop and
    // schedules them as independent streams, giving the OoO engine / HW
    // prefetcher multiple "virtual streams" to track (mimicking SOA's 8-stream
    // pattern from a single block-stream).
    //
    // Empirically UNROLL=4 with B=16 gives the best L3-resident speedup on
    // float8: ~28% faster than plain for_each at N=262K, and crosses SOA
    // (0.88-0.92x vs SOA). In-cache it is at parity with for_each. At 1M
    // (DRAM-bound), unrolling does not help — the bottleneck moves from MLP
    // to TLB/DRAM-row-switching and needs a different approach.
    //
    // Use this as an opt-in path when your working set is L3-resident and
    // B=16. Leave the default for_each for everything else.
    template<size_t UNROLL, class F>
    void for_each_unrolled(F&& f) {
        for_each_unrolled_impl<UNROLL>(std::forward<F>(f),
                                       std::index_sequence_for<Ts...>{});
    }
    template<size_t UNROLL, class F>
    void for_each_unrolled(F&& f) const {
        for_each_unrolled_const_impl<UNROLL>(std::forward<F>(f),
                                             std::index_sequence_for<Ts...>{});
    }

    // Multi-stream variant: splits the block vector into K far-apart segments
    // and processes one block from each segment per outer iteration. The K
    // base pointers are segment_size * sizeof(Block) bytes apart in VA space,
    // so the hardware L2 streamer trains K independent trackers — giving
    // AoSoA the same multi-stream memory-level parallelism that SOA gets for
    // free from its N field arrays.
    //
    // Closes most of the float8 / 1M gap vs SOA at K=8 (1.37x → 1.11x on this
    // hardware). Neutral in cache. Intended as opt-in for large-N reductions.
    template<size_t K, class F>
    void for_each_multistream(F&& f) {
        for_each_multistream_impl<K>(std::forward<F>(f),
                                     std::index_sequence_for<Ts...>{});
    }
    template<size_t K, class F>
    void for_each_multistream(F&& f) const {
        for_each_multistream_const_impl<K>(std::forward<F>(f),
                                           std::index_sequence_for<Ts...>{});
    }

    // Apply f(global_index, refs...) to every element. Useful when the body
    // depends on element position.
    template<class F>
    void for_each_indexed(F&& f) {
        for_each_indexed_impl(std::forward<F>(f), std::index_sequence_for<Ts...>{});
    }

    // Apply f(refs...) where refs are only the selected fields.
    // e.g. aosoa.for_each_field<0, 2>([](auto& x, auto& z){ ... });
    template<size_t... Sel, class F>
    void for_each_field(F&& f) {
        static_assert(sizeof...(Sel) > 0, "for_each_field needs at least one field");
        for_each_field_impl<Sel...>(std::forward<F>(f));
    }

    // Reduce: lambda takes (accumulator, refs...) and returns new accumulator.
    template<class Acc, class F>
    Acc reduce(Acc init, F&& f) const {
        return reduce_impl(std::move(init), std::forward<F>(f),
                           std::index_sequence_for<Ts...>{});
    }

    // Filter: returns a new AoSoA of the same shape containing elements
    // where pred(refs...) is true. Two-phase per block — predicate fills
    // a bool mask (vectorizes), then scalar compaction copies survivors.
    template<class Pred>
    AoSoA filter(Pred&& pred) const {
        return filter_impl(std::forward<Pred>(pred),
                           std::index_sequence_for<Ts...>{});
    }

    // ========================================================================
    // Opt-in SIMD fast path for float-only, B=16 AoSoA.
    //
    // These methods bypass the generic lambda-driven for_each and use
    // hand-written AVX2 intrinsics with:
    //   - aligned loads (Block is alignas(64), std::array<float,16> fits)
    //   - one __m256 accumulator per field (breaks the cross-field sum
    //     dep-chain that GCC bakes into a scalar fold)
    //   - software prefetching 8 blocks ahead (4 KiB lookahead, covers the
    //     single-stream MLP shortfall)
    //
    // Empirically on float8 B=16, AVX2, 12 MiB L3:
    //   sum_all_f32_avx2() is 2.3-2.8x FASTER than SOA_Read in cache
    //   (L1/L2-resident), because SOA's own scalar fold has the same
    //   cross-field dep chain and the hand-written 8-accumulator version
    //   beats both. At 1M (DRAM-bound) it closes ~29% of the residual gap
    //   vs SOA (1.44x → 1.27x) — the remaining 1.27x is TLB/DRAM-row and
    //   cannot be closed from software.
    //
    // Constrained with `requires` so callers on non-float or B!=16 get a
    // clean compile error. Call directly on specific hot paths; leave the
    // generic for_each for everything else.
    // ========================================================================
    static constexpr bool is_float_only_B16 =
        (B == 16) && (sizeof...(Ts) >= 1) &&
        (std::conjunction_v<std::is_same<Ts, float>...>);

#if AOSOA_HAS_AVX2
    // Sum all fields of all elements with 8 per-field __m256 accumulators.
    float sum_all_f32_avx2() const
        requires (is_float_only_B16)
    {
        const size_t nb = blocks.size();
        if (nb == 0) return 0.0f;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        constexpr size_t N = sizeof...(Ts);
        __m256 acc[N];
        for (size_t f = 0; f < N; ++f) acc[f] = _mm256_setzero_ps();

        constexpr size_t PF_AHEAD = 8;
        for (size_t bi = 0; bi < full; ++bi) {
            if (bi + PF_AHEAD < full) {
                // All 8 cache lines of the next-ahead block. Agent J's static
                // analysis suggested dropping 7 of 8 prefetches based on the
                // assumption that the L1 DCU next-line prefetcher would fill
                // in the rest. Measured: in cache that saves ~5% of load-port
                // pressure (confirmed), but at 1M DRAM-bound it blows the
                // timing from 1.3x to 3.0x vs SOA because only 1 of 8 lines
                // arrives ahead of time. Keeping all 8 for the DRAM win.
                const char* next = reinterpret_cast<const char*>(&blocks[bi + PF_AHEAD]);
                _mm_prefetch(next,        _MM_HINT_T0);
                _mm_prefetch(next + 64,   _MM_HINT_T0);
                _mm_prefetch(next + 128,  _MM_HINT_T0);
                _mm_prefetch(next + 192,  _MM_HINT_T0);
                _mm_prefetch(next + 256,  _MM_HINT_T0);
                _mm_prefetch(next + 320,  _MM_HINT_T0);
                _mm_prefetch(next + 384,  _MM_HINT_T0);
                _mm_prefetch(next + 448,  _MM_HINT_T0);
            }
            const auto& blk = blocks[bi];
            sum_block_avx2_impl<0>(blk, acc);
        }
        __m256 total = _mm256_setzero_ps();
        for (size_t f = 0; f < N; ++f) total = _mm256_add_ps(total, acc[f]);
        float out = hsum256_ps(total);

        if (tail > 0) {
            const auto& blk = blocks[full];
            sum_scalar_tail_impl<0>(blk, tail, out);
        }
        return out;
    }

    // Compute x0*x1 + x2 + x3 + ... + x(N-1), summed over all elements.
    // FMA on the multiplied pair, per-field accumulators on the added rest.
    float compute_all_f32_avx2() const
        requires (is_float_only_B16 && sizeof...(Ts) >= 3)
    {
        const size_t nb = blocks.size();
        if (nb == 0) return 0.0f;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        constexpr size_t N = sizeof...(Ts);
        __m256 acc_mul_a = _mm256_setzero_ps();
        __m256 acc_mul_b = _mm256_setzero_ps();
        __m256 acc_sum[N];
        for (size_t f = 0; f < N; ++f) acc_sum[f] = _mm256_setzero_ps();

        constexpr size_t PF_AHEAD = 8;
        for (size_t bi = 0; bi < full; ++bi) {
            if (bi + PF_AHEAD < full) {
                // All 8 cache lines of the next-ahead block. Agent J's static
                // analysis suggested dropping 7 of 8 prefetches based on the
                // assumption that the L1 DCU next-line prefetcher would fill
                // in the rest. Measured: in cache that saves ~5% of load-port
                // pressure (confirmed), but at 1M DRAM-bound it blows the
                // timing from 1.3x to 3.0x vs SOA because only 1 of 8 lines
                // arrives ahead of time. Keeping all 8 for the DRAM win.
                const char* next = reinterpret_cast<const char*>(&blocks[bi + PF_AHEAD]);
                _mm_prefetch(next,        _MM_HINT_T0);
                _mm_prefetch(next + 64,   _MM_HINT_T0);
                _mm_prefetch(next + 128,  _MM_HINT_T0);
                _mm_prefetch(next + 192,  _MM_HINT_T0);
                _mm_prefetch(next + 256,  _MM_HINT_T0);
                _mm_prefetch(next + 320,  _MM_HINT_T0);
                _mm_prefetch(next + 384,  _MM_HINT_T0);
                _mm_prefetch(next + 448,  _MM_HINT_T0);
            }
            const auto& blk = blocks[bi];
            __m256 x0a, x0b, x1a, x1b;
            load_aligned_pair_impl(std::get<0>(blk.data).data(), x0a, x0b);
            load_aligned_pair_impl(std::get<1>(blk.data).data(), x1a, x1b);
            acc_mul_a = _mm256_fmadd_ps(x0a, x1a, acc_mul_a);
            acc_mul_b = _mm256_fmadd_ps(x0b, x1b, acc_mul_b);
            compute_sum_rest_impl<2>(blk, acc_sum);
        }
        __m256 total = _mm256_add_ps(acc_mul_a, acc_mul_b);
        for (size_t f = 2; f < N; ++f) total = _mm256_add_ps(total, acc_sum[f]);
        float out = hsum256_ps(total);

        if (tail > 0) {
            const auto& blk = blocks[full];
            compute_scalar_tail_impl(blk, tail, out);
        }
        return out;
    }
#endif // AOSOA_HAS_AVX2

    // ========================================================================
    // Escape hatch: raw block iteration.
    //
    // for_each_block(f) calls f(block, n) for each block. n == B on every
    // full block, possibly < B on the last. Use this to hand-write traversals
    // (e.g. split arrays into locals, write an explicit SIMD loop, apply a
    // fused operation across fields) without losing the container abstraction.
    // ========================================================================

    template<class F>
    void for_each_block(F&& f) {
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) f(blocks[bi], B);
        if (tail > 0)                        f(blocks[full], tail);
    }
    template<class F>
    void for_each_block(F&& f) const {
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) f(blocks[bi], B);
        if (tail > 0)                        f(blocks[full], tail);
    }

    // ========================================================================
    // Legacy element-level Iterator/Proxy.
    //
    // Kept so benchmarks can compare the old (broken) iterator to the new
    // for_each path. Do NOT use this in new code — the modulo-B branch in
    // operator++ defeats auto-vectorization. See the blog post for the full
    // story. operator[] is implemented via the same Proxy for ad-hoc access.
    // ========================================================================

    class Proxy {
    public:
        std::tuple<Ts&...> refs;
        template<size_t I> auto&       get()       { return std::get<I>(refs); }
        template<size_t I> const auto& get() const { return std::get<I>(refs); }
    };

    class Iterator {
        AoSoA* owner_;
        size_t block_idx_;
        size_t offset_;
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = Proxy;
        using difference_type   = std::ptrdiff_t;

        Iterator(AoSoA* o, size_t flat)
            : owner_(o), block_idx_(flat / B), offset_(flat % B) {}

        Proxy operator*() {
            return make_proxy(std::index_sequence_for<Ts...>{});
        }

        // NOTE: this branch is why the iterator cannot be vectorized.
        // Prefer for_each in new code.
        Iterator& operator++() {
            if (++offset_ == B) { offset_ = 0; ++block_idx_; }
            return *this;
        }
        Iterator operator++(int) { auto t = *this; ++*this; return t; }

        Iterator& operator+=(difference_type d) {
            const difference_type flat =
                static_cast<difference_type>(block_idx_ * B + offset_) + d;
            block_idx_ = static_cast<size_t>(flat) / B;
            offset_    = static_cast<size_t>(flat) % B;
            return *this;
        }
        Iterator operator+(difference_type d) const { auto t = *this; t += d; return t; }

        bool operator==(const Iterator& o) const {
            return owner_ == o.owner_ && block_idx_ == o.block_idx_ && offset_ == o.offset_;
        }
        bool operator!=(const Iterator& o) const { return !(*this == o); }

    private:
        template<size_t... Is>
        Proxy make_proxy(std::index_sequence<Is...>) {
            return Proxy{{ std::get<Is>(owner_->blocks[block_idx_].data)[offset_]... }};
        }
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end()   { return Iterator(this, size_); }

    Proxy operator[](size_t i) {
        return make_proxy_at(i / B, i % B, std::index_sequence_for<Ts...>{});
    }

private:
#if AOSOA_HAS_AVX2
    // ---- SIMD intrinsic helpers for sum_all_f32_avx2 / compute_all_f32_avx2 ----

    static inline float hsum256_ps(__m256 v) {
        __m128 lo = _mm256_castps256_ps128(v);
        __m128 hi = _mm256_extractf128_ps(v, 1);
        __m128 s  = _mm_add_ps(lo, hi);
        __m128 shuf = _mm_movehdup_ps(s);
        __m128 sums = _mm_add_ps(s, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        return _mm_cvtss_f32(sums);
    }

    static inline void load_aligned_pair_impl(const float* ptr, __m256& a, __m256& b) {
        a = _mm256_load_ps(ptr);
        b = _mm256_load_ps(ptr + 8);
    }

    // Recursive per-field fold: unrolled at compile time for all fields.
    template<size_t F>
    static inline void sum_block_avx2_impl(const BlockT& blk, __m256* acc) {
        if constexpr (F < sizeof...(Ts)) {
            __m256 a, b;
            load_aligned_pair_impl(std::get<F>(blk.data).data(), a, b);
            acc[F] = _mm256_add_ps(acc[F], _mm256_add_ps(a, b));
            sum_block_avx2_impl<F + 1>(blk, acc);
        }
    }

    template<size_t F>
    static inline void compute_sum_rest_impl(const BlockT& blk, __m256* acc_sum) {
        if constexpr (F < sizeof...(Ts)) {
            __m256 a, b;
            load_aligned_pair_impl(std::get<F>(blk.data).data(), a, b);
            acc_sum[F] = _mm256_add_ps(acc_sum[F], _mm256_add_ps(a, b));
            compute_sum_rest_impl<F + 1>(blk, acc_sum);
        }
    }

    template<size_t F>
    static inline void sum_scalar_tail_impl(const BlockT& blk, size_t n, float& out) {
        if constexpr (F < sizeof...(Ts)) {
            const auto& arr = std::get<F>(blk.data);
            for (size_t i = 0; i < n; ++i) out += arr[i];
            sum_scalar_tail_impl<F + 1>(blk, n, out);
        }
    }

    static inline void compute_scalar_tail_impl(const BlockT& blk, size_t n, float& out) {
        constexpr size_t N = sizeof...(Ts);
        for (size_t i = 0; i < n; ++i) {
            float v = std::get<0>(blk.data)[i] * std::get<1>(blk.data)[i];
            [&]<size_t... Js>(std::index_sequence<Js...>) {
                ((v += std::get<Js + 2>(blk.data)[i]), ...);
            }(std::make_index_sequence<N - 2>{});
            out += v;
        }
    }
#endif // AOSOA_HAS_AVX2

    // ---- for_each / reduce / filter internals ----

    template<class F, size_t... Is>
    void for_each_impl(F&& f, std::index_sequence<Is...>) {
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) {
            auto& blk = blocks[bi];
            for (size_t i = 0; i < B; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            auto& blk = blocks[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
    }

    template<class F, size_t... Is>
    void for_each_const_impl(F&& f, std::index_sequence<Is...>) const {
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) {
            const auto& blk = blocks[bi];
            for (size_t i = 0; i < B; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            const auto& blk = blocks[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
    }

    template<size_t UNROLL, class F, size_t... Is>
    void for_each_unrolled_impl(F&& f, std::index_sequence<Is...>) {
        static_assert(UNROLL >= 1, "UNROLL must be >= 1");
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        BlockT* const blk_base = blocks.data();
        size_t bi = 0;
        if constexpr (UNROLL >= 2) {
            for (; bi + UNROLL <= full; bi += UNROLL) {
                BlockT* __restrict__ const b0 = blk_base + bi;
                // The `Us` fold produces UNROLL back-to-back copies of the
                // baseline inner loop. Writing each as a lambda inside the
                // fold keeps GCC from merging them, while still letting it
                // vectorize each copy independently.
                [&]<size_t... Us>(std::index_sequence<Us...>) {
                    ((
                        [&] {
                            auto& blk = b0[Us];
                            for (size_t i = 0; i < B; ++i) {
                                f(std::get<Is>(blk.data)[i]...);
                            }
                        }()
                    ), ...);
                }(std::make_index_sequence<UNROLL>{});
            }
        }
        for (; bi < full; ++bi) {
            auto& blk = blocks[bi];
            for (size_t i = 0; i < B; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            auto& blk = blocks[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
    }

    template<size_t UNROLL, class F, size_t... Is>
    void for_each_unrolled_const_impl(F&& f, std::index_sequence<Is...>) const {
        static_assert(UNROLL >= 1, "UNROLL must be >= 1");
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        const BlockT* const blk_base = blocks.data();
        size_t bi = 0;
        if constexpr (UNROLL >= 2) {
            for (; bi + UNROLL <= full; bi += UNROLL) {
                const BlockT* __restrict__ const b0 = blk_base + bi;
                [&]<size_t... Us>(std::index_sequence<Us...>) {
                    ((
                        [&] {
                            const auto& blk = b0[Us];
                            for (size_t i = 0; i < B; ++i) {
                                f(std::get<Is>(blk.data)[i]...);
                            }
                        }()
                    ), ...);
                }(std::make_index_sequence<UNROLL>{});
            }
        }
        for (; bi < full; ++bi) {
            const auto& blk = blocks[bi];
            for (size_t i = 0; i < B; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            const auto& blk = blocks[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
    }

    // Multi-stream implementation — splits the block vector into K segments
    // and iterates one block from each segment per outer iter. Segments are
    // `full/K` blocks apart in the vector, so the load addresses are
    // (full/K) * sizeof(BlockT) bytes apart in virtual memory — far enough
    // that the L2 streamer must allocate K independent tracker slots.
    //
    // Fallbacks handle (a) K=1 or K>full, (b) leftover blocks past K*seg,
    // (c) partial tail block.
    template<size_t K, class F, size_t... Is>
    void for_each_multistream_impl(F&& f, std::index_sequence<Is...>) {
        static_assert(K >= 1, "K must be >= 1");
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        BlockT* const blk_base = blocks.data();

        if constexpr (K >= 2) {
            const size_t seg = full / K;
            if (seg > 0) {
                for (size_t bi = 0; bi < seg; ++bi) {
                    [&]<size_t... Ks>(std::index_sequence<Ks...>) {
                        ((
                            [&] {
                                auto& blk = blk_base[bi + Ks * seg];
                                for (size_t i = 0; i < B; ++i) {
                                    f(std::get<Is>(blk.data)[i]...);
                                }
                            }()
                        ), ...);
                    }(std::make_index_sequence<K>{});
                }
                for (size_t bi = K * seg; bi < full; ++bi) {
                    auto& blk = blk_base[bi];
                    for (size_t i = 0; i < B; ++i) {
                        f(std::get<Is>(blk.data)[i]...);
                    }
                }
            } else {
                for (size_t bi = 0; bi < full; ++bi) {
                    auto& blk = blk_base[bi];
                    for (size_t i = 0; i < B; ++i) {
                        f(std::get<Is>(blk.data)[i]...);
                    }
                }
            }
        } else {
            for (size_t bi = 0; bi < full; ++bi) {
                auto& blk = blk_base[bi];
                for (size_t i = 0; i < B; ++i) {
                    f(std::get<Is>(blk.data)[i]...);
                }
            }
        }

        if (tail > 0) {
            auto& blk = blk_base[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
    }

    template<size_t K, class F, size_t... Is>
    void for_each_multistream_const_impl(F&& f, std::index_sequence<Is...>) const {
        static_assert(K >= 1, "K must be >= 1");
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        const BlockT* const blk_base = blocks.data();

        if constexpr (K >= 2) {
            const size_t seg = full / K;
            if (seg > 0) {
                for (size_t bi = 0; bi < seg; ++bi) {
                    [&]<size_t... Ks>(std::index_sequence<Ks...>) {
                        ((
                            [&] {
                                const auto& blk = blk_base[bi + Ks * seg];
                                for (size_t i = 0; i < B; ++i) {
                                    f(std::get<Is>(blk.data)[i]...);
                                }
                            }()
                        ), ...);
                    }(std::make_index_sequence<K>{});
                }
                for (size_t bi = K * seg; bi < full; ++bi) {
                    const auto& blk = blk_base[bi];
                    for (size_t i = 0; i < B; ++i) {
                        f(std::get<Is>(blk.data)[i]...);
                    }
                }
            } else {
                for (size_t bi = 0; bi < full; ++bi) {
                    const auto& blk = blk_base[bi];
                    for (size_t i = 0; i < B; ++i) {
                        f(std::get<Is>(blk.data)[i]...);
                    }
                }
            }
        } else {
            for (size_t bi = 0; bi < full; ++bi) {
                const auto& blk = blk_base[bi];
                for (size_t i = 0; i < B; ++i) {
                    f(std::get<Is>(blk.data)[i]...);
                }
            }
        }

        if (tail > 0) {
            const auto& blk = blk_base[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Is>(blk.data)[i]...);
            }
        }
    }

    template<class F, size_t... Is>
    void for_each_indexed_impl(F&& f, std::index_sequence<Is...>) {
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) {
            auto& blk = blocks[bi];
            const size_t base = bi * B;
            for (size_t i = 0; i < B; ++i) {
                f(base + i, std::get<Is>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            auto& blk = blocks[full];
            const size_t base = full * B;
            for (size_t i = 0; i < tail; ++i) {
                f(base + i, std::get<Is>(blk.data)[i]...);
            }
        }
    }

    template<size_t... Sel, class F>
    void for_each_field_impl(F&& f) {
        const size_t nb = blocks.size();
        if (nb == 0) return;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) {
            auto& blk = blocks[bi];
            for (size_t i = 0; i < B; ++i) {
                f(std::get<Sel>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            auto& blk = blocks[full];
            for (size_t i = 0; i < tail; ++i) {
                f(std::get<Sel>(blk.data)[i]...);
            }
        }
    }

    template<class Acc, class F, size_t... Is>
    Acc reduce_impl(Acc acc, F&& f, std::index_sequence<Is...>) const {
        const size_t nb = blocks.size();
        if (nb == 0) return acc;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;
        for (size_t bi = 0; bi < full; ++bi) {
            const auto& blk = blocks[bi];
            for (size_t i = 0; i < B; ++i) {
                acc = f(acc, std::get<Is>(blk.data)[i]...);
            }
        }
        if (tail > 0) {
            const auto& blk = blocks[full];
            for (size_t i = 0; i < tail; ++i) {
                acc = f(acc, std::get<Is>(blk.data)[i]...);
            }
        }
        return acc;
    }

    template<class Pred, size_t... Is>
    AoSoA filter_impl(Pred pred, std::index_sequence<Is...>) const {
        AoSoA out;
        out.reserve(size_);
        const size_t nb = blocks.size();
        if (nb == 0) return out;
        const size_t tail = size_ % B;
        const size_t full = (tail == 0) ? nb : nb - 1;

        auto run = [&](const BlockT& blk, size_t n) {
            bool mask[B];
            for (size_t i = 0; i < n; ++i) {
                mask[i] = pred(std::get<Is>(blk.data)[i]...);
            }
            for (size_t i = 0; i < n; ++i) {
                if (mask[i]) {
                    out.push_back(std::get<Is>(blk.data)[i]...);
                }
            }
        };
        for (size_t bi = 0; bi < full; ++bi) run(blocks[bi], B);
        if (tail > 0)                        run(blocks[full], tail);
        return out;
    }

    template<size_t... Is>
    Proxy make_proxy_at(size_t bi, size_t off, std::index_sequence<Is...>) {
        return Proxy{{ std::get<Is>(blocks[bi].data)[off]... }};
    }

    template<typename Tuple, size_t... Is>
    static void write_at(BlockT& b, size_t off, Tuple&& t, std::index_sequence<Is...>) {
        ((std::get<Is>(b.data)[off] = std::get<Is>(t)), ...);
    }
};

// Recommended default block size.
//
// Empirical sweep across B ∈ {2, 4, 8, 16, 32, 64, 128}, 4 type configurations
// (float3, float8, double3, int+float+double), 5 sizes (1K to 1M), 4 ops
// (Read, Write, Compute, FilterCopy) on Intel Core Ultra 7 (12 MiB L3, AVX2).
//
// B=16 minimizes geometric-mean slowdown across all 80 workloads (1.097x vs
// oracle best-B), has the lowest worst-case (1.79x) of any single default,
// and wins 30/80 cells outright — more than any other B. Detailed study:
// see the SOA vs AoSoA blog post.
template<typename... Ts>
using AoSoAd = AoSoA<16, Ts...>;
