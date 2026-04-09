#include <benchmark/benchmark.h>
#include <vector>
#include <tuple>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "aosoa.hpp"

// ============================================================================
// Type utilities
// ============================================================================

template<typename... Ts>
using common_t = std::common_type_t<Ts...>;

// ============================================================================
// AOS: Array of Structures (generic)
// ============================================================================

template<typename... Ts>
struct AOS {
    std::tuple<Ts...> data;

    AOS() = default;

    template<typename... Args>
    explicit AOS(Args&&... args) : data(std::forward<Args>(args)...) {}

    template<size_t I>
    auto& get() { return std::get<I>(data); }

    template<size_t I>
    const auto& get() const { return std::get<I>(data); }

    static constexpr size_t field_count() { return sizeof...(Ts); }
};

// ============================================================================
// SOA: Structure of Arrays (generic)
// ============================================================================

template<typename... Ts>
class SOA {
public:
    std::tuple<std::vector<Ts>...> arrays;

    explicit SOA(size_t n = 0) {
        resize(n);
    }

    void resize(size_t n) {
        resize_impl(n, std::index_sequence_for<Ts...>{});
    }

    size_t size() const {
        return std::get<0>(arrays).size();
    }

    template<size_t I>
    auto& array() { return std::get<I>(arrays); }

    template<size_t I>
    const auto& array() const { return std::get<I>(arrays); }

    static constexpr size_t field_count() { return sizeof...(Ts); }

    // ========================================================================
    // Proxy: provides AOS-like access to SOA elements
    // ========================================================================
    class Proxy {
    public:
        std::tuple<Ts&...> refs;

        template<size_t I>
        auto& get() { return std::get<I>(refs); }

        template<size_t I>
        const auto& get() const { return std::get<I>(refs); }
    };

    // ========================================================================
    // Iterator: iterates over SOA returning Proxy objects
    // ========================================================================
    class Iterator {
        SOA* soa_;
        size_t index_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Proxy;
        using difference_type = std::ptrdiff_t;

        Iterator(SOA* soa, size_t index) : soa_(soa), index_(index) {}

        Proxy operator*() {
            return make_proxy(std::index_sequence_for<Ts...>{});
        }

        Iterator& operator++() {
            ++index_;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return index_ == other.index_ && soa_ == other.soa_;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        template<size_t... Is>
        Proxy make_proxy(std::index_sequence<Is...>) {
            return Proxy{{std::get<Is>(soa_->arrays)[index_]...}};
        }
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end() { return Iterator(this, size()); }

    // Reserve capacity for all arrays
    void reserve(size_t n) {
        reserve_impl(n, std::index_sequence_for<Ts...>{});
    }

    // Push back an element to all arrays
    template<typename... Args>
    void push_back(Args&&... args) {
        push_back_impl(std::forward_as_tuple(args...), std::index_sequence_for<Ts...>{});
    }

private:
    template<size_t... Is>
    void resize_impl(size_t n, std::index_sequence<Is...>) {
        ((std::get<Is>(arrays).resize(n)), ...);
    }

    template<size_t... Is>
    void reserve_impl(size_t n, std::index_sequence<Is...>) {
        ((std::get<Is>(arrays).reserve(n)), ...);
    }

    template<typename Tuple, size_t... Is>
    void push_back_impl(Tuple&& t, std::index_sequence<Is...>) {
        ((std::get<Is>(arrays).push_back(std::get<Is>(t))), ...);
    }
};

// ============================================================================
// Generic operations
// ============================================================================

// Sum from a specific index onwards
template<size_t Offset, typename Tuple, size_t... Is>
auto sum_from_index(const Tuple& t, std::index_sequence<Is...>) {
    return (std::get<Offset + Is>(t) + ...);
}

// Sum all fields in a tuple
template<typename Tuple>
auto sum_all_fields(const Tuple& t) {
    return std::apply([](const auto&... args) {
        return (args + ...);
    }, t);
}

// Compute: get<0> * get<1> + sum(rest) if >= 3 fields
template<typename Tuple>
auto compute_fields_impl(const Tuple& t) {
    constexpr size_t N = std::tuple_size_v<std::decay_t<Tuple>>;

    if constexpr (N == 1) {
        return std::get<0>(t);
    } else if constexpr (N == 2) {
        return std::get<0>(t) * std::get<1>(t);
    } else {
        return std::get<0>(t) * std::get<1>(t) +
               sum_from_index<2>(t, std::make_index_sequence<N - 2>{});
    }
}

// Increment all fields by (index + 1)
template<typename Tuple, size_t... Is>
void increment_all(Tuple& t, std::index_sequence<Is...>) {
    ((std::get<Is>(t) += (Is + 1)), ...);
}

// Conditional transform: if get<0> > get<1> then get<2>*2 else get<2>+1
// For 2 fields: if get<0> > get<1> then get<0>*2 else get<1>+1
template<typename Tuple>
auto conditional_transform_impl(const Tuple& t) {
    constexpr size_t N = std::tuple_size_v<std::decay_t<Tuple>>;

    if constexpr (N >= 3) {
        if (std::get<0>(t) > std::get<1>(t)) {
            return std::get<2>(t) * 2;
        } else {
            return std::get<2>(t) + 1;
        }
    } else if constexpr (N == 2) {
        if (std::get<0>(t) > std::get<1>(t)) {
            return std::get<0>(t) * 2;
        } else {
            return std::get<1>(t) + 1;
        }
    } else {
        return std::get<0>(t);
    }
}

// ============================================================================
// Initialization
// ============================================================================

template<typename... Ts, size_t... Is>
void init_element_impl(AOS<Ts...>& elem, SOA<Ts...>& soa, size_t i, std::index_sequence<Is...>) {
    using TupleType = std::tuple<Ts...>;
    ((std::get<Is>(elem.data) = static_cast<std::tuple_element_t<Is, TupleType>>(i + Is)), ...);
    ((std::get<Is>(soa.arrays)[i] = static_cast<std::tuple_element_t<Is, TupleType>>(i + Is)), ...);
}

template<typename... Ts>
void initialize_data(std::vector<AOS<Ts...>>& aos, SOA<Ts...>& soa, size_t n) {
    aos.resize(n);
    soa.resize(n);

    for (size_t i = 0; i < n; ++i) {
        init_element_impl(aos[i], soa, i, std::index_sequence_for<Ts...>{});
    }
}

// Initialize sorted data for merge benchmarks
template<typename... Ts, size_t... Is>
void init_sorted_element_impl(AOS<Ts...>& elem, SOA<Ts...>& soa, size_t i, int offset, std::index_sequence<Is...>) {
    using TupleType = std::tuple<Ts...>;
    // First field is sorted (i * 2 + offset), rest are i + Is
    std::get<0>(elem.data) = static_cast<std::tuple_element_t<0, TupleType>>(i * 2 + offset);
    std::get<0>(soa.arrays)[i] = static_cast<std::tuple_element_t<0, TupleType>>(i * 2 + offset);

    if constexpr (sizeof...(Ts) > 1) {
        auto init_rest = [&]<size_t... Js>(std::index_sequence<Js...>) {
            ((std::get<Js + 1>(elem.data) = static_cast<std::tuple_element_t<Js + 1, TupleType>>(i + Js + 1)), ...);
            ((std::get<Js + 1>(soa.arrays)[i] = static_cast<std::tuple_element_t<Js + 1, TupleType>>(i + Js + 1)), ...);
        };
        init_rest(std::make_index_sequence<sizeof...(Ts) - 1>{});
    }
}

template<typename... Ts>
void initialize_sorted_data(std::vector<AOS<Ts...>>& aos, SOA<Ts...>& soa, size_t n, int offset = 0) {
    aos.resize(n);
    soa.resize(n);

    for (size_t i = 0; i < n; ++i) {
        init_sorted_element_impl(aos[i], soa, i, offset, std::index_sequence_for<Ts...>{});
    }
}

// ============================================================================
// Benchmarks: Read
// ============================================================================

template<typename... Ts>
static void BM_AOS_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t sum = 0;
        for (const auto& elem : aos) {
            sum += sum_all_fields(elem.data);
        }
        benchmark::DoNotOptimize(sum);
    }
}

