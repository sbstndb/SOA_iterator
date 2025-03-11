#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>
#include <cstring> 

#include <omp.h>

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

static void BM_AOS_raw_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int sum = 0;
        for (int i = 0 ; i < size ; i++) {
		
            sum += aos[i].a + aos[i].b + aos[i].c;
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

static void BM_SOA_raw_Read(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int sum = 0;
        for (int i = 0 ; i < size; i++) {
            sum += soa.a[i] + soa.b[i] + soa.c[i];
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



static void BM_AOS_raw_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);
    for (auto s : state) {
	#pragma omp simd
        for (int i = 0 ; i < size ; i++) {
            aos[i].a += 1;
            aos[i].b += 2;
            aos[i].c += 3;
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


static void BM_SOA_raw_Write(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
	#pragma omp simd
        for (int i = 0 ; i < size ; i++) {
            soa.a[i] += 1;
            soa.b[i] += 2;
            soa.c[i] += 3;
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

static void BM_AOS_raw_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int result = 0;
	#pragma omp simd
        for (int i = 0 ; i < size ; i++) {
            result += aos[i].a * aos[i].b + aos[i].c;
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


static void BM_SOA_raw_Compute(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        int result = 0;
	#pragma omp simd
        for (int i = 0 ; i < size ; i++) {
            result += soa.a[i] * soa.b[i] + soa.c[i];
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
        for (int i = 0 ; i < size ; i++) {
            if (soa.a[i] == target) {
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

// Nouveau Benchmark : Filtrage avec copie (AOS)
static void BM_AOS_raw_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        std::vector<AOS> filtered;
        filtered.reserve(size / 2); // Estimation pour réduire les réallocations
        for (int i = 0 ; i < size ; i++) {
            if (aos[i].a < aos[i].b) {
                filtered.push_back(aos[i]);
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

// Nouveau Benchmark : Filtrage avec copie (SOA)
static void BM_SOA_raw_FilterCopy(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos;
    SOA soa(0);
    initialize_data(aos, soa, size);

    for (auto _ : state) {
        SOA filtered(0);
        filtered.a.reserve(size / 2); // Estimation pour réduire les réallocations
        filtered.b.reserve(size / 2);
        filtered.c.reserve(size / 2);
        for (int i = 0 ; i < size ; i++) {
            if (soa.a[i] < soa.b[i]) {
                filtered.a.push_back(soa.a[i]);
                filtered.b.push_back(soa.b[i]);
                filtered.c.push_back(soa.c[i]);
            }
        }
        benchmark::DoNotOptimize(filtered.a.data());
    }
}




// Initialisation des données triées
void initialize_sorted_data(std::vector<AOS>& aos, SOA& soa, size_t size, int offset = 0) {
    aos.resize(size);
    soa = SOA(size);
    for (size_t i = 0; i < size; ++i) {
        aos[i].a = static_cast<int>(i * 2 + offset); // Séquence paire pour tri
        aos[i].b = static_cast<int>(i + 1);
        aos[i].c = static_cast<int>(i + 2);
        soa.a[i] = static_cast<int>(i * 2 + offset);
        soa.b[i] = static_cast<int>(i + 1);
        soa.c[i] = static_cast<int>(i + 2);
    }
}
// Benchmark : Fusion de deux AOS
static void BM_AOS_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos1, aos2;
    SOA dummy(0); // Non utilisé ici
    initialize_sorted_data(aos1, dummy, size, 0);   // 0, 2, 4, ...
    initialize_sorted_data(aos2, dummy, size, 1);   // 1, 3, 5, ...

    for (auto _ : state) {
        std::vector<AOS> merged;
        merged.reserve(2 * size); // Préallouer pour éviter trop de réallocations
        size_t i = 0, j = 0;
        while (i < aos1.size() && j < aos2.size()) {
            if (aos1[i].a <= aos2[j].a) {
                merged.push_back(aos1[i++]);
            } else {
                merged.push_back(aos2[j++]);
            }
        }
        // Ajouter les éléments restants de aos1
        while (i < aos1.size()) {
            merged.push_back(aos1[i++]);
        }
        // Ajouter les éléments restants de aos2
        while (j < aos2.size()) {
            merged.push_back(aos2[j++]);
        }
        benchmark::DoNotOptimize(merged.data());
    }
}

/**
static void BM_AOS_nopushback_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos1, aos2;
    SOA dummy(0); // Non utilisé ici
    initialize_sorted_data(aos1, dummy, size, 0);   // 0, 2, 4, ...
    initialize_sorted_data(aos2, dummy, size, 1);   // 1, 3, 5, ...

    for (auto _ : state) {
        std::vector<AOS> merged;
        // Calculer la taille totale et préallouer avec resize
        size_t total_size = aos1.size() + aos2.size();
        merged.resize(total_size);

        size_t i = 0, j = 0, k = 0; // k est l'indice dans merged
        while (i < aos1.size() && j < aos2.size()) {
            if (aos1[i].a <= aos2[j].a) {
                merged[k] = aos1[i];
                ++i;
            } else {
                merged[k] = aos2[j];
                ++j;
            }
            ++k;
        }
        // Ajouter les éléments restants de aos1
        while (i < aos1.size()) {
            merged[k] = aos1[i];
            ++i;
            ++k;
        }
        // Ajouter les éléments restants de aos2
        while (j < aos2.size()) {
            merged[k] = aos2[j];
            ++j;
            ++k;
        }
        benchmark::DoNotOptimize(merged.data());
    }
}
**/

static void BM_AOS_nopushback_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> aos1, aos2;
    SOA dummy(0); // Non utilisé ici
    initialize_sorted_data(aos1, dummy, size, 0);   // 0, 2, 4, ...
    initialize_sorted_data(aos2, dummy, size, 1);   // 1, 3, 5, ...

    for (auto _ : state) {
        std::vector<AOS> merged;
        const size_t total_size = aos1.size() + aos2.size();
        merged.resize(total_size);

        // Optimisation 1: Pointeurs bruts
        AOS* dest = merged.data();
        const AOS* src1 = aos1.data();
        const AOS* src2 = aos2.data();

        size_t i = 0, j = 0;
        const size_t size1 = aos1.size();
        const size_t size2 = aos2.size();

        // Fusion principale avec pointeurs
        while (i < size1 && j < size2) {
            if (src1[i].a <= src2[j].a) {
                *dest++ = src1[i++];
            } else {
                *dest++ = src2[j++];
            }
        }

        // Optimisation 2: Utilisation de memcpy pour les éléments restants
        if (i < size1) {
            const size_t remaining = size1 - i;
            std::memcpy(dest, src1 + i, remaining * sizeof(AOS));
        }
        else if (j < size2) {
            const size_t remaining = size2 - j;
            std::memcpy(dest, src2 + j, remaining * sizeof(AOS));
        }

        benchmark::DoNotOptimize(merged.data());
    }
}

// Benchmark : Fusion de deux SOA
static void BM_SOA_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> dummy1, dummy2; // Non utilisé ici
    SOA soa1(0), soa2(0);
    initialize_sorted_data(dummy1, soa1, size, 0);  // 0, 2, 4, ...
    initialize_sorted_data(dummy2, soa2, size, 1);  // 1, 3, 5, ...

    for (auto _ : state) {
        SOA merged(0);
        merged.a.reserve(2 * size);
        merged.b.reserve(2 * size);
        merged.c.reserve(2 * size);
        size_t i = 0, j = 0;
        while (i < soa1.a.size() && j < soa2.a.size()) {
            if (soa1.a[i] <= soa2.a[j]) {
                merged.a.push_back(soa1.a[i]);
                merged.b.push_back(soa1.b[i]);
                merged.c.push_back(soa1.c[i]);
                ++i;
            } else {
                merged.a.push_back(soa2.a[j]);
                merged.b.push_back(soa2.b[j]);
                merged.c.push_back(soa2.c[j]);
                ++j;
            }
        }
        // Ajouter les éléments restants de soa1
        while (i < soa1.a.size()) {
            merged.a.push_back(soa1.a[i]);
            merged.b.push_back(soa1.b[i]);
            merged.c.push_back(soa1.c[i]);
            ++i;
        }
        // Ajouter les éléments restants de soa2
        while (j < soa2.a.size()) {
            merged.a.push_back(soa2.a[j]);
            merged.b.push_back(soa2.b[j]);
            merged.c.push_back(soa2.c[j]);
            ++j;
        }
        benchmark::DoNotOptimize(merged.a.data());
    }
}

/**
static void BM_SOA_nopushback_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> dummy1, dummy2; // Non utilisé ici
    SOA soa1(0), soa2(0);
    initialize_sorted_data(dummy1, soa1, size, 0);  // 0, 2, 4, ...
    initialize_sorted_data(dummy2, soa2, size, 1);  // 1, 3, 5, ...

    for (auto _ : state) {
        SOA merged(0);
        // Calculer la taille totale et préallouer avec resize
        size_t total_size = soa1.a.size() + soa2.a.size();
        merged.a.resize(total_size);
        merged.b.resize(total_size);
        merged.c.resize(total_size);

        size_t i = 0, j = 0, k = 0; // k est l'indice dans merged
        while (i < soa1.a.size() && j < soa2.a.size()) {
            if (soa1.a[i] <= soa2.a[j]) {
                merged.a[k] = soa1.a[i];
                merged.b[k] = soa1.b[i];
                merged.c[k] = soa1.c[i];
                ++i;
            } else {
                merged.a[k] = soa2.a[j];
                merged.b[k] = soa2.b[j];
                merged.c[k] = soa2.c[j];
                ++j;
            }
            ++k;
        }
        // Ajouter les éléments restants de soa1
        while (i < soa1.a.size()) {
            merged.a[k] = soa1.a[i];
            merged.b[k] = soa1.b[i];
            merged.c[k] = soa1.c[i];
            ++i;
            ++k;
        }
        // Ajouter les éléments restants de soa2
        while (j < soa2.a.size()) {
            merged.a[k] = soa2.a[j];
            merged.b[k] = soa2.b[j];
            merged.c[k] = soa2.c[j];
            ++j;
            ++k;
        }
        benchmark::DoNotOptimize(merged.a.data());
    }
}
**/
static void BM_SOA_nopushback_Merge(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<AOS> dummy1, dummy2; // Non utilisé
    SOA soa1(0), soa2(0);
    initialize_sorted_data(dummy1, soa1, size, 0);  // 0, 2, 4, ...
    initialize_sorted_data(dummy2, soa2, size, 1);  // 1, 3, 5, ...

    for (auto _ : state) {
        SOA merged(0);
        const size_t total_size = soa1.a.size() + soa2.a.size();

        // Préallocation
        merged.a.resize(total_size);
        merged.b.resize(total_size);
        merged.c.resize(total_size);

        // Optimisation 1: Pointeurs bruts pour éviter les vérifications d'index
        auto* dest_a = merged.a.data();
        auto* dest_b = merged.b.data();
        auto* dest_c = merged.c.data();

        const auto* src1_a = soa1.a.data();
        const auto* src1_b = soa1.b.data();
        const auto* src1_c = soa1.c.data();

        const auto* src2_a = soa2.a.data();
        const auto* src2_b = soa2.b.data();
        const auto* src2_c = soa2.c.data();

        size_t i = 0, j = 0;
        const size_t size1 = soa1.a.size();
        const size_t size2 = soa2.a.size();

        // Optimisation 2: Fusion principale avec moins d'opérations par itération
        while (i < size1 && j < size2) {
            if (src1_a[i] <= src2_a[j]) {
                *dest_a++ = *src1_a++;
                *dest_b++ = *src1_b++;
                *dest_c++ = *src1_c++;
                ++i;
            } else {
                *dest_a++ = *src2_a++;
                *dest_b++ = *src2_b++;
                *dest_c++ = *src2_c++;
                ++j;
            }
        }

        // Optimisation 3: Utilisation de memcpy pour les éléments restants
        if (i < size1) {
            const size_t remaining = size1 - i;
            std::memcpy(dest_a, src1_a + i, remaining * sizeof(*src1_a));
            std::memcpy(dest_b, src1_b + i, remaining * sizeof(*src1_b));
            std::memcpy(dest_c, src1_c + i, remaining * sizeof(*src1_c));
        }
        else if (j < size2) {
            const size_t remaining = size2 - j;
            std::memcpy(dest_a, src2_a + j, remaining * sizeof(*src2_a));
            std::memcpy(dest_b, src2_b + j, remaining * sizeof(*src2_b));
            std::memcpy(dest_c, src2_c + j, remaining * sizeof(*src2_c));
        }

        benchmark::DoNotOptimize(merged.a.data());
    }
}

BENCHMARK(BM_AOS_Read)->Range(1000, 1000000);
BENCHMARK(BM_AOS_raw_Read)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Read)->Range(1000, 1000000);
BENCHMARK(BM_SOA_raw_Read)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Write)->Range(1000, 1000000);
BENCHMARK(BM_AOS_raw_Write)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Write)->Range(1000, 1000000);
BENCHMARK(BM_SOA_raw_Write)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Compute)->Range(1000, 1000000);
BENCHMARK(BM_AOS_raw_Compute)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Compute)->Range(1000, 1000000);
BENCHMARK(BM_SOA_raw_Compute)->Range(1000, 1000000);
BENCHMARK(BM_AOS_ComputeVector)->Range(1000, 1000000);
BENCHMARK(BM_SOA_ComputeVector)->Range(1000, 1000000);
BENCHMARK(BM_AOS_ComputePushBack)->Range(1000, 1000000);
BENCHMARK(BM_SOA_ComputePushBack)->Range(1000, 1000000);
BENCHMARK(BM_AOS_ConditionalTransform)->Range(1000, 1000000);
BENCHMARK(BM_SOA_ConditionalTransform)->Range(1000, 1000000);
BENCHMARK(BM_AOS_LinearSearch)->Range(10, 1000000);
BENCHMARK(BM_SOA_LinearSearch)->Range(10, 1000000);
BENCHMARK(BM_AOS_FilterCopy)->Range(1000, 1000000);
BENCHMARK(BM_AOS_raw_FilterCopy)->Range(1000, 1000000);
BENCHMARK(BM_SOA_FilterCopy)->Range(1000, 1000000);
BENCHMARK(BM_SOA_raw_FilterCopy)->Range(1000, 1000000);
BENCHMARK(BM_AOS_Merge)->Range(1000, 1000000);
BENCHMARK(BM_AOS_nopushback_Merge)->Range(1000, 1000000);
BENCHMARK(BM_SOA_Merge)->Range(1000, 1000000);
BENCHMARK(BM_SOA_nopushback_Merge)->Range(1000, 1000000);


BENCHMARK_MAIN();
