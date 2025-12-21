# Iterator Performance: AOS vs. SOA Data Layouts

This project benchmarks the performance of iterators for two data layouts: **Array of Structures (AOS)** and **Structure of Arrays (SOA)**. The focus is on comparing the standard, highly-optimized iterators of `std::vector` (used in AOS) against a custom-built proxy iterator for the SOA layout.

## Data Layouts and Iteration Strategy

### Array of Structures (AOS)

Fields of the same object are stored together. Iteration is performed using the container's standard, pointer-like iterators.

**Memory Layout:**
```
  Object 0          Object 1          Object 2
[a0, b0, c0],     [a1, b1, c1],     [a2, b2, c2], ...
|-----------|     |-----------|     |-----------|
```
-   **Iterator**: Uses the native `std::vector<AOS>::iterator`. This is typically as fast as a raw pointer.
-   **Data Access**: Accessing multiple fields of an object (`it->a`, `it->b`) is cache-friendly. However, accessing the same field across many objects can lead to strided memory access, hindering automatic vectorization.

### Structure of Arrays (SOA)

Each field is in a separate array. To provide a convenient, AOS-like interface, a custom iterator is implemented.

**Memory Layout:**
```
  Array for 'a'        Array for 'b'        Array for 'c'
[a0, a1, a2, ...],   [b0, b1, b2, ...],   [c0, c1, c2, ...]
|----------------|   |----------------|   |----------------|
```
-   **Iterator**: A custom iterator that returns a lightweight `Proxy` object on dereferencing. This proxy holds references to the elements in the separate arrays (`a[i]`, `b[i]`, `c[i]`), allowing for an intuitive `it->a` syntax.
-   **Data Access**: Accessing a single field across objects (`soa.a[i]`) is ideal for cache performance and vectorization. The proxy iterator adds a small layer of abstraction to enable convenient, object-oriented syntax.

## Build and Run

### Prerequisites
-   CMake (>= 3.10)
-   C++20 Compiler
-   Google Benchmark
-   OpenMP

On Debian/Ubuntu, install dependencies:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libgoogle-benchmark-dev libomp-dev
```

### Instructions
```bash
# Create a build directory
mkdir build && cd build

# Configure
cmake ..

# Compile
make

# Run the benchmarks
./benchmark_aos_soa
```

## Benchmark Scenarios

The project compares iterator-based loops against raw, index-based loops to isolate the performance of the iteration strategy:
-   **Iterator Benchmarks** (e.g., `BM_AOS_Read`, `BM_SOA_Read`): Use range-based for loops, which rely on the respective iterators. This tests the native `std::vector` iterator vs. the custom SOA proxy iterator.
-   **Raw Benchmarks** (e.g., `BM_AOS_raw_Read`, `BM_SOA_raw_Read`): Use traditional `for (int i=0; ...)` loops with direct index access. This serves as a baseline, showing performance without any iterator abstraction.

The following operations are tested:
-   **Read**: Simple data read (sum of fields).
-   **Write**: Data modification.
-   **Compute**: Simple computation (`a * b + c`).
-   **ComputeVector**: Compute with storage in a pre-allocated vector.
-   **ComputePushBack**: Compute with dynamic push_back.
-   **Conditional Transform**: A transform with a condition.
-   **Linear Search**: Searching for a value.
-   **Filter and Copy**: Filtering and copying data.
-   **Merge**: Merging two sorted datasets.

Variants include:
-   **raw**: Direct index-based access (no iterators)
-   **SOA2**: Alternative SOA iterator using `std::tuple`
-   **nopushback**: Optimized merge using pre-allocation and memcpy

## System Configuration

```
CPU: 22 cores @ 4700 MHz
L1 Data: 48 KiB (x11)
L1 Instruction: 64 KiB (x11)
L2 Unified: 2048 KiB (x11)
L3 Unified: 24576 KiB (x1)
Compiler: GCC 14.2.0
Flags: -Ofast -march=native -mtune=native -funroll-loops -fpeel-loops -ftree-vectorize -fprefetch-loop-arrays
```

## Benchmark Results

### Read Operations
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_Read/1000                     334 ns          334 ns      2088043
BM_AOS_Read/4096                    1413 ns         1413 ns       492332
BM_AOS_Read/32768                  11243 ns        11243 ns        61992
BM_AOS_Read/262144                 94009 ns        93979 ns         7468
BM_AOS_Read/1000000               449778 ns       449726 ns         1471
BM_AOS_raw_Read/1000                 336 ns          336 ns      2091523
BM_AOS_raw_Read/4096                1431 ns         1431 ns       492382
BM_AOS_raw_Read/32768              11349 ns        11347 ns        61447
BM_AOS_raw_Read/262144             95551 ns        95506 ns         7349
BM_AOS_raw_Read/1000000           448681 ns       448242 ns         1438
BM_SOA_Read/1000                     111 ns          111 ns      6264403
BM_SOA_Read/4096                     496 ns          496 ns      1418594
BM_SOA_Read/32768                   5075 ns         5075 ns       136581
BM_SOA_Read/262144                 74125 ns        74095 ns        10176
BM_SOA_Read/1000000               314738 ns       314529 ns         2329
BM_SOA2_Read/1000                    113 ns          112 ns      6336639
BM_SOA2_Read/4096                    527 ns          527 ns      1000000
BM_SOA2_Read/32768                  5120 ns         5119 ns       135986
BM_SOA2_Read/262144                68452 ns        68425 ns        10180
BM_SOA2_Read/1000000              305453 ns       305293 ns         2226
BM_SOA_raw_Read/1000                 109 ns          109 ns      6405610
BM_SOA_raw_Read/4096                 484 ns          484 ns      1448289
BM_SOA_raw_Read/32768               5076 ns         5075 ns       136234
BM_SOA_raw_Read/262144             67453 ns        67437 ns        10396
BM_SOA_raw_Read/1000000           298265 ns       298186 ns         2344
```
**Observation**: SOA is ~3x faster than AOS for read operations due to better cache locality for sequential field access.