template<typename... Ts>
static void BM_AOS_raw_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t sum = 0;
        for (size_t i = 0; i < size; ++i) {
            sum += sum_all_fields(aos[i].data);
        }
        benchmark::DoNotOptimize(sum);
    }
}

template<typename... Ts>
static void BM_SOA_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t sum = 0;
        for (auto proxy : soa) {
            sum += sum_all_fields(proxy.refs);
        }
        benchmark::DoNotOptimize(sum);
    }
}

template<typename... Ts, size_t... Is>
auto sum_at_index(const SOA<Ts...>& soa, size_t i, std::index_sequence<Is...>) {
    return (std::get<Is>(soa.arrays)[i] + ...);
}

template<typename... Ts>
static void BM_SOA_raw_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t sum = 0;
        for (size_t i = 0; i < size; ++i) {
            sum += sum_at_index(soa, i, std::index_sequence_for<Ts...>{});
        }
        benchmark::DoNotOptimize(sum);
    }
}

// ============================================================================
// Benchmarks: Write
// ============================================================================

template<typename... Ts>
static void BM_AOS_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        for (auto& elem : aos) {
            increment_all(elem.data, std::index_sequence_for<Ts...>{});
        }
        benchmark::ClobberMemory();
    }
}

template<typename... Ts>
static void BM_AOS_raw_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        for (size_t i = 0; i < size; ++i) {
            increment_all(aos[i].data, std::index_sequence_for<Ts...>{});
        }
        benchmark::ClobberMemory();
    }
}

template<typename... Ts>
static void BM_SOA_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        for (auto proxy : soa) {
            increment_all(proxy.refs, std::index_sequence_for<Ts...>{});
        }
        benchmark::ClobberMemory();
    }
}

template<typename... Ts, size_t... Is>
void increment_at_index(SOA<Ts...>& soa, size_t i, std::index_sequence<Is...>) {
    ((std::get<Is>(soa.arrays)[i] += (Is + 1)), ...);
}

template<typename... Ts>
static void BM_SOA_raw_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        for (size_t i = 0; i < size; ++i) {
            increment_at_index(soa, i, std::index_sequence_for<Ts...>{});
        }
        benchmark::ClobberMemory();
    }
}

// ============================================================================
// Benchmarks: Compute
// ============================================================================

template<typename... Ts>
static void BM_AOS_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t result = 0;
        for (const auto& elem : aos) {
            result += compute_fields_impl(elem.data);
        }
        benchmark::DoNotOptimize(result);
    }
}

template<typename... Ts>
static void BM_AOS_raw_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t result = 0;
        for (size_t i = 0; i < size; ++i) {
            result += compute_fields_impl(aos[i].data);
        }
        benchmark::DoNotOptimize(result);
    }
}

template<typename... Ts>
static void BM_SOA_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t result = 0;
        for (auto proxy : soa) {
            result += compute_fields_impl(proxy.refs);
        }
        benchmark::DoNotOptimize(result);
    }
}

template<typename... Ts, size_t... Is>
auto compute_at_index_impl(const SOA<Ts...>& soa, size_t i, std::index_sequence<Is...>) {
    constexpr size_t N = sizeof...(Ts);
    if constexpr (N == 1) {
        return std::get<0>(soa.arrays)[i];
    } else if constexpr (N == 2) {
        return std::get<0>(soa.arrays)[i] * std::get<1>(soa.arrays)[i];
    } else {
        auto sum_rest = [&]<size_t... Js>(std::index_sequence<Js...>) {
            return (std::get<Js + 2>(soa.arrays)[i] + ...);
        };
        return std::get<0>(soa.arrays)[i] * std::get<1>(soa.arrays)[i] +
               sum_rest(std::make_index_sequence<N - 2>{});
    }
}

template<typename... Ts>
static void BM_SOA_raw_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t result = 0;
        for (size_t i = 0; i < size; ++i) {
            result += compute_at_index_impl(soa, i, std::index_sequence_for<Ts...>{});
        }
        benchmark::DoNotOptimize(result);
    }
}

// ============================================================================
// Benchmarks: ComputeVector (store results in pre-allocated vector)
// ============================================================================

