

This project compares the performance of iterators using Array off Structures (AOS) implementation with a Structure of Arrays (SOA) implementation The tests measure performance for different operations (reading, writing, compute).

Data Structures
AOS: A std::vector containing structures with three int fields (a, b, c).
SOA: Three separate std::vector<int> containers, one for each field (a, b, c), with a custom iterator using a proxy to mimic AOS-like access.

Tested Operations
The benchmarks evaluate the following operations:
- Simple Read (BM_AOS_Read and BM_SOA_Read): Computes the sum of fields a, b, and c for each element.
- Write (BM_AOS_Write): Increments the fields a, b, and c for each element. (Note: Data for BM_SOA_Write is missing from the provided results.)
- Compute 

Benchmark Results
Here are the results obtained for various data sizes:
```
-----------------------------------------------------------------
Benchmark                       Time             CPU   Iterations
-----------------------------------------------------------------
BM_AOS_Read/1000              352 ns          352 ns      2007341
BM_AOS_Read/4096             1445 ns         1445 ns       478783
BM_AOS_Read/32768           11541 ns        11540 ns        60910
BM_AOS_Read/262144         106295 ns       106247 ns         6736
BM_AOS_Read/1000000        556400 ns       556340 ns         1282
BM_SOA_Read/1000              137 ns          137 ns      5102624
BM_SOA_Read/4096              499 ns          499 ns      1401343
BM_SOA_Read/32768            5309 ns         5309 ns       129781
BM_SOA_Read/262144          69610 ns        69606 ns         9986
BM_SOA_Read/1000000        341322 ns       341271 ns         1981
BM_AOS_Write/1000             256 ns          256 ns      2730928
BM_AOS_Write/4096            1112 ns         1112 ns       630474
BM_AOS_Write/32768          10718 ns        10718 ns        65160
```


