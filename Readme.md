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

 ## Intuition:

Since a license plate is exactly 9 digits long (with each digit ranging from 0 to 9), we can theoretically map this directly to a 9-dimensional array. The first digit determines the index of the 1st dimension, the second digit determines the index of the 2nd dimension, and so on.
By the time the CPU resolves the 9th dimension, it lands perfectly on the exact Car object. No searching, no collisions, no hashing-just pure, deterministic $O(1)$ array access.

 ## The Code Implementation

 To execute this, we first define the massive 9D array structure. We then use our GET_DIGIT macro (which isolates a specific digit using division and modulo arithmetic) to route the lookup.
  * Additional Notes: 
  * * We wrap the flush caching function with time Pausing operations because we dont want to profile the additional cache dumping in the profiler. The benchmark assumes worst conditions, where each attempted access to the car object is on a "cold" cache - therefore is not part of the algorithm.
  * * We use actual, static values as parameters in the GET_DIGIT function in order to prevent any unnecessary overhead from automated functions.
  * * We use the operand "DoNotOptimize" in order to tell the compiler,  that even though we have a massive loop that doesnt actually do any logic or arent manipulating any of the data in any way, Not to skip each process. Else, the aggressive optimization that the compiler will use, would definetly skip this loop and not run this part of the code in any way. This way we gaurantee a "look-up" / access of the car object within the database.

  ### 1.Laying the DataStructre and the Database:
 ```cpp
using ObjArray = Car[10][10][10][10][10][10][10][10][10];
static void BM_9D_OBJ_Array(benchmark::State& state) {
    auto plates = GenerateTestPlates();
    auto multiArr = new ObjArray(); // Allocate a 9 dimensional array.  
    for (uint32_t lp : plates) { // Fill the DataBase with the generated plates.
            multiArr[GET_DIGIT(lp, 100000000)]
                    [GET_DIGIT(lp, 10000000)]
                    [GET_DIGIT(lp, 1000000)]
                    [GET_DIGIT(lp, 100000)]
                    [GET_DIGIT(lp, 10000)]
                    [GET_DIGIT(lp, 1000)]
                    [GET_DIGIT(lp, 100)]
                    [GET_DIGIT(lp, 10)]
                    [GET_DIGIT(lp, 1)] = Car(lp, 999);
    }
    ...
 ```
 ### 2.Flushing the Cache and accessing the car:
 ```cpp
 for(auto _ : state) {
        state.PauseTiming(); // pause googleBenchmark clock
        __itt_pause();  // pause vtune profiling
        FlushCacheCold(); // fill cache with garbage value 
        __itt_resume(); // resume vtune profiling
        state.ResumeTiming(); // resume googlebenchmark clock
        
        for (uint32_t lp : plates) { // for each licensePlate, access the car through the 9D array:
            Car& accessedCar = multiArr
                    [GET_DIGIT(lp, 100000000)]
                    [GET_DIGIT(lp, 10000000)]
                    [GET_DIGIT(lp, 1000000)]
                    [GET_DIGIT(lp, 100000)]
                    [GET_DIGIT(lp, 10000)]
                    [GET_DIGIT(lp, 1000)]
                    [GET_DIGIT(lp, 100)]
                    [GET_DIGIT(lp, 10)]
                    [GET_DIGIT(lp, 1)];
            benchmark::DoNotOptimize(accessedCar.timeStamp);
        }
        ```