template<typename... Ts>
static void BM_AOS_ComputeVector(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;
    std::vector<result_t> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (const auto& elem : aos) {
            results[i++] = compute_fields_impl(elem.data);
        }
        benchmark::DoNotOptimize(results.data());
    }
}

template<typename... Ts>
static void BM_SOA_ComputeVector(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;
    std::vector<result_t> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (auto proxy : soa) {
            results[i++] = compute_fields_impl(proxy.refs);
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// ============================================================================
// Benchmarks: ComputePushBack (store results with push_back)
// ============================================================================

template<typename... Ts>
static void BM_AOS_ComputePushBack(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        std::vector<result_t> results;
        for (const auto& elem : aos) {
            results.push_back(compute_fields_impl(elem.data));
        }
        benchmark::DoNotOptimize(results.data());
    }
}

template<typename... Ts>
static void BM_SOA_ComputePushBack(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        std::vector<result_t> results;
        for (auto proxy : soa) {
            results.push_back(compute_fields_impl(proxy.refs));
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// ============================================================================
// Benchmarks: ConditionalTransform
// ============================================================================

template<typename... Ts>
static void BM_AOS_ConditionalTransform(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;
    std::vector<result_t> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (const auto& elem : aos) {
            results[i++] = conditional_transform_impl(elem.data);
        }
        benchmark::DoNotOptimize(results.data());
    }
}

template<typename... Ts>
static void BM_SOA_ConditionalTransform(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using result_t = common_t<Ts...>;
    std::vector<result_t> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (auto proxy : soa) {
            results[i++] = conditional_transform_impl(proxy.refs);
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// ============================================================================
// Benchmarks: LinearSearch (parameterized on field index)
// ============================================================================

template<size_t FieldIndex, typename... Ts>
static void BM_AOS_LinearSearch(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using FieldType = std::tuple_element_t<FieldIndex, std::tuple<Ts...>>;
    const FieldType target = static_cast<FieldType>(size / 2 + FieldIndex);

    for (auto _ : state) {
        size_t found_index = size;
        for (size_t i = 0; i < aos.size(); ++i) {
            if (std::get<FieldIndex>(aos[i].data) == target) {
                found_index = i;
                break;
            }
        }
        benchmark::DoNotOptimize(found_index);
    }
}

template<size_t FieldIndex, typename... Ts>
static void BM_SOA_LinearSearch(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    using FieldType = std::tuple_element_t<FieldIndex, std::tuple<Ts...>>;
    const FieldType target = static_cast<FieldType>(size / 2 + FieldIndex);

    for (auto _ : state) {
        size_t found_index = size;
        const auto& search_array = std::get<FieldIndex>(soa.arrays);
        for (size_t i = 0; i < size; ++i) {
            if (search_array[i] == target) {
                found_index = i;
                break;
            }
        }
        benchmark::DoNotOptimize(found_index);
    }
}

// ============================================================================
// Benchmarks: FilterCopy
// ============================================================================

template<typename... Ts>
static void BM_AOS_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        std::vector<AOS<Ts...>> filtered;
        filtered.reserve(size / 2);
        for (const auto& elem : aos) {
            // Filter: first field < second field
            if constexpr (sizeof...(Ts) >= 2) {
                if (std::get<0>(elem.data) < std::get<1>(elem.data)) {
                    filtered.push_back(elem);
                }
            } else {
                if (std::get<0>(elem.data) > 0) {
                    filtered.push_back(elem);
                }
            }
        }
        benchmark::DoNotOptimize(filtered.data());
    }
}

template<typename... Ts>
static void BM_AOS_raw_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        std::vector<AOS<Ts...>> filtered;
        filtered.reserve(size / 2);
        for (size_t i = 0; i < size; ++i) {
            if constexpr (sizeof...(Ts) >= 2) {
                if (std::get<0>(aos[i].data) < std::get<1>(aos[i].data)) {
                    filtered.push_back(aos[i]);
                }
            } else {
                if (std::get<0>(aos[i].data) > 0) {
                    filtered.push_back(aos[i]);
                }
            }
        }
        benchmark::DoNotOptimize(filtered.data());
    }
}

template<typename... Ts, size_t... Is>
void push_back_from_proxy(SOA<Ts...>& dest, const typename SOA<Ts...>::Proxy& proxy, std::index_sequence<Is...>) {
    ((std::get<Is>(dest.arrays).push_back(std::get<Is>(proxy.refs))), ...);
}

template<typename... Ts>
static void BM_SOA_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        SOA<Ts...> filtered(0);
        filtered.reserve(size / 2);
        for (auto proxy : soa) {
            if constexpr (sizeof...(Ts) >= 2) {
                if (std::get<0>(proxy.refs) < std::get<1>(proxy.refs)) {
                    push_back_from_proxy(filtered, proxy, std::index_sequence_for<Ts...>{});
                }
            } else {
                if (std::get<0>(proxy.refs) > 0) {
                    push_back_from_proxy(filtered, proxy, std::index_sequence_for<Ts...>{});
                }
            }
        }
        benchmark::DoNotOptimize(std::get<0>(filtered.arrays).data());
    }
}

template<typename... Ts, size_t... Is>
void push_back_at_index(SOA<Ts...>& dest, const SOA<Ts...>& src, size_t i, std::index_sequence<Is...>) {
    ((std::get<Is>(dest.arrays).push_back(std::get<Is>(src.arrays)[i])), ...);
}

template<typename... Ts>
static void BM_SOA_raw_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos;
    SOA<Ts...> soa;
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        SOA<Ts...> filtered(0);
        filtered.reserve(size / 2);
        for (size_t i = 0; i < size; ++i) {
            if constexpr (sizeof...(Ts) >= 2) {
                if (std::get<0>(soa.arrays)[i] < std::get<1>(soa.arrays)[i]) {
                    push_back_at_index(filtered, soa, i, std::index_sequence_for<Ts...>{});
                }
            } else {
                if (std::get<0>(soa.arrays)[i] > 0) {
                    push_back_at_index(filtered, soa, i, std::index_sequence_for<Ts...>{});
                }
            }
        }
        benchmark::DoNotOptimize(std::get<0>(filtered.arrays).data());
    }
}

// ============================================================================
// Benchmarks: Merge
// ============================================================================

template<typename... Ts>
static void BM_AOS_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos1, aos2;
    SOA<Ts...> dummy1(0), dummy2(0);
    initialize_sorted_data(aos1, dummy1, size, 0);  // 0, 2, 4, ...
    initialize_sorted_data(aos2, dummy2, size, 1);  // 1, 3, 5, ...

    for (auto _ : state) {
        std::vector<AOS<Ts...>> merged;
        merged.reserve(2 * size);
        size_t i = 0, j = 0;
        while (i < aos1.size() && j < aos2.size()) {
            if (std::get<0>(aos1[i].data) <= std::get<0>(aos2[j].data)) {
                merged.push_back(aos1[i++]);
            } else {
                merged.push_back(aos2[j++]);
            }
        }
        while (i < aos1.size()) {
            merged.push_back(aos1[i++]);
        }
        while (j < aos2.size()) {
            merged.push_back(aos2[j++]);
        }
        benchmark::DoNotOptimize(merged.data());
    }
}

template<typename... Ts>
static void BM_AOS_nopushback_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> aos1, aos2;
    SOA<Ts...> dummy1(0), dummy2(0);
    initialize_sorted_data(aos1, dummy1, size, 0);
    initialize_sorted_data(aos2, dummy2, size, 1);

    for (auto _ : state) {
        std::vector<AOS<Ts...>> merged;
        const size_t total_size = aos1.size() + aos2.size();
        merged.resize(total_size);

        AOS<Ts...>* dest = merged.data();
        const AOS<Ts...>* src1 = aos1.data();
        const AOS<Ts...>* src2 = aos2.data();

        size_t i = 0, j = 0;
        const size_t size1 = aos1.size();
        const size_t size2 = aos2.size();

        while (i < size1 && j < size2) {
            if (std::get<0>(src1[i].data) <= std::get<0>(src2[j].data)) {
                *dest++ = src1[i++];
            } else {
                *dest++ = src2[j++];
            }
        }

        if (i < size1) {
            std::memcpy(dest, src1 + i, (size1 - i) * sizeof(AOS<Ts...>));
        } else if (j < size2) {
            std::memcpy(dest, src2 + j, (size2 - j) * sizeof(AOS<Ts...>));
        }

        benchmark::DoNotOptimize(merged.data());
    }
}

template<typename... Ts, size_t... Is>
void push_back_soa_element(SOA<Ts...>& dest, const SOA<Ts...>& src, size_t i, std::index_sequence<Is...>) {
    ((std::get<Is>(dest.arrays).push_back(std::get<Is>(src.arrays)[i])), ...);
}

template<typename... Ts>
static void BM_SOA_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> dummy1, dummy2;
    SOA<Ts...> soa1(0), soa2(0);
    initialize_sorted_data(dummy1, soa1, size, 0);
    initialize_sorted_data(dummy2, soa2, size, 1);

    for (auto _ : state) {
        SOA<Ts...> merged(0);
        merged.reserve(2 * size);
        size_t i = 0, j = 0;
        while (i < soa1.size() && j < soa2.size()) {
            if (std::get<0>(soa1.arrays)[i] <= std::get<0>(soa2.arrays)[j]) {
                push_back_soa_element(merged, soa1, i++, std::index_sequence_for<Ts...>{});
            } else {
                push_back_soa_element(merged, soa2, j++, std::index_sequence_for<Ts...>{});
            }
        }
        while (i < soa1.size()) {
            push_back_soa_element(merged, soa1, i++, std::index_sequence_for<Ts...>{});
        }
        while (j < soa2.size()) {
            push_back_soa_element(merged, soa2, j++, std::index_sequence_for<Ts...>{});
        }
        benchmark::DoNotOptimize(std::get<0>(merged.arrays).data());
    }
}

template<typename... Ts, size_t... Is>
void copy_soa_element(SOA<Ts...>& dest, size_t dest_idx, const SOA<Ts...>& src, size_t src_idx, std::index_sequence<Is...>) {
    ((std::get<Is>(dest.arrays)[dest_idx] = std::get<Is>(src.arrays)[src_idx]), ...);
}

template<typename... Ts, size_t... Is>
void memcpy_soa_range(SOA<Ts...>& dest, size_t dest_start, const SOA<Ts...>& src, size_t src_start, size_t count, std::index_sequence<Is...>) {
    ((std::memcpy(std::get<Is>(dest.arrays).data() + dest_start,
                  std::get<Is>(src.arrays).data() + src_start,
                  count * sizeof(std::tuple_element_t<Is, std::tuple<Ts...>>))), ...);
}

template<typename... Ts>
static void BM_SOA_nopushback_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS<Ts...>> dummy1, dummy2;
    SOA<Ts...> soa1(0), soa2(0);
    initialize_sorted_data(dummy1, soa1, size, 0);
    initialize_sorted_data(dummy2, soa2, size, 1);

    for (auto _ : state) {
        SOA<Ts...> merged(0);
        const size_t total_size = soa1.size() + soa2.size();
        merged.resize(total_size);

        size_t i = 0, j = 0, k = 0;
        const size_t size1 = soa1.size();
        const size_t size2 = soa2.size();

        while (i < size1 && j < size2) {
            if (std::get<0>(soa1.arrays)[i] <= std::get<0>(soa2.arrays)[j]) {
                copy_soa_element(merged, k++, soa1, i++, std::index_sequence_for<Ts...>{});
            } else {
                copy_soa_element(merged, k++, soa2, j++, std::index_sequence_for<Ts...>{});
            }
        }

        if (i < size1) {
            memcpy_soa_range(merged, k, soa1, i, size1 - i, std::index_sequence_for<Ts...>{});
        } else if (j < size2) {
            memcpy_soa_range(merged, k, soa2, j, size2 - j, std::index_sequence_for<Ts...>{});
        }

        benchmark::DoNotOptimize(std::get<0>(merged.arrays).data());
    }
}

// ============================================================================
// AoSoA initialization helpers
// ============================================================================

template<size_t B, typename... Ts, size_t... Is>
void init_aosoa_element_impl(AoSoA<B, Ts...>& aosoa, size_t i, std::index_sequence<Is...>) {
    using TupleType = std::tuple<Ts...>;
    auto proxy = aosoa[i];
    ((std::get<Is>(proxy.refs) = static_cast<std::tuple_element_t<Is, TupleType>>(i + Is)), ...);
}

