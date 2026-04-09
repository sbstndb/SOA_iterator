#pragma once
#include <array>
#include <tuple>
#include <vector>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <iterator>

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
