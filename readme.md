# AOS vs SOA iterators

This project compares the performance of **iterators** using **Array of Structures** (AOS) implementation with a **Structure of Arrays** (SOA) implementation The tests measure performance for different operations (reading, writing, compute).

### Data Structures
**AOS**: A std::vector containing structures with three int fields (a, b, c).
**SOA**: Three separate std::vector<int> containers, one for each field (a, b, c), with a custom iterator using a **proxy to mimic AOS-like access**.

### Tested Operations
The benchmarks evaluate the following operations:
- **Simple Read** (BM_AOS_Read and BM_SOA_Read): Computes the sum of fields a, b, and c for each element.
- **Write** (BM_AOS_Write and BM_SOA_Write): Increments the fields a, b, and c for each element.
- **Compute** (BM_AOS_Compute and BM_SOA_Compute): compute the sum of the fields a, b, c for each element.

### Benchmark Results
Here are the results obtained for various data sizes:
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


