# Generic AOS vs SOA Benchmark Suite

A comprehensive C++20 benchmark suite comparing **Array of Structures (AOS)** and **Structure of Arrays (SOA)** data layouts using variadic templates for maximum flexibility.

## Key Features

- **Fully Generic**: Support for any number of fields and any combination of types
- **Heterogeneous Types**: Mix `int`, `float`, `double` in the same structure
- **Zero-Overhead Abstraction**: Proxy iterator provides AOS-like syntax with SOA performance
- **Comprehensive Benchmarks**: 26 benchmark variants per type configuration

## Architecture

### Generic AOS (Array of Structures)

```cpp
template<typename... Ts>
struct AOS {
    std::tuple<Ts...> data;

    template<size_t I>
    auto& get() { return std::get<I>(data); }
};
```

**Memory Layout:**
```
  Object 0              Object 1              Object 2
[f0, f1, f2, ...],    [f0, f1, f2, ...],    [f0, f1, f2, ...], ...
```

### Generic SOA (Structure of Arrays)

```cpp
template<typename... Ts>
class SOA {
    std::tuple<std::vector<Ts>...> arrays;

    class Proxy {
        std::tuple<Ts&...> refs;  // References to elements across arrays
    };

    class Iterator {
        Proxy operator*();  // Returns proxy for AOS-like access
    };
};
```

**Memory Layout:**
```
  Array for field 0       Array for field 1       Array for field 2
[f0_0, f0_1, f0_2, ...], [f1_0, f1_1, f1_2, ...], [f2_0, f2_1, f2_2, ...]
```

### Generic Operations with Fold Expressions

```cpp
// Sum all fields
template<typename Tuple>
auto sum_all_fields(const Tuple& t) {
    return std::apply([](const auto&... args) {
        return (args + ...);
    }, t);
}

// Increment all fields
template<typename Tuple, size_t... Is>
void increment_all(Tuple& t, std::index_sequence<Is...>) {
    ((std::get<Is>(t) += (Is + 1)), ...);
}
```

## Build and Run

### Prerequisites
- CMake (>= 3.10)
- C++20 Compiler (GCC 10+, Clang 12+)
- Google Benchmark

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake libgoogle-benchmark-dev
```

### Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run Benchmarks

```bash
# Run all benchmarks
./benchmark_aos_soa

# Run specific type configuration
./benchmark_aos_soa --benchmark_filter="int3"
./benchmark_aos_soa --benchmark_filter="double3"
./benchmark_aos_soa --benchmark_filter="float8"
./benchmark_aos_soa --benchmark_filter="int_float_double"

