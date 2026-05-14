# Data-Oriented Design vs. The Hardware: A Microbenchmarking Experiment

> *A practical experiment exploring why code that looks perfectly fast on a whiteboard can be incredibly slow in reality due to physical computer hardware limits.*

## The Premise: The Illusion of O(1)
In standard data structure curricula, **O(1)** complexity is often treated as the ultimate gold standard.However, in reality, not all O(1) algorithms are created equal.

In time-critical and real-time systems, proving an algorithm is mathematically deterministic is only half the battle. An algorithm can be theoretically perfect on a whiteboard, but if a system requires a strict execution window (e.g., under 300 milliseconds), that same algorithm can easily fail due to physical hardware bottlenecks, cache fragmentation, and pipeline stalls.

## The Inspiration
The premise for this experiment was sparked by looking out at a parking lot from my balcony one morning. Observing the license plates on the cars posed a highly practical systems design question: How would a time-critical system efficiently store, route, and access a massive database of vehicles using a 9-digit identifier? 
## The Objective
The goal of this experiment is straightforward: Design the fastest possible O(1) deterministic access architecture for a large dataset of car objects and accessing a unique car with it's 9 digit license plate as a key to the data set, bound strictly by the physical limitations of the hardware rather than just algorithmic theory.

## Methodology
Throughout this repository, I document a series of architectural approaches to this problem. Using Google Benchmark for software-level timing and Intel VTune Profiler for bare-metal hardware telemetry (running natively on an Intel CPU), I analyze exactly what the processor is doing under the hood.
For each approach, I will:

    1. Introduce the routing concept and its intended benefits.
    2. Break down the bare-metal execution and hardware penalties (the "bumps" in the pipeline).
    3. Improve upon the architecture in the next iteration based purely on the physical profiling data.
## Environment
To ensure accurate microarchitectural profiling, all benchmarks were built in Release x64 and run natively to measure actual nanosecond-level memory latency.

CPU: Intel Core i9-14900HX (Mobile DDR5 Platform)

Microarchitecture Caches:

L1 Data: 48 KiB per core

L1 Instruction: 32 KiB per core

L2 Unified: 2 MiB per core

L3 Unified: 36 MiB shared

DRAM: 32 GB DDR5

Compiler: MSVC (Required for Link-Time Code Generation and VTune microarchitecture profiling)

Libraries: Google Benchmark (for statistical timing), Intel VTune Profiler (for hardware performance counters)

## Foundation
Before evaluating the different routing approaches, we established a strict, standardized environment. The common.hpp header defines the core data structures and the benchmarking constraints used across all tests.
 ### 1. The Car Object:
We defined a minimal struct to represent our data payload.
    
 ``` cpp
    struct Car
    {
        //9 digits license plate - 4 bytes
        uint32_t licensePlate;

        //Time elapsed since program start in nanoseconds
        uint64_t timeStamp;

        Car(uint32_t lp = 0,uint64_t Time = 0) : licensePlate(lp) , timeStamp(Time) {}

        //c++ compiler will add another 4 bytes of padding to this struct in order to round to 16 bytes.
    };
 ```

* Data-Oriented Note: While the defined variables only take up 12 bytes, the C++ compilerautomatically adds 4 bytes of padding to round the struct to 16 bytes. This ensures optimalmemory alignment when arrays of Car objects are loaded into the CPU's 64-byte cache lines.

  ### 2. The Constants:

 We establish the scale of the dataset and the exact target we are searching for to ensure consistency across all benchmarks.
 ``` cpp
    #define GET_DIGIT(LP , POW) ((LP/POW) % 10)// Math to isolate a single 0-9 digit
    const uint32_t TARGET_LP = 123456789;
    const int NUM_CARS = 1000000;
    const int CACHE_SIZE = 40 * 1024 * 1024; // 40 MB for L3 Flush - tailored for my L3
 ```   
 ### 3. Guaranteeing "Cold" Reads
Modern CPUs are incredibly aggressive at prefetching and caching data. If we run a loop 1,000 times, iterations 2 through 1000 will be artificially fast because the data is already sitting in the L1/L2 cache. To measure true RAM latency, we must physically force the CPU to forget the data between iterations.
 ``` cpp
    inline void FlushCacheCold() {
        volatile char* cacheTrash = new char[CACHE_SIZE];
        for(int i = 0; i < CACHE_SIZE; i += 64) {
            cacheTrash[i] = 1; 
        }
        delete[] cacheTrash;
    }
 ```
    * Because the i9-14900HX has a 36 MB L3 Cache, we allocate a 40 MB dummy array and write to every 64th byte (the exact size of a cache line). This completely evicts our Car data from the processor, guaranteeing that every benchmark iteration forces a fresh, 80-nanosecond physical read from the DDR5 RAM.

 ### 4. Deterministic Data Generation:

Finally, GenerateTestPlates() uses the <random> library (std::mt19937) to generate exactly 1,000,000 unique 9-digit license plates. It shuffles them to prevent sequential access biases and explicitly inserts our TARGET_LP to guarantee a successful lookup during the benchmarks.
    
 ``` cpp
    inline std::vector<uint32_t> GenerateTestPlates() {
        std::vector<uint32_t> plates;
        std::mt19937 rng(42); // Fixed seed for fair comparison across tests
        std::uniform_int_distribution<uint32_t> dist(100000000, 999999999);

        for(int i = 0; i < NUM_CARS; ++i) {
            plates.push_back(dist(rng));
        }
        return plates;
    }
 ```



## 1st APPROACH: 9-Dimensional Object Array
Intuition: Similiar to a simple hash table, we can decifer the position of the car object 
by using the license-Plate's first digit for accessing the first layer of arrays, the second digit
will decifer the obj's position within the 2nd layer and so forth until the last digit in the 9th layer. Trivial to prove that such algorithm is a deterministic O(1). Yet, the performance is a disaster: For each layer in the multi array, the CPU sends an ALU BRUNCH request and stalls until the calculation is complete before the next layer could be calculated. On the ALU's side , modular operations and division by numbers that arent devided by 2 is a massive calculation in the microarchitecture resolution. 
/*9D_Array_Approach.cpp code snippet and exlpanation, Vtune profile spreadsheat and google benchmark result output txt file - proving the performance.*/