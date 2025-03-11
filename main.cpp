#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>

struct AOS {
    int a;
    int b;
    int c;
};

class SOA {
public:
    std::vector<int> a;
    std::vector<int> b;
    std::vector<int> c;

    explicit SOA(size_t size) : a(size), b(size), c(size) {}

    template <typename T>
    class Proxy {
    public:
        T& a;
        T& b;
        T& c;

        Proxy(T& a_, T& b_, T& c_) : a(a_), b(b_), c(c_) {}
    };

    class Iterator {
        SOA* soa;
        size_t index;

    public:
        Iterator(SOA* soa_, size_t index_) : soa(soa_), index(index_) {}

        Proxy<int> operator*() {
            return Proxy<int>(soa->a[index], soa->b[index], soa->c[index]);
        }

        Iterator& operator++() {
            ++index;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return index != other.index || soa != other.soa;
        }
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end() { return Iterator(this, a.size()); }
};

void initialize_data(std::vector<AOS>& aos, SOA& soa, size_t size) {
    aos.resize(size);
    soa = SOA(size);
    for (size_t i = 0; i < size; ++i) {
        aos[i].a = static_cast<int>(i);
        aos[i].b = static_cast<int>(i + 1);
        aos[i].c = static_cast<int>(i + 2);
        soa.a[i] = static_cast<int>(i);
        soa.b[i] = static_cast<int>(i + 1);
        soa.c[i] = static_cast<int>(i + 2);
    }
}

static void BM_AOS_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int sum = 0;
        for (const auto& elem : aos) {
            sum += elem.a + elem.b + elem.c;
        }
        benchmark::DoNotOptimize(sum);
    }
}

static void BM_SOA_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int sum = 0;
        for (const auto& elem : soa) {
            sum += elem.a + elem.b + elem.c;
        }
        benchmark::DoNotOptimize(sum);
    }
}

static void BM_AOS_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        for (auto& elem : aos) {
            elem.a += 1;
            elem.b += 2;
            elem.c += 3;
        }
        benchmark::ClobberMemory();
    }
}

static void BM_SOA_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        for (auto elem : soa) {
            elem.a += 1;
            elem.b += 2;
            elem.c += 3;
        }
        benchmark::ClobberMemory();
    }
}

static void BM_AOS_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int result = 0;
        for (const auto& elem : aos) {
            result += elem.a * elem.b + elem.c;
        }
        benchmark::DoNotOptimize(result);
    }
}

static void BM_SOA_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int result = 0;
        for (const auto& elem : soa) {
            result += elem.a * elem.b + elem.c;
        }
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(BM_AOS_Read)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Read)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Write)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Write)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Compute)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Compute)->Range(1000, 1000000);

BENCHMARK_MAIN();