# Run specific operation
./benchmark_aos_soa --benchmark_filter="Read"
./benchmark_aos_soa --benchmark_filter="Compute"
```

## Type Configurations

| Config | Types | Size | Use Case |
|--------|-------|------|----------|
| `int3` | int, int, int | 12 bytes | Integer computations |
| `float3` | float, float, float | 12 bytes | Graphics, physics |
| `double3` | double, double, double | 24 bytes | Scientific computing |
| `float8` | float x 8 | 32 bytes | SIMD-friendly (AVX) |
| `int2` | int, int | 8 bytes | 2D coordinates |
| `double2` | double, double | 16 bytes | High-precision 2D |
| `float4` | float x 4 | 16 bytes | RGBA, quaternions |
| `int_float_double` | int, float, double | 16 bytes | Heterogeneous data |
| `float_double2` | float, double, double | 20 bytes | Mixed precision |

## Benchmark Operations

| Benchmark | Description |
|-----------|-------------|
| `Read` | Sum all fields (iterator-based) |
| `raw_Read` | Sum all fields (index-based) |
| `Write` | Increment all fields |
| `Compute` | `field[0] * field[1] + sum(rest)` |
| `ComputeVector` | Compute with pre-allocated output |
| `ComputePushBack` | Compute with dynamic push_back |
| `CondTransform` | Conditional transformation |
| `FilterCopy` | Filter and copy matching elements |
| `Merge` | Merge two sorted datasets |
| `nopb_Merge` | Merge without push_back (memcpy) |
| `Search_f0` | Linear search on first field |

## System Configuration

```
CPU: 22 cores @ 4700 MHz
L1 Data: 48 KiB (x11)
L1 Instruction: 64 KiB (x11)
L2 Unified: 2048 KiB (x11)
L3 Unified: 24576 KiB (x1)
Compiler: GCC 14.2.0
Flags: -Ofast -march=native -mtune=native -funroll-loops -ftree-vectorize
```

## Benchmark Results

### int3 Configuration (int, int, int)

#### Read Operations
```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Read/1000                333 ns        2.73x
SOA_Read/1000                122 ns
AOS_Read/1000000          416291 ns        1.42x
SOA_Read/1000000          292516 ns
```

#### Compute Operations
```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Compute/1000             362 ns        2.57x
SOA_Compute/1000             141 ns
AOS_Compute/1000000       491965 ns        1.57x
SOA_Compute/1000000       312947 ns
```

#### Write Operations
```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Write/1000               136 ns        0.59x (AOS faster!)
SOA_Write/1000               231 ns
AOS_Write/1000000         755913 ns        1.14x
SOA_Write/1000000         661042 ns
```

#### Linear Search
```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Search/1000000        179918 ns        1.16x
SOA_Search/1000000        154963 ns
```

### double3 Configuration (double, double, double)

```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Read/1000                719 ns        1.99x
SOA_Read/1000                362 ns
AOS_Compute/1000             780 ns        3.92x
SOA_Compute/1000             199 ns
```

### float8 Configuration (8 floats - 32 bytes, AVX-friendly)

```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Read/1000               2686 ns       10.10x
SOA_Read/1000                266 ns
AOS_Compute/1000            2447 ns        9.23x
SOA_Compute/1000             265 ns
AOS_Read/1000000         3677241 ns        2.22x
SOA_Read/1000000         1659004 ns
```

### Heterogeneous: int_float_double

```
Benchmark                      Time       Ratio (AOS/SOA)
-----------------------------------------------------------
AOS_Read/1000               1123 ns        3.32x
SOA_Read/1000                338 ns
AOS_Read/1000000         1482934 ns        2.35x
SOA_Read/1000000          631752 ns
```

## Performance Analysis

### SOA Advantages

| Operation | Speedup | Reason |
|-----------|---------|--------|
| Read (small) | 2.5-10x | Cache line utilization, SIMD vectorization |
| Compute | 2.5-4x | Contiguous memory enables auto-vectorization |
| Search | 1.1-1.2x | Single array scan vs strided access |

### AOS Advantages

| Operation | Speedup | Reason |
|-----------|---------|--------|
| Write (small) | 1.7x | Single memory transaction per object |
| FilterCopy | 1.5-2x | Copy entire object at once |
| Merge | 1.3x | Object-level operations benefit from locality |

### Scaling Behavior

The SOA advantage diminishes with size due to memory bandwidth saturation:

| Size | SOA Read Speedup |
|------|------------------|
| 1K | 2.73x |
| 32K | 2.37x |
| 262K | 1.33x |
| 1M | 1.42x |

At large sizes, both layouts are memory-bound rather than cache-bound.

### Field Count Impact (float configurations)

| Fields | AOS Read | SOA Read | Speedup |
|--------|----------|----------|---------|
| 3 | 356 ns | 124 ns | 2.87x |
| 4 | 472 ns | 142 ns | 3.32x |
| 8 | 2686 ns | 266 ns | **10.10x** |

More fields = more wasted cache lines in AOS = bigger SOA advantage.

## Adding New Type Configurations

```cpp
// In main.cpp, add:
REGISTER_ALL_BENCHMARKS("my_config", float, int, double, char)

// For search benchmarks:
REGISTER_SEARCH_BENCHMARKS("my_config", 0, float, int, double, char)
```

## Conclusion

| Use Case | Recommended Layout |
|----------|-------------------|
| Sequential field access | **SOA** |
| Computation-heavy loops | **SOA** |
| Many fields (>4) | **SOA** (up to 10x faster) |
| SIMD/vectorization | **SOA** |
| Object copying/moving | **AOS** |
| Filtering/merging | **AOS** |
| Random access patterns | **AOS** |

The generic implementation with variadic templates provides:
- **Type safety**: Compile-time type checking for all operations
- **Flexibility**: Any number and combination of types
- **Zero overhead**: Same performance as hand-written code
- **Clean API**: Proxy iterator enables intuitive `for (auto& elem : soa)` syntax
