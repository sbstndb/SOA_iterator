#pragma once
#include <array>
#include <tuple>
#include <vector>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <iterator>

// Block: one SOA chunk of fixed capacity B, stored inline
template<size_t B, typename... Ts>
struct alignas(64) Block {
    std::tuple<std::array<Ts, B>...> data;
    template<size_t I> auto& array()       { return std::get<I>(data); }
    template<size_t I> const auto& array() const { return std::get<I>(data); }
};

// AoSoA: array of SOA blocks
template<size_t B, typename... Ts>
class AoSoA {
    static_assert(B > 0, "Block size must be positive");
public:
    using BlockT = Block<B, Ts...>;
    std::vector<BlockT> blocks;  // public like SOA::arrays
    size_t size_ = 0;

    static constexpr size_t block_size() { return B; }
    static constexpr size_t field_count() { return sizeof...(Ts); }

    AoSoA() = default;
    explicit AoSoA(size_t n) { resize(n); }

    size_t size() const { return size_; }
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

    // Proxy: same shape as SOA::Proxy so generic ops work unchanged
    class Proxy {
    public:
        std::tuple<Ts&...> refs;
        template<size_t I> auto& get()             { return std::get<I>(refs); }
        template<size_t I> const auto& get() const { return std::get<I>(refs); }
    };

    // Iterator: tracks (block_idx, offset), random-access
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
    template<size_t... Is>
    Proxy make_proxy_at(size_t bi, size_t off, std::index_sequence<Is...>) {
        return Proxy{{ std::get<Is>(blocks[bi].data)[off]... }};
    }

    template<typename Tuple, size_t... Is>
    static void write_at(BlockT& b, size_t off, Tuple&& t, std::index_sequence<Is...>) {
        ((std::get<Is>(b.data)[off] = std::get<Is>(t)), ...);
    }
};