### Write Operations
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_Write/1000                    255 ns          255 ns      2741054
BM_AOS_Write/4096                   1159 ns         1158 ns       603657
BM_AOS_Write/32768                 10602 ns        10599 ns        65797
BM_AOS_Write/262144               139031 ns       139013 ns         5545
BM_AOS_Write/1000000              649467 ns       649281 ns          920
BM_AOS_raw_Write/1000                255 ns          255 ns      2744044
BM_AOS_raw_Write/4096               1160 ns         1160 ns       603452
BM_AOS_raw_Write/32768             10608 ns        10605 ns        65952
BM_AOS_raw_Write/262144           136575 ns       136548 ns         5166
BM_AOS_raw_Write/1000000          680014 ns       679880 ns         1248
BM_SOA_Write/1000                    200 ns          200 ns      3515464
BM_SOA_Write/4096                   1059 ns         1058 ns       656112
BM_SOA_Write/32768                 10031 ns        10031 ns        69711
BM_SOA_Write/262144               154991 ns       154948 ns         4610
BM_SOA_Write/1000000              560750 ns       560600 ns         1250
BM_SOA_raw_Write/1000                200 ns          200 ns      3504578
BM_SOA_raw_Write/4096               1059 ns         1058 ns       662435
BM_SOA_raw_Write/32768             10181 ns        10178 ns        68516
BM_SOA_raw_Write/262144           182006 ns       181936 ns         3705
BM_SOA_raw_Write/1000000          570884 ns       570780 ns         1082
```
**Observation**: Write performance is comparable between AOS and SOA, with SOA showing slight advantages at smaller sizes.

### Compute Operations
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_Compute/1000                  364 ns          363 ns      1916305
BM_AOS_Compute/4096                 1489 ns         1489 ns       474381
BM_AOS_Compute/32768               11913 ns        11910 ns        59410
BM_AOS_Compute/262144              97965 ns        97957 ns         7044
BM_AOS_Compute/1000000            413433 ns       413289 ns         1753
BM_AOS_raw_Compute/1000              362 ns          362 ns      1926351
BM_AOS_raw_Compute/4096             1484 ns         1484 ns       410245
BM_AOS_raw_Compute/32768           12233 ns        12230 ns        58707
BM_AOS_raw_Compute/262144          97533 ns        97534 ns         7300
BM_AOS_raw_Compute/1000000        492167 ns       491821 ns         1486
BM_SOA_Compute/1000                  112 ns          112 ns      6220679
BM_SOA_Compute/4096                  491 ns          490 ns      1430689
BM_SOA_Compute/32768                5104 ns         5103 ns       136204
BM_SOA_Compute/262144              69727 ns        69700 ns        10156
BM_SOA_Compute/1000000            303481 ns       303455 ns         1662
BM_SOA_raw_Compute/1000              123 ns          123 ns      5713393
BM_SOA_raw_Compute/4096              536 ns          536 ns      1303579
BM_SOA_raw_Compute/32768            5498 ns         5496 ns       127596
BM_SOA_raw_Compute/262144          67883 ns        67875 ns        10303
BM_SOA_raw_Compute/1000000        293444 ns       293358 ns         2304
```
**Observation**: SOA is ~3x faster for compute operations, showing excellent vectorization potential.