template<size_t B, typename... Ts>
void initialize_aosoa(AoSoA<B, Ts...>& aosoa, size_t n) {
    aosoa.resize(n);
    for (size_t i = 0; i < n; ++i) {
        init_aosoa_element_impl(aosoa, i, std::index_sequence_for<Ts...>{});
    }
}

template<size_t B, typename... Ts, size_t... Is>
void init_sorted_aosoa_element_impl(AoSoA<B, Ts...>& aosoa, size_t i, int offset, std::index_sequence<Is...>) {
    using TupleType = std::tuple<Ts...>;
    auto proxy = aosoa[i];
    // First field is sorted (i * 2 + offset), rest are i + Js + 1
    std::get<0>(proxy.refs) = static_cast<std::tuple_element_t<0, TupleType>>(i * 2 + offset);
    if constexpr (sizeof...(Ts) > 1) {
        auto init_rest = [&]<size_t... Js>(std::index_sequence<Js...>) {
            ((std::get<Js + 1>(proxy.refs) = static_cast<std::tuple_element_t<Js + 1, TupleType>>(i + Js + 1)), ...);
        };
        init_rest(std::make_index_sequence<sizeof...(Ts) - 1>{});
    }
    (void)sizeof...(Is); // silence unused
}

template<size_t B, typename... Ts>
void initialize_sorted_aosoa(AoSoA<B, Ts...>& aosoa, size_t n, int offset = 0) {
    aosoa.resize(n);
    for (size_t i = 0; i < n; ++i) {
        init_sorted_aosoa_element_impl(aosoa, i, offset, std::index_sequence_for<Ts...>{});
    }
}

// ============================================================================
// Benchmarks: AoSoA
// ============================================================================

template<size_t B, typename... Ts>
static void BM_AoSoA_Read(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t sum = 0;
        for (auto proxy : aosoa) {
            sum += sum_all_fields(proxy.refs);
        }
        benchmark::DoNotOptimize(sum);
    }
}

// ---- DIAGNOSIS: direct block-wise loop, no Iterator, no Proxy ----
// If this matches SOA, the container is sane and the iterator abstraction
// alone is the bottleneck. If it matches the iterator version, the layout
// itself has problems.
template<size_t B, typename... Ts>
static void BM_AoSoA_direct_Read(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using result_t = common_t<Ts...>;
    const size_t nblocks = aosoa.blocks.size();
    const size_t tail = size % B;
    const size_t full = (tail == 0) ? nblocks : nblocks - 1;

    for (auto _ : state) {
        result_t sum = 0;
        for (size_t bi = 0; bi < full; ++bi) {
            auto& blk = aosoa.blocks[bi];
            // Inline unrolled: B iterations over N scalar fields.
            // B is a compile-time constant, GCC should unroll + SIMD this.
            [&]<size_t... Is>(std::index_sequence<Is...>) {
                for (size_t i = 0; i < B; ++i) {
                    sum += ((std::get<Is>(blk.data)[i]) + ...);
                }
            }(std::index_sequence_for<Ts...>{});
        }
        if (tail > 0) {
            auto& blk = aosoa.blocks[full];
            [&]<size_t... Is>(std::index_sequence<Is...>) {
                for (size_t i = 0; i < tail; ++i) {
                    sum += ((std::get<Is>(blk.data)[i]) + ...);
                }
            }(std::index_sequence_for<Ts...>{});
        }
        benchmark::DoNotOptimize(sum);
    }
}

template<size_t B, typename... Ts>
static void BM_AoSoA_Write(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    for (auto _ : state) {
        for (auto proxy : aosoa) {
            increment_all(proxy.refs, std::index_sequence_for<Ts...>{});
        }
        benchmark::ClobberMemory();
    }
}

template<size_t B, typename... Ts>
static void BM_AoSoA_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t result = 0;
        for (auto proxy : aosoa) {
            result += compute_fields_impl(proxy.refs);
        }
        benchmark::DoNotOptimize(result);
    }
}

template<size_t B, typename... Ts, size_t... Is>
void push_back_from_aosoa_proxy(AoSoA<B, Ts...>& dest,
                                 const typename AoSoA<B, Ts...>::Proxy& proxy,
                                 std::index_sequence<Is...>) {
    dest.push_back(std::get<Is>(proxy.refs)...);
}

template<size_t B, typename... Ts>
static void BM_AoSoA_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    for (auto _ : state) {
        AoSoA<B, Ts...> filtered;
        filtered.reserve(size / 2);
        for (auto proxy : aosoa) {
            if constexpr (sizeof...(Ts) >= 2) {
                if (std::get<0>(proxy.refs) < std::get<1>(proxy.refs)) {
                    push_back_from_aosoa_proxy(filtered, proxy, std::index_sequence_for<Ts...>{});
                }
            } else {
                if (std::get<0>(proxy.refs) > 0) {
                    push_back_from_aosoa_proxy(filtered, proxy, std::index_sequence_for<Ts...>{});
                }
            }
        }
        benchmark::DoNotOptimize(filtered.blocks.data());
    }
}

template<size_t B, typename... Ts, size_t... Is>
void copy_aosoa_element(AoSoA<B, Ts...>& dest, size_t dest_idx,
                        AoSoA<B, Ts...>& src, size_t src_idx,
                        std::index_sequence<Is...>) {
    auto dp = dest[dest_idx];
    auto sp = src[src_idx];
    ((std::get<Is>(dp.refs) = std::get<Is>(sp.refs)), ...);
}

