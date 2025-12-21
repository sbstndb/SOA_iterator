#include <benchmark/benchmark.h>
#include <vector>
#include <tuple>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

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

BENCHMARK_MAIN();
