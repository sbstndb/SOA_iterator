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


// Benchmark : Calculs avec stockage dans un std::vector (AOS)
static void BM_AOS_ComputeVector(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    std::vector<int> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (const auto& elem : aos) {
            results[i++] = elem.a * elem.b + elem.c;
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// Benchmark : Calculs avec stockage dans un std::vector (SOA)
static void BM_SOA_ComputeVector(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    std::vector<int> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (const auto elem : soa) {
            results[i++] = elem.a * elem.b + elem.c;
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// Nouveau Benchmark : Calculs avec push_back (AOS)
static void BM_AOS_ComputePushBack(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        std::vector<int> results;
        for (const auto& elem : aos) {
            results.push_back(elem.a * elem.b + elem.c);
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// Nouveau Benchmark : Calculs avec push_back (SOA)
static void BM_SOA_ComputePushBack(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        std::vector<int> results;
        for (const auto elem : soa) {
            results.push_back(elem.a * elem.b + elem.c);
        }
        benchmark::DoNotOptimize(results.data());
    }
}


// Nouveau Benchmark : Transformation conditionnelle (AOS)
static void BM_AOS_ConditionalTransform(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    std::vector<int> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (const auto& elem : aos) {
            if (elem.a > elem.b) {
                results[i++] = elem.c * 2;
            } else {
                results[i++] = elem.c + 1;
            }
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// Nouveau Benchmark : Transformation conditionnelle (SOA)
static void BM_SOA_ConditionalTransform(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    std::vector<int> results(size);

    for (auto _ : state) {
        size_t i = 0;
        for (const auto elem : soa) {
            if (elem.a > elem.b) {
                results[i++] = elem.c * 2;
            } else {
                results[i++] = elem.c + 1;
            }
        }
        benchmark::DoNotOptimize(results.data());
    }
}

// Nouveau Benchmark : Recherche linéaire (AOS)
static void BM_AOS_LinearSearch(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    const int target = (int)(size/2); // Valeur cible à rechercher


    for (auto _ : state) {
        size_t found_index = size; // Par défaut, non trouvé
        for (size_t i = 0; i < aos.size(); ++i) {
            if (aos[i].a == target) {
                found_index = i;
                break;
            }
        }
        benchmark::DoNotOptimize(found_index);
    }
}

// Nouveau Benchmark : Recherche linéaire (SOA)
static void BM_SOA_LinearSearch(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    const int target = (int)(size/2); // Valeur cible à rechercher

    for (auto _ : state) {
        size_t found_index = size; // Par défaut, non trouvé
        size_t i = 0;
        for (const auto elem : soa) {
            if (elem.a == target) {
                found_index = i;
                break;
            }
            ++i;
        }
        benchmark::DoNotOptimize(found_index);
    }
}

// Nouveau Benchmark : Filtrage avec copie (AOS)
static void BM_AOS_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        std::vector<AOS> filtered;
        filtered.reserve(size / 2); // Estimation pour réduire les réallocations
        for (const auto& elem : aos) {
            if (elem.a < elem.b) {
                filtered.push_back(elem);
            }
        }
        benchmark::DoNotOptimize(filtered.data());
    }
}

// Nouveau Benchmark : Filtrage avec copie (SOA)
static void BM_SOA_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        SOA filtered(0);
        filtered.a.reserve(size / 2); // Estimation pour réduire les réallocations
        filtered.b.reserve(size / 2);
        filtered.c.reserve(size / 2);
        for (const auto elem : soa) {
            if (elem.a < elem.b) {
                filtered.a.push_back(elem.a);
                filtered.b.push_back(elem.b);
                filtered.c.push_back(elem.c);
            }
        }
        benchmark::DoNotOptimize(filtered.a.data());
    }
}



BENCHMARK(BM_AOS_Read)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Read)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Write)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Write)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Compute)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Compute)->Range(1000, 1000000);
BENCHMARK(BM_AOS_ComputeVector)->Range(1000, 1000000);
BENCHMARK(BM_SOA_ComputeVector)->Range(1000, 1000000);
BENCHMARK(BM_AOS_ComputePushBack)->Range(1000, 1000000);
BENCHMARK(BM_SOA_ComputePushBack)->Range(1000, 1000000);
BENCHMARK(BM_AOS_ConditionalTransform)->Range(1000, 1000000);
BENCHMARK(BM_SOA_ConditionalTransform)->Range(1000, 1000000);
BENCHMARK(BM_AOS_LinearSearch)->Range(1000, 1000000);
BENCHMARK(BM_SOA_LinearSearch)->Range(1000, 1000000);
BENCHMARK(BM_AOS_FilterCopy)->Range(1000, 1000000);
BENCHMARK(BM_SOA_FilterCopy)->Range(1000, 1000000);


BENCHMARK_MAIN();