### Compute with Vector Storage
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_ComputeVector/1000            371 ns          371 ns      1892589
BM_AOS_ComputeVector/4096           1530 ns         1530 ns       460668
BM_AOS_ComputeVector/32768         12086 ns        12083 ns        57783
BM_AOS_ComputeVector/262144       110872 ns       110854 ns         5817
BM_AOS_ComputeVector/1000000      653003 ns       652730 ns         1165
BM_SOA_ComputeVector/1000            109 ns          109 ns      6420025
BM_SOA_ComputeVector/4096            919 ns          919 ns       763866
BM_SOA_ComputeVector/32768          6440 ns         6438 ns       107851
BM_SOA_ComputeVector/262144       118172 ns       118153 ns         5941
BM_SOA_ComputeVector/1000000      486717 ns       486569 ns         1442
```

### Compute with Push Back
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_ComputePushBack/1000          984 ns          984 ns       705582
BM_AOS_ComputePushBack/4096         3417 ns         3416 ns       205422
BM_AOS_ComputePushBack/32768       25164 ns        25158 ns        27654
BM_AOS_ComputePushBack/262144     227434 ns       227363 ns         3064
BM_AOS_ComputePushBack/1000000   1461180 ns      1460768 ns          440
BM_SOA_ComputePushBack/1000         1044 ns         1044 ns       669204
BM_SOA_ComputePushBack/4096         3840 ns         3838 ns       181365
BM_SOA_ComputePushBack/32768       27643 ns        27635 ns        25469
BM_SOA_ComputePushBack/262144     246903 ns       246859 ns         2834
BM_SOA_ComputePushBack/1000000   1230018 ns      1229588 ns          567
```
**Observation**: Dynamic allocation overhead masks the SOA advantage at smaller sizes.

### Conditional Transform
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_ConditionalTransform/1000           396 ns          395 ns      1759942
BM_AOS_ConditionalTransform/4096          1692 ns         1692 ns       421080
BM_AOS_ConditionalTransform/32768        13113 ns        13112 ns        53006
BM_AOS_ConditionalTransform/262144      115946 ns       115908 ns         6090
BM_AOS_ConditionalTransform/1000000     669944 ns       669432 ns          931
BM_SOA_ConditionalTransform/1000           163 ns          162 ns      4366428
BM_SOA_ConditionalTransform/4096          1075 ns         1075 ns       662831
BM_SOA_ConditionalTransform/32768         7511 ns         7510 ns        92933
BM_SOA_ConditionalTransform/262144      114746 ns       114734 ns         6133
BM_SOA_ConditionalTransform/1000000     488812 ns       488726 ns         1493
```
**Observation**: SOA maintains advantage even with branching, though the gap narrows at large sizes.

### Linear Search
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_LinearSearch/10               2.99 ns         2.99 ns    237542260
BM_AOS_LinearSearch/64               11.7 ns         11.7 ns     60190306
BM_AOS_LinearSearch/512              79.7 ns         79.7 ns      8798310
BM_AOS_LinearSearch/4096              507 ns          507 ns      1394445
BM_AOS_LinearSearch/32768            3940 ns         3939 ns       173098
BM_AOS_LinearSearch/262144          35240 ns        35233 ns        19446
BM_AOS_LinearSearch/1000000        180033 ns       179980 ns         3809
BM_SOA_LinearSearch/10               3.63 ns         3.63 ns    192672183
BM_SOA_LinearSearch/64               6.83 ns         6.83 ns    103644089
BM_SOA_LinearSearch/512              41.5 ns         41.5 ns     16868612
BM_SOA_LinearSearch/4096              339 ns          339 ns      2066944
BM_SOA_LinearSearch/32768            2626 ns         2626 ns       267582
BM_SOA_LinearSearch/262144          20911 ns        20908 ns        33282
BM_SOA_LinearSearch/1000000         80059 ns        80055 ns         9058
```
**Observation**: SOA is ~2x faster for linear search as only one array needs to be scanned.