template<size_t B, typename... Ts>
static void BM_AoSoA_nopb_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> a1, a2;
    initialize_sorted_aosoa(a1, size, 0);  // 0, 2, 4, ...
    initialize_sorted_aosoa(a2, size, 1);  // 1, 3, 5, ...

    for (auto _ : state) {
        AoSoA<B, Ts...> merged;
        const size_t total_size = a1.size() + a2.size();
        merged.resize(total_size);

        size_t i = 0, j = 0, k = 0;
        const size_t size1 = a1.size();
        const size_t size2 = a2.size();

        while (i < size1 && j < size2) {
            auto p1 = a1[i];
            auto p2 = a2[j];
            if (std::get<0>(p1.refs) <= std::get<0>(p2.refs)) {
                copy_aosoa_element(merged, k++, a1, i++, std::index_sequence_for<Ts...>{});
            } else {
                copy_aosoa_element(merged, k++, a2, j++, std::index_sequence_for<Ts...>{});
            }
        }
        while (i < size1) {
            copy_aosoa_element(merged, k++, a1, i++, std::index_sequence_for<Ts...>{});
        }
        while (j < size2) {
            copy_aosoa_element(merged, k++, a2, j++, std::index_sequence_for<Ts...>{});
        }

        benchmark::DoNotOptimize(merged.blocks.data());
    }
}

template<size_t FieldIndex, size_t B, typename... Ts>
static void BM_AoSoA_LinearSearch(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using FieldType = std::tuple_element_t<FieldIndex, std::tuple<Ts...>>;
    const FieldType target = static_cast<FieldType>(size / 2 + FieldIndex);

    for (auto _ : state) {
        size_t found_index = size;
        size_t idx = 0;
        for (auto proxy : aosoa) {
            if (std::get<FieldIndex>(proxy.refs) == target) {
                found_index = idx;
                break;
            }
            ++idx;
        }
        benchmark::DoNotOptimize(found_index);
    }
}

// ============================================================================
// AoSoA v2: functional API (for_each / reduce / filter)
// These benchmarks drive the new lambda-based surface. They should match SOA
// speed in cache (the broken element iterator is bypassed entirely).
// ============================================================================

template<size_t B, typename... Ts>
static void BM_AoSoA_v2_Read(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t sum = 0;
        aosoa.for_each([&](auto&... xs) {
            sum += (static_cast<result_t>(xs) + ...);
        });
        benchmark::DoNotOptimize(sum);
    }
}

template<size_t B, typename... Ts>
static void BM_AoSoA_v2_Write(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    for (auto _ : state) {
        // Use for_each_indexed... no, that's global index.
        // Use an index_sequence trick so each field gets its own +I+1 constant.
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            aosoa.for_each([](auto&... xs) {
                size_t k = 0;
                ((xs += static_cast<std::decay_t<decltype(xs)>>(++k)), ...);
            });
        }(std::index_sequence_for<Ts...>{});
        benchmark::ClobberMemory();
    }
}

template<size_t B, typename... Ts>
static void BM_AoSoA_v2_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using result_t = common_t<Ts...>;

    for (auto _ : state) {
        result_t result = 0;
        // Use for_each with captured accumulator. The forwarded pack goes
        // directly into the compute expression — no intermediate tuple — so
        // GCC sees a clean fold per element and vectorizes.
        aosoa.for_each([&]<typename... Xs>(Xs&... xs) {
            constexpr size_t N = sizeof...(Xs);
            if constexpr (N == 1) {
                result += (static_cast<result_t>(xs) + ...);
            } else if constexpr (N == 2) {
                result_t head = 1;
                ((head *= static_cast<result_t>(xs)), ...);
                result += head;
            } else {
                // get<0>*get<1> + sum(get<2..N-1>)
                auto args = std::forward_as_tuple(xs...);
                result_t head = static_cast<result_t>(std::get<0>(args)) *
                                static_cast<result_t>(std::get<1>(args));
                result_t tail = [&]<size_t... Js>(std::index_sequence<Js...>) {
                    return (static_cast<result_t>(std::get<Js + 2>(args)) + ...);
                }(std::make_index_sequence<N - 2>{});
                result += head + tail;
            }
        });
        benchmark::DoNotOptimize(result);
    }
}

template<size_t B, typename... Ts>
static void BM_AoSoA_v2_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    for (auto _ : state) {
        auto filtered = aosoa.filter([](auto&... xs) {
            if constexpr (sizeof...(xs) >= 2) {
                auto tup = std::forward_as_tuple(xs...);
                return std::get<0>(tup) < std::get<1>(tup);
            } else {
                auto tup = std::forward_as_tuple(xs...);
                return std::get<0>(tup) > 0;
            }
        });
        benchmark::DoNotOptimize(filtered.blocks.data());
    }
}

// ---- Act 4: unrolled for_each variants (Agent F) ----

template<size_t UNROLL, size_t B, typename... Ts>
static void BM_AoSoA_v2_Read_uN(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);
    using result_t = common_t<Ts...>;
    for (auto _ : state) {
        result_t sum = 0;
        aosoa.template for_each_unrolled<UNROLL>([&](auto&... xs) {
            sum += (static_cast<result_t>(xs) + ...);
        });
        benchmark::DoNotOptimize(sum);
    }
}

template<size_t UNROLL, size_t B, typename... Ts>
static void BM_AoSoA_v2_Compute_uN(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);
    using result_t = common_t<Ts...>;
    for (auto _ : state) {
        result_t result = 0;
        aosoa.template for_each_unrolled<UNROLL>([&]<typename... Xs>(Xs&... xs) {
            constexpr size_t N = sizeof...(Xs);
            if constexpr (N == 1) {
                result += (static_cast<result_t>(xs) + ...);
            } else if constexpr (N == 2) {
                result_t head = 1;
                ((head *= static_cast<result_t>(xs)), ...);
                result += head;
            } else {
                auto args = std::forward_as_tuple(xs...);
                result_t head = static_cast<result_t>(std::get<0>(args)) *
                                static_cast<result_t>(std::get<1>(args));
                result_t tail = [&]<size_t... Js>(std::index_sequence<Js...>) {
                    return (static_cast<result_t>(std::get<Js + 2>(args)) + ...);
                }(std::make_index_sequence<N - 2>{});
                result += head + tail;
            }
        });
        benchmark::DoNotOptimize(result);
    }
}

// ---- Act 4: AVX2 intrinsic variants (Agent G) — float-only, B=16 ----

#if AOSOA_HAS_AVX2
template<size_t B, typename... Ts>
static void BM_AoSoA_v2_Read_avx2(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);
    for (auto _ : state) {
        float sum = aosoa.sum_all_f32_avx2();
        benchmark::DoNotOptimize(sum);
    }
}

