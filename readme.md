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
-   **Read**: Simple data read.
-   **Write**: Data modification.
-   **Compute**: Simple computation (`a * b + c`).
-   **Conditional Transform**: A transform with a condition.
-   **Linear Search**: Searching for a value.
-   **Filter and Copy**: Filtering and copying data.
-   **Merge**: Merging two sorted datasets.

## Benchmark Results
Here are the results for different data sizes:
```
-----------------------------------------------------------------
Benchmark                       Time             CPU   Iterations
-----------------------------------------------------------------
BM_AOS_Read/1000              368 ns          367 ns      1953735
BM_AOS_Read/4096             1475 ns         1474 ns       461440
BM_AOS_Read/32768           11651 ns        11645 ns        60219
BM_AOS_Read/262144         104639 ns       104545 ns         6712
BM_AOS_Read/1000000        504241 ns       504034 ns         1461
BM_SOA_Read/1000              137 ns          137 ns      5130444
BM_SOA_Read/4096              501 ns          501 ns      1399139
BM_SOA_Read/32768            5358 ns         5356 ns       131349
BM_SOA_Read/262144          71277 ns        71251 ns         9827
BM_SOA_Read/1000000        304215 ns       303880 ns         2342
BM_AOS_Write/1000             256 ns          256 ns      2727926
BM_AOS_Write/4096            1123 ns         1123 ns       630868
BM_AOS_Write/32768          10708 ns        10704 ns        65373
BM_AOS_Write/262144        135642 ns       135633 ns         4690
BM_AOS_Write/1000000       495859 ns       495578 ns         1157
BM_SOA_Write/1000             246 ns          246 ns      2843085
BM_SOA_Write/4096            1008 ns         1008 ns       690732
BM_SOA_Write/32768           9864 ns         9852 ns        71092
BM_SOA_Write/262144        126255 ns       126208 ns         5749
BM_SOA_Write/1000000       542865 ns       542509 ns         1000
BM_AOS_Compute/1000           365 ns          365 ns      1907706
BM_AOS_Compute/4096          1497 ns         1496 ns       466071
BM_AOS_Compute/32768        11939 ns        11934 ns        58936
BM_AOS_Compute/262144      104748 ns       104723 ns         5727
BM_AOS_Compute/1000000     480188 ns       480116 ns         1530
BM_SOA_Compute/1000           148 ns          148 ns      4716868
BM_SOA_Compute/4096           553 ns          552 ns      1265570
BM_SOA_Compute/32768         5718 ns         5715 ns       123606
BM_SOA_Compute/262144       69112 ns        69096 ns        10082
BM_SOA_Compute/1000000     311815 ns       311769 ns         2268
```

## Conclusion

The benchmark results consistently show that the **SOA layout significantly outperforms AOS** for data-parallel operations. This is due to **superior cache locality and vectorization (SIMD)** potential. When an operation accesses the same field across many elements (e.g., reading all `a` values), the contiguous memory layout of SOA is ideal for modern CPUs.

The custom **SOA proxy iterator**, while introducing a minor abstraction overhead compared to a raw indexed loop, provides a major ergonomic benefit. The performance gains from the underlying SOA layout far outweigh this small cost, making the iterator-based SOA approach a clear winner for performance-critical applications.

## TODO / Future Work

- [ ] **Sorting Benchmark**: Implement a benchmark to compare sorting an AOS `std::vector` versus sorting the SOA structure (which requires synchronizing all parallel arrays).
- [ ] **More Complex Data**: Add benchmarks using different data types (`float`, `double`) or structures with more fields to analyze the impact on performance.
- [ ] **AOS-Favoring Algorithms**: Implement benchmarks for algorithms that might favor AOS, such as those with random access patterns or operations that require all fields of a single object simultaneously.
- [ ] **Generic SOA Container**: Evolve the `SOA` class into a generic, template-based component to support any number and type of fields, making it more reusable.