### Filter and Copy
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_FilterCopy/1000               983 ns          983 ns       711565
BM_AOS_FilterCopy/4096              4472 ns         4471 ns       156451
BM_AOS_FilterCopy/32768            37500 ns        37493 ns        18627
BM_AOS_FilterCopy/262144          354523 ns       354504 ns         1943
BM_AOS_FilterCopy/1000000        2759132 ns      2758636 ns          265
BM_AOS_raw_FilterCopy/1000          1009 ns         1009 ns       691750
BM_AOS_raw_FilterCopy/4096          4562 ns         4561 ns       130928
BM_AOS_raw_FilterCopy/32768        37875 ns        37866 ns        18411
BM_AOS_raw_FilterCopy/262144      352412 ns       352352 ns         1995
BM_AOS_raw_FilterCopy/1000000    2906698 ns      2905808 ns          267
BM_SOA_FilterCopy/1000              2141 ns         2141 ns       311802
BM_SOA_FilterCopy/4096              8152 ns         8136 ns        89139
BM_SOA_FilterCopy/32768            65729 ns        65697 ns        10138
BM_SOA_FilterCopy/262144          576509 ns       576256 ns         1140
BM_SOA_FilterCopy/1000000        2695751 ns      2694499 ns          276
BM_SOA_raw_FilterCopy/1000          2161 ns         2161 ns       308309
BM_SOA_raw_FilterCopy/4096          8047 ns         8041 ns        69819
BM_SOA_raw_FilterCopy/32768        65049 ns        65026 ns        10839
BM_SOA_raw_FilterCopy/262144      560852 ns       560636 ns         1256
BM_SOA_raw_FilterCopy/1000000    2519636 ns      2517680 ns          287
```
**Observation**: AOS is faster for filter+copy operations due to better memory locality when copying entire structures.

### Merge Operations
```
------------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------------
BM_AOS_Merge/1000                   2907 ns         2906 ns       241491
BM_AOS_Merge/4096                  12129 ns        12126 ns        57638
BM_AOS_Merge/32768                 97990 ns        97968 ns         7217
BM_AOS_Merge/262144               795929 ns       795820 ns          881
BM_AOS_Merge/1000000             4409764 ns      4408393 ns          137
BM_AOS_nopushback_Merge/1000        3178 ns         3178 ns       219761
BM_AOS_nopushback_Merge/4096       13904 ns        13900 ns        50571
BM_AOS_nopushback_Merge/32768     111251 ns       111216 ns         6371
BM_AOS_nopushback_Merge/262144    979894 ns       979877 ns          718
BM_AOS_nopushback_Merge/1000000  6684895 ns      6683163 ns           98
BM_SOA_Merge/1000                   4709 ns         4708 ns       148524
BM_SOA_Merge/4096                  20557 ns        20555 ns        33704
BM_SOA_Merge/32768                161488 ns       161477 ns         4156
BM_SOA_Merge/262144              1324071 ns      1323378 ns          523
BM_SOA_Merge/1000000             5057890 ns      5054543 ns          113
BM_SOA_nopushback_Merge/1000        2940 ns         2940 ns       238688
BM_SOA_nopushback_Merge/4096       12922 ns        12919 ns        54331
BM_SOA_nopushback_Merge/32768     109577 ns       109562 ns         6346
BM_SOA_nopushback_Merge/262144   1005641 ns      1005379 ns          691
BM_SOA_nopushback_Merge/1000000  7076725 ns      7075814 ns           88
```
**Observation**: AOS with push_back is faster for merge operations. The nopushback optimization with memcpy doesn't improve performance here.

## Conclusion

| Operation | Winner | Speedup |
|-----------|--------|---------|
| Read | SOA | ~3x |
| Write | Comparable | ~1.15x |
| Compute | SOA | ~3x |
| ComputeVector | SOA | ~1.3x |
| ComputePushBack | AOS | ~1.2x |
| ConditionalTransform | SOA | ~1.4x |
| LinearSearch | SOA | ~2x |
| FilterCopy | AOS | ~2x |
| Merge | AOS | ~1.6x |

The benchmark results consistently show that the **SOA layout significantly outperforms AOS** for data-parallel operations. This is due to **superior cache locality and vectorization (SIMD)** potential. When an operation accesses the same field across many elements (e.g., reading all `a` values), the contiguous memory layout of SOA is ideal for modern CPUs.

The custom **SOA proxy iterator**, while introducing a minor abstraction overhead compared to a raw indexed loop, provides a major ergonomic benefit. The performance gains from the underlying SOA layout far outweigh this small cost, making the iterator-based SOA approach a clear winner for performance-critical applications.

**When to use SOA:**
- Sequential reads across all elements
- Computation-heavy workloads
- Searching a single field
- SIMD-friendly operations

**When to use AOS:**
- Copying entire structures
- Merge operations
- Random access patterns
- When structure locality matters

## TODO / Future Work

- [ ] **Sorting Benchmark**: Implement a benchmark to compare sorting an AOS `std::vector` versus sorting the SOA structure (which requires synchronizing all parallel arrays).
- [ ] **More Complex Data**: Add benchmarks using different data types (`float`, `double`) or structures with more fields to analyze the impact on performance.
- [ ] **AOS-Favoring Algorithms**: Implement benchmarks for algorithms that might favor AOS, such as those with random access patterns or operations that require all fields of a single object simultaneously.
- [ ] **Generic SOA Container**: Evolve the `SOA` class into a generic, template-based component to support any number and type of fields, making it more reusable.