template<size_t B, typename... Ts>
static void BM_AoSoA_v2_Compute_avx2(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);
    for (auto _ : state) {
        float r = aosoa.compute_all_f32_avx2();
        benchmark::DoNotOptimize(r);
    }
}
#endif

template<size_t FieldIndex, size_t B, typename... Ts>
static void BM_AoSoA_v2_LinearSearch(benchmark::State& state) {
    size_t size = state.range(0);
    AoSoA<B, Ts...> aosoa;
    initialize_aosoa(aosoa, size);

    using FieldType = std::tuple_element_t<FieldIndex, std::tuple<Ts...>>;
    const FieldType target = static_cast<FieldType>(size / 2 + FieldIndex);

    for (auto _ : state) {
        // Linear search via for_each_indexed with early-exit emulated by
        // storing the found index in a captured variable. GCC cannot break
        // out of a lambda-driven loop, so this walks to the end — which is
        // a property of the functional API worth benchmarking honestly.
        size_t found_index = size;
        aosoa.for_each_indexed([&](size_t gi, auto&... xs) {
            if (found_index == size) {
                auto tup = std::forward_as_tuple(xs...);
                if (std::get<FieldIndex>(tup) == target) found_index = gi;
            }
        });
        benchmark::DoNotOptimize(found_index);
    }
}

// ============================================================================
// Benchmark Registration Macros
// ============================================================================

#define REGISTER_ALL_BENCHMARKS(name, ...) \
    BENCHMARK(BM_AOS_Read<__VA_ARGS__>)->Name("AOS_Read/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_raw_Read<__VA_ARGS__>)->Name("AOS_raw_Read/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_Read<__VA_ARGS__>)->Name("SOA_Read/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_raw_Read<__VA_ARGS__>)->Name("SOA_raw_Read/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_Write<__VA_ARGS__>)->Name("AOS_Write/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_raw_Write<__VA_ARGS__>)->Name("AOS_raw_Write/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_Write<__VA_ARGS__>)->Name("SOA_Write/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_raw_Write<__VA_ARGS__>)->Name("SOA_raw_Write/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_Compute<__VA_ARGS__>)->Name("AOS_Compute/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_raw_Compute<__VA_ARGS__>)->Name("AOS_raw_Compute/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_Compute<__VA_ARGS__>)->Name("SOA_Compute/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_raw_Compute<__VA_ARGS__>)->Name("SOA_raw_Compute/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_ComputeVector<__VA_ARGS__>)->Name("AOS_ComputeVector/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_ComputeVector<__VA_ARGS__>)->Name("SOA_ComputeVector/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_ComputePushBack<__VA_ARGS__>)->Name("AOS_ComputePushBack/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_ComputePushBack<__VA_ARGS__>)->Name("SOA_ComputePushBack/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_ConditionalTransform<__VA_ARGS__>)->Name("AOS_CondTransform/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_ConditionalTransform<__VA_ARGS__>)->Name("SOA_CondTransform/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_FilterCopy<__VA_ARGS__>)->Name("AOS_FilterCopy/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_raw_FilterCopy<__VA_ARGS__>)->Name("AOS_raw_FilterCopy/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_FilterCopy<__VA_ARGS__>)->Name("SOA_FilterCopy/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_raw_FilterCopy<__VA_ARGS__>)->Name("SOA_raw_FilterCopy/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_Merge<__VA_ARGS__>)->Name("AOS_Merge/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_AOS_nopushback_Merge<__VA_ARGS__>)->Name("AOS_nopb_Merge/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_Merge<__VA_ARGS__>)->Name("SOA_Merge/" name)->Range(1000, 1000000); \
    BENCHMARK(BM_SOA_nopushback_Merge<__VA_ARGS__>)->Name("SOA_nopb_Merge/" name)->Range(1000, 1000000);

#define REGISTER_SEARCH_BENCHMARKS(name, field_idx, ...) \
    BENCHMARK(BM_AOS_LinearSearch<field_idx, __VA_ARGS__>)->Name("AOS_Search_f" #field_idx "/" name)->Range(10, 1000000); \
    BENCHMARK(BM_SOA_LinearSearch<field_idx, __VA_ARGS__>)->Name("SOA_Search_f" #field_idx "/" name)->Range(10, 1000000);

#define REGISTER_AOSOA_BENCHMARKS(name, B, ...) \
    BENCHMARK_TEMPLATE(BM_AoSoA_Read, B, __VA_ARGS__)->Name("AoSoA" #B "_Read/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_direct_Read, B, __VA_ARGS__)->Name("AoSoA" #B "_direct_Read/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_v2_Read, B, __VA_ARGS__)->Name("AoSoA" #B "_v2_Read/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_Write, B, __VA_ARGS__)->Name("AoSoA" #B "_Write/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_v2_Write, B, __VA_ARGS__)->Name("AoSoA" #B "_v2_Write/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_Compute, B, __VA_ARGS__)->Name("AoSoA" #B "_Compute/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_v2_Compute, B, __VA_ARGS__)->Name("AoSoA" #B "_v2_Compute/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_FilterCopy, B, __VA_ARGS__)->Name("AoSoA" #B "_FilterCopy/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_v2_FilterCopy, B, __VA_ARGS__)->Name("AoSoA" #B "_v2_FilterCopy/" name)->Range(1000, 1000000); \
    BENCHMARK_TEMPLATE(BM_AoSoA_nopb_Merge, B, __VA_ARGS__)->Name("AoSoA" #B "_nopb_Merge/" name)->Range(1000, 1000000);

#define REGISTER_AOSOA_SEARCH_BENCHMARKS(name, field_idx, B, ...) \
    BENCHMARK_TEMPLATE(BM_AoSoA_LinearSearch, field_idx, B, __VA_ARGS__)->Name("AoSoA" #B "_Search_f" #field_idx "/" name)->Range(10, 1000000);

// ============================================================================
// Register benchmarks for various configurations
// ============================================================================

// 3 fields - same type (baseline comparison with original)
REGISTER_ALL_BENCHMARKS("int3", int, int, int)
REGISTER_ALL_BENCHMARKS("float3", float, float, float)
REGISTER_ALL_BENCHMARKS("double3", double, double, double)

// 3 fields - heterogeneous
REGISTER_ALL_BENCHMARKS("float_double2", float, double, double)
REGISTER_ALL_BENCHMARKS("int_float_double", int, float, double)

// 2 fields
REGISTER_ALL_BENCHMARKS("int2", int, int)
REGISTER_ALL_BENCHMARKS("double2", double, double)

// 4 fields
REGISTER_ALL_BENCHMARKS("float4", float, float, float, float)

// 8 fields (half cache line for floats)
REGISTER_ALL_BENCHMARKS("float8", float, float, float, float, float, float, float, float)

// LinearSearch benchmarks (searching on field 0)
REGISTER_SEARCH_BENCHMARKS("int3", 0, int, int, int)
REGISTER_SEARCH_BENCHMARKS("float3", 0, float, float, float)
REGISTER_SEARCH_BENCHMARKS("double3", 0, double, double, double)

// ============================================================================
// AoSoA registrations: 4 type configs x 4 block sizes
// ============================================================================

// float3 with B ∈ {2, 4, 8, 16, 32, 64, 128}
REGISTER_AOSOA_BENCHMARKS("float3", 2,   float, float, float)
REGISTER_AOSOA_BENCHMARKS("float3", 4,   float, float, float)
REGISTER_AOSOA_BENCHMARKS("float3", 8,   float, float, float)
REGISTER_AOSOA_BENCHMARKS("float3", 16,  float, float, float)
REGISTER_AOSOA_BENCHMARKS("float3", 32,  float, float, float)
REGISTER_AOSOA_BENCHMARKS("float3", 64,  float, float, float)
REGISTER_AOSOA_BENCHMARKS("float3", 128, float, float, float)

// float8 with B ∈ {2, 4, 8, 16, 32, 64, 128}
REGISTER_AOSOA_BENCHMARKS("float8", 2,   float, float, float, float, float, float, float, float)
REGISTER_AOSOA_BENCHMARKS("float8", 4,   float, float, float, float, float, float, float, float)
REGISTER_AOSOA_BENCHMARKS("float8", 8,   float, float, float, float, float, float, float, float)
REGISTER_AOSOA_BENCHMARKS("float8", 16,  float, float, float, float, float, float, float, float)
REGISTER_AOSOA_BENCHMARKS("float8", 32,  float, float, float, float, float, float, float, float)
REGISTER_AOSOA_BENCHMARKS("float8", 64,  float, float, float, float, float, float, float, float)
REGISTER_AOSOA_BENCHMARKS("float8", 128, float, float, float, float, float, float, float, float)

// Act 4: opt-in optimized variants for float8 at B=16 (the recommended default).
// Unrolled for_each (Agent F) + AVX2 intrinsics with per-field accumulators
// + SW prefetch 8-blocks-ahead (Agent G).
BENCHMARK_TEMPLATE(BM_AoSoA_v2_Read_uN,    4, 16, float, float, float, float, float, float, float, float)
    ->Name("AoSoA16_v2_Read_u4/float8")->Range(1000, 1000000);
BENCHMARK_TEMPLATE(BM_AoSoA_v2_Compute_uN, 4, 16, float, float, float, float, float, float, float, float)
    ->Name("AoSoA16_v2_Compute_u4/float8")->Range(1000, 1000000);
#if AOSOA_HAS_AVX2
BENCHMARK_TEMPLATE(BM_AoSoA_v2_Read_avx2,    16, float, float, float, float, float, float, float, float)
    ->Name("AoSoA16_v2_Read_avx2/float8")->Range(1000, 1000000);
BENCHMARK_TEMPLATE(BM_AoSoA_v2_Compute_avx2, 16, float, float, float, float, float, float, float, float)
    ->Name("AoSoA16_v2_Compute_avx2/float8")->Range(1000, 1000000);
#endif

// Same opt-in variants for float3 at B=64 (the best B for that config)
BENCHMARK_TEMPLATE(BM_AoSoA_v2_Read_uN,    4, 64, float, float, float)
    ->Name("AoSoA64_v2_Read_u4/float3")->Range(1000, 1000000);
BENCHMARK_TEMPLATE(BM_AoSoA_v2_Compute_uN, 4, 64, float, float, float)
    ->Name("AoSoA64_v2_Compute_u4/float3")->Range(1000, 1000000);

// double3 with B ∈ {2, 4, 8, 16, 32, 64, 128}
REGISTER_AOSOA_BENCHMARKS("double3", 2,   double, double, double)
REGISTER_AOSOA_BENCHMARKS("double3", 4,   double, double, double)
REGISTER_AOSOA_BENCHMARKS("double3", 8,   double, double, double)
REGISTER_AOSOA_BENCHMARKS("double3", 16,  double, double, double)
REGISTER_AOSOA_BENCHMARKS("double3", 32,  double, double, double)
REGISTER_AOSOA_BENCHMARKS("double3", 64,  double, double, double)
REGISTER_AOSOA_BENCHMARKS("double3", 128, double, double, double)

// int_float_double with B ∈ {2, 4, 8, 16, 32, 64, 128}
REGISTER_AOSOA_BENCHMARKS("int_float_double", 2,   int, float, double)
REGISTER_AOSOA_BENCHMARKS("int_float_double", 4,   int, float, double)
REGISTER_AOSOA_BENCHMARKS("int_float_double", 8,   int, float, double)
REGISTER_AOSOA_BENCHMARKS("int_float_double", 16,  int, float, double)
REGISTER_AOSOA_BENCHMARKS("int_float_double", 32,  int, float, double)
REGISTER_AOSOA_BENCHMARKS("int_float_double", 64,  int, float, double)
REGISTER_AOSOA_BENCHMARKS("int_float_double", 128, int, float, double)

// AoSoA LinearSearch benchmarks (searching on field 0)
REGISTER_AOSOA_SEARCH_BENCHMARKS("float3", 0, 4, float, float, float)
REGISTER_AOSOA_SEARCH_BENCHMARKS("float3", 0, 8, float, float, float)
REGISTER_AOSOA_SEARCH_BENCHMARKS("float3", 0, 16, float, float, float)
REGISTER_AOSOA_SEARCH_BENCHMARKS("float3", 0, 64, float, float, float)

BENCHMARK_MAIN();
