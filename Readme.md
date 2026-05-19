# Array of Structs vs. The Hardware: Exploiting Memory Level Parallelism

> *A practical experiment exploring why code that looks perfectly fast on whiteboard can be incredibly slow in reality, and how to use hardware pipelines to force standard C++ objects to perform at scale.*

## Table of Contents
* [The Premise: The Classroom Disconnect](#the-premise-the-classroom-disconnect)
* [The Inspiration](#the-inspiration)
* [The Objective](#the-objective)
* [Methodology & Environment](#methodology)
* [Foundation & Standardized Constraints](#foundation)
* [1st Approach: 9-Dimensional Object Array](#1st-approach-9-dimensional-object-array)
* [2nd Approach: Routing Table](#2nd-approach-routing-table)
* [3rd Approach: 1D Linear Object Array (The Brute Force Baseline)](#3rd-approach-1d-linear-object-array-the-brute-force-baseline)
* [4th Approach: Batched Routing Table](#4th-approach-batched-routing-table)
* [Conclusions and Further Optimizations](#conclusions-and-further-optimizations)
* [Final Words](#final-words)

---

## The Premise: The Classroom Disconnect
In standard computer science curricula, algorithms are often taught in a vacuum. We learn that O(1) complexity is the ultimate gold standard, and we are encouraged to build applications using intuitive, object-oriented Array of Structs (AoS) architectures (e.g., an array of Car objects).

However, in time-critical and real-time systems, proving an algorithm is mathematically deterministic on a whiteboard is only half the battle. When I asked a question in class about how these standard O(1) data structures actually behave in physical RAM, the purely theoretical answers felt unsatisfying. The reality is that if a system requires a strict execution window, a mathematically "perfect" AoS algorithm can easily fail due to physical hardware bottlenecks, cache fragmentation and pipeline stalls.

## The Inspiration
The premise for this experiment was sparked by a question that i came up with during algorithm course in 3rd semester of my studies. The teacher showed us the basics of hash table datastructures where the given example was hashing a number to the table by it's 1st digit (using modulo operation hashing). This was to show that the number '72' & '52' could be mapped to the same bucket. and in turn there will be different solutions to this problem. 

That led me to ask the professor after class, *what if we make another hash table for each bucket?* - meaning that if '72' & '52' are mapped to the same bucket, we can run another hash function on the 2nd digit, routing each number to different 'nested' buckets. 
*But what about an N digit number?* - That would mean we would need a hash table sized N that consists of Hash tables sized N in each bucket, and so on and so on... 

On paper that is a deterministic "access" time for each object, but in real world scenarios, the real time to access an pbject is a nightmare due to different hardware limitations.

So how do algorithm reaserchers really optimize their algorithm? 
The professor gave a somewhat theoretical answer - *"In cases like these sometimes the Algorithm will change and might be preferred to use less optimal time complexity solutions"*. 

I was unsatisfied with the answer but left it at that for the time being.

One day, While looking out at a parking lot from my balcony, Observing the license plates on the cars i remembered my curiosity regarding a question that left me unsatisfied and thought of a practical systems design question:

How would a time-critical system efficiently access a massive database of vehicles using a 9-digit identifier (license plates)? 
And so, the project was born. 

## The Objective
The goal of this experiment is straightforward: Design the fastest possible throughput for a large dataset of car objects and accessing a unique car with it's 9 digit license plate as a key to the data set, bound strictly by the physical limitations of the hardware rather than just algorithmic theory.

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
 * The code snippets refer to [common.hpp](/common.hpp)

### 1. The Car Object:
We defined a minimal struct to represent our data payload.
    
 ``` cpp
    struct Car
    {
        //9 digits license plate - 4 bytes
        uint32_t licensePlate;

        //Time elapsed since program start in nanoseconds. Ended up Using it by ensuring the lookup succeeded with donotoptimize    directive. (misleading var naming).
        uint64_t timeStamp; // 8 bytes

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
        auto cacheTrash = std::make_unique<char[]>(CACHE_SIZE);
        volatile char* rawTrash = cacheTrash.get(); //Extract a raw volatile pointer purely to bypass compiler optimization
        for(int i = 0; i < CACHE_SIZE; i += 64) {
            rawTrash[i] = 1;
        }
    
    }
 ```
* Because the i9-14900HX has a 36 MB L3 Cache, we allocate a 40 MB dummy array and write to every 64th byte (the exact size of a cache line). This completely evicts our Car data from the processor, guaranteeing that every benchmark iteration forces a fresh, 80-nanosecond physical read from the DDR5 RAM.

### 4. Deterministic Data Generation:

Finally, GenerateTestPlates() uses the <random> library (std::mt19937) to generate exactly 1,000,000 unique 9-digit license plates. It shuffles them to prevent sequential access biases and explicitly inserts our license plates to guarantee a successful lookup during the benchmarks.
    
 ```cpp
    inline std::vector<uint32_t> GenerateTestPlates() {
        std::unordered_set<uint32_t> uniqueSet;

        // Pre-allocate the hash map:
        // This prevents the set from re-allocating and re-hashing 
        // memory every time it grows - to save setup time.
        uniqueSet.reserve(NUM_CARS);

        std::mt19937 rng(TARGET_INDEX); // Fixed seed for deterministic benchmarks
        std::uniform_int_distribution<uint32_t> dist(100000000, 999999999);



        // Rejection Sampling: Keeps generating until 1,000,000
        while (uniqueSet.size() < NUM_CARS) {
            uniqueSet.insert(dist(rng)); // Duplicates are automatically ignored
        }

        // Transfers the guaranteed unique numbers to a cache-friendly array
        std::vector<uint32_t> plates(uniqueSet.begin(), uniqueSet.end());

        // Shuffles the vector so the access pattern is more likely to be random
        std::shuffle(plates.begin(), plates.end(), rng);

        return plates;
    }
 ```



## 1st APPROACH: [9-Dimensional Object Array](/9D_Array_Approach.cpp)

### Intuition:

Since a license plate is exactly 9 digits long (with each digit ranging from 0 to 9), we can theoretically map this directly to a 9-dimensional array. The first digit determines the index of the 1st dimension, the second digit determines the index of the 2nd dimension, and so on.
By the time the CPU resolves the 9th dimension, it lands perfectly on the exact Car object. No searching, no collisions, no hashing-just pure, deterministic $O(1)$ array access.

### The Code Implementation
To execute this, we first define the massive 9D array structure. We then use our GET_DIGIT macro (which isolates a specific digit using division and modulo arithmetic) to route the lookup.
  
#### Additional Notes: 
   * We wrap the flush caching function with time Pausing operations because we dont want to profile the additional cache dumping in the profiler. The benchmark assumes worst conditions, where each attempted sequential access to the car objects is on a "cold" cache - therefore is not part of the measuring.
   * We use actual, static values as parameters in the GET_DIGIT function in order to prevent any unnecessary overhead from automated functions.
   * We use the function "DoNotOptimize" in order to tell the compiler,  that even though we have a massive loop that doesnt actually do any logic or arent manipulating any of the data in any way, Not to skip each process. Else, the aggressive optimization that the compiler will use, would definetly skip this loop and not run this part of the code in any way. This way we gaurantee a "look-up" / access of the car object within the database.

#### 1.Laying the DataStructre and the Database:
 ```cpp
using ObjArray = Car[10][10][10][10][10][10][10][10]; // 8 dimensional array in order to obey the initial strict 2GB Allowance from the OS

static void BM_9D_OBJ_Array(benchmark::State& state) {
    auto plates = GenerateTestPlates();
    auto multiArr = std::make_unique<ObjArray[]>(10); // Allocate a 9 dimensional array.  
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
    ...rest of code.
 ```
#### 2.Flushing the Cache and accessing the car:
 ```cpp
 for(auto _ : state) {
            state.PauseTiming(); // pause googleBenchmark clock
            __itt_pause();  // pause vtune profiling
            FlushCacheCold(); // fill cache with garbage value 
            state.ResumeTiming(); // resume googlebenchmark clock
            __itt_resume(); // resume vtune profiling
            for (uint32_t lp : plates) {
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
            benchmark::ClobberMemory();
        }
        ...
```
## The Results & Telemetry
### Google Benchmark Output: 5,000 iterations, ~22.5ms per iteration
Run for 5,000 iterations, the result is ~22.5 ms per iteration. Since each iteration performs 1,000,000 object accesses, this results in exactly 22.5 ns per object lookup. While 22.5 ns might seem fast on paper, running this approach through the Intel VTune Profiler reveals that the silicon is actually struggling massively. Instead of a memory-bandwidth bottleneck, we hit a massive computational wall:
### VTune Telemetry Summary
| Metric | Value | Microarchitectural Impact |
| :---   | :---  | :---                      |
| **Execution Time** | `0.234 s` | Massive delay compared to linear lookup. |
| **CPI Rate** | `0.53` | Cycles Per Instruction- indicates pipeline efficiency. |
| **Core Bound** | `55.8%` | ALU is severely bottlenecked by division/modulo operations. |
| **Store STLB Hit** | `19.2%` | Heavy TLB pressure from fragmenting 16GB of memory pages. |

* (For the complete hardware counter breakdown, see the [raw VTune xlsx export](/Gathered%20Results/RES_9D_ARR_OBJ.csv))*
#### Core Bound (55.8%): 
 Over half of the CPU's pipeline slots are stalled directly inside the execution units. The ALU (Arithmetic Logic Unit) is completely saturated trying to process the heavy idiv (integer division) instructions required by the GET_DIGIT macro. The processor is so busy doing math that it cannot efficiently issue memory requests.
#### Store Latency & STLB Overhead (~19.2%):
 The processor's Second-Level Translation Lookaside Buffer (STLB) is under immense pressure. Because the 16GB array is so heavily fragmented across the system's memory pages, the hardware struggles to map the virtual addresses to physical RAM addresses, adding significant latency before the DRAM is even accessed.

## 2nd Approach: [Routing Table](/1D_RoutingTable_Approach.cpp)
### Intuition
By minimizing the size of the array, the CPU could potentially fetch more data into the L3 cache at each cache miss. Furthermore, the contiguous storage array should allow the hardware prefetcher to load data before we even request it.
### The Code Implementation
This improved approach implements Data-Oriented Design. We use a 4GB flat array of 4-byte integers (indices) to act as a "Routing Table". These indices point to a tightly packed, contiguous `carStorage` array. 

#### Side note (The Caveat)
To isolate the memory bandwidth for this benchmark, I assumed the exact number of active cars (1,000,000) beforehand to pre-allocate a tight `carStorage` array. In a real-world system with 1 billion possible 9-digit plates, guaranteeing this perfect, continuous density dynamically is impossible without implementing custom memory arenas or page-based pool allocators. I have intentionally traded dynamic flexibility for hardware speed (which could be argued as cheating in some sense , but i didnt want my test to run for two days to finish gathering the data).
#### 1.Laying the DataStructre and the Database
```cpp
static void BM_RoutingTable(benchmark::State& state) {
    auto plates = GenerateTestPlates();
    auto carStorage = std::make_unique<Car[]>(NUM_CARS);
    
    // Allocate 1 billion 4-byte integers (4GB) instead of 16-byte objects
    auto routingTable = std::make_unique<uint32_t[]>(1000000000);

    for (uint32_t i = 0; i < NUM_CARS; ++i) {
        uint32_t lp = plates[i];
        carStorage[i] = Car(lp, 999);
        routingTable[lp] = i; // Store the index, not the address
    }
    ...rest of code
```
#### 2.Flushing the Cache and accessing the car
```cpp
    // ...flush cache before starting a new pointer chasing-iteration

        for (uint32_t lp : plates) { // for each of the 1m plates:
            //Step 1: Fetch the 4-byte index
            uint32_t fetchedIndex = routingTable[lp];
            //Step 2: Access the dense array
            Car* accessedCar = &carStorage[fetchedIndex];
            benchmark::DoNotOptimize(accessedCar->timeStamp);
        }
...rest of code
```
## The Results & Telemetry
### Google Benchmark Output: 1,000 iterations, ~21.7ms per iteration
Run for 1,000 iterations, the result is ~21.7 ms per iteration. Since each iteration performs 1,000,000 object accesses, this results in 21.7ns on average per object lookup.

Turns out, by completely eliminating the heavy integer division math of the 9D array, and shrinking the footprint from 16GB to 4GB, The Approach only improved the lookup time by a fraction of a nanosecond (from 22.5 ns to 21.7 ns).

Why didn't this fix the bottleneck? *([The Intel VTune profiling table](/Gathered%20Results/RES_ROUTING.csv))* gives the exact answer.
#### VTune Telemetry & Assembly Analysis
|Metric |	Value |	Microarchitectural Impact|
| :---   | :---  | :---                      |
| **Execution Time** |	`17.75 s` |	Massive cumulative time spent stalled waiting for memory.|
| **CPI Rate** | `4.91`|	Cycles Per Instruction; extremely high, proving the pipeline is frozen.|
| **DRAM Bound** | `82.6%`	|Successfully traded the ALU math bottleneck for a pure physical RAM bottleneck.|

 * Of course, there are many more interesting columns that reveal clues - as shown in the [exported sheet](/Gathered%20Results/RES_ROUTING.csv), but i have left them out for brevity.  

#### The Pointer Chasing Trap
By removing the heavy math of the 9D Array, in doing so, This accidentally created a new, equally destructive bottleneck: a data dependency chain in memory. 
To find a car, the CPU must now execute two distinct steps: Fetch the intermediate index from the routingTable, (Cache Miss $\rightarrow$ RAM Stall).
Use that fetched index to pull the Car from carStorage (Cache Miss $\rightarrow$ RAM Stall).
Because Step 2 strictly requires the exact value from Step 1, the CPU is stalled. It cannot look ahead or optimize the second memory fetch because it doesn't have the address yet. In time-critical systems, waiting for a chained memory pointer is just as destructive to throughput as a heavy ALU math stall. Although, it's important to note that even though the throughput hasn't improved drastically - 4GB as opposed to 16GB is a massive improvement and much more desired in a real world scenario. 

## 3rd Approach: [1D Linear Object Array](/1D_Linear_Array_Approach.cpp) (The Brute Force Baseline)
### Intuition: 
The profiling of the 9-Dimensional array proved that heavy arithmetic (division and modulo) destroys the CPU pipeline before it can even fetch the memory, The Routing table proved that dependant address fetching in each lookup is a massive hit to performance. Therefore, by combining the best of these two approaches-the next logical step is to unfortunately go back to an undesirable 16GB datastructure in order to achieve true, unobstructed O(1) access and eliminate the GET_DIGIT math entirely. 
The most direct solution is to flatten the architecture. Since the maximum license plate value is 999,999,999 I have to allocate a single, massive 1-Dimensional array with 1 billion slots. By using the 9-digit license plate as the literal index (array[licensePlate]), This reduces the access logic to a single, native CPU instruction: a base-pointer offset. No division, no traversal—just immediate memory addressing. - This is an impractical solution, as most systems do not usually rely on such a large DRAM component to achieve throughput. But as the Vtune and google benchmark telemetry will soon reveal, There is a major Lesson to be learned and apply.

### The [Code Implementation](/1D_Linear_Array_Approach.cpp)
#### 1. Laying the DataStructre and the Database:
```cpp
static void BM_1D_Linear_Array(benchmark::State& state)
{
    auto plates = GenerateTestPlates();
    auto multiArr = std::make_unique<Car[]>(1000000000ULL); // allocate 1 Bilion cars
    for (uint32_t lp : plates) // Fill 1 milion random plates into the database.
    {
        multiArr[lp] = Car(lp, 999);
    }
...rest of code
```
 #### 2. Flushing the Cache and accessing the car:
```cpp
for (auto _ : state)
    {
        //... fill cache with grabage before next iteration.(flushCacheCold func.)

        for (uint32_t lp : plates)
        {
            Car& accessedCar = multiArr[lp]; // access the car directly in the array - 1 memory request.
            benchmark::DoNotOptimize(accessedCar.timeStamp);
        }
...rest of code
```
## The Results & Telemetry
### [Google Benchmark Output](/Gathered%20Results/GBMRES_1D_Linear.txt): ~2.99 ms per iteration
Run for 1,000 iterations, the result is ~2.99 ms per iteration. Since each iteration performs 1,000,000 object accesses, this results in exactly 2.99 ns per object lookup.
When I first saw this output in the terminal, I honestly thought I had broken the benchmark.

I double-checked my FlushCacheCold() function and my DoNotOptimize functions, assuming the compiler had somehow cheated and skipped the loop. The approach is randomly jumping around a massive, empty 16 Gigabyte RAM allocation. A single physical read from DDR5 RAM should take roughly 80 nanoseconds. The prefetcher and brunch prediction mechanisms are powerful, but the result is suspiciuosly **x20 times Faster!**. There has to be something going on that im not aware of. 
After reading more about the observed phenomena and consulting several LLMs , I Learned that the reason behind the extra-ordinary throughput achieved, is due to a mechanism in x86 cpus called **Memory Level Parallelism (MLP)**. 
A simple explanation of MLP is that the CPU can send multiple requests to load from memory simultaniously. 
In the context of the 1D Linear approach, it can be observed in the *([Vtune profiling table](/Gathered%20Results/RES_LINEAR_ARR.csv))*    

### [VTune Telemetry Summary](/Gathered%20Results/RES_LINEAR_ARR.csv)
| Metric | Value | Microarchitectural Impact |
| :---   | :---  | :---                      |
| **Execution Time** | `0.41 s`|	A massive speedup compared to the Routing Table lookup. |
| **CPI Rate** | `0.96` | Cycles Per Instruction. the pipeline is flowing again (down from 4.91). |
| **DRAM Bound** | `24.3%`	| The CPU is still fetching from RAM, but it is no longer frozen by it.|

### Memory Level Parallelism (MLP)
In the Routing Table approach, the CPU was trapped in a serial dependency. It had to wait for the index to arrive from RAM before it could ask for the Car.But in the Brute Force 1D Array, I completely removed that intermediate step. Inside my loop, multiArr[lp] only depends on the license plate lp (which is already sitting locally in the L1 cache). Because the memory addresses don't depend on each other, the CPU's Out-of-Order execution engine - comes into effect.*(to put simply, a mechanism that can look ahead at the next Assembly inst., and if it finds any that can be "calculated" or is a L/S inst. independent of other inst. to execute/calculate, completes the task and flags the register as a "completed" task. In the case of Load instructions, as long as the memory address doesnt depend on a still-calculating register, the CPU's Load/Store Unit fires off the memory request and assigns it to an available Line Fill Buffer (LFB).)* . 
And so, The CPU fires off dozens of parallel requests directly to the physical RAM at the exact same time *(More accurately, the Reorder Buffer (ROB) tracks the original sequential order of the code to ensure the chaotic OoO execution doesn't break the program state. - Therefore, While the ROB is stalled waiting for the first 80ns memory load to finish, the OoO engine uses the LFBs to fetch the next memory requests in parallel. Once the first load arrives and retires, the ROB can retire the subsequent loads because their data was already fetched in the background)*

Even though every single Car still physically takes 80 nanoseconds to travel from the DDR5 sticks to the CPU, the hardware pipelines the fetches well enough that the wait time drops to 2.99 nanoseconds.

## 4th Approach: Batched Routing Table
### Intuition
 After benchmarking the 1D_Linear approach,
 The Data speaks for itself - the Routing Table managed to access the car within ~3ns; That means that the CPU didnt entirely stall for the entire Load process (given that DRAM is ~80ns). As discussed before, what probably happened is that the OoO Engine realised that the next car to look-up in the data set is not dependant on the previous license plate, So instead of stalling while the licensePlate No.1 was being fetched from memory, it meanwhile moved to take care of the next licensePlate(No.2) it added another memory request execution for No.2, the ROB "Filled it's tray" with requests to load from memory, and so on...
 That Got me Thinking:  
 How can we combine the Two Approaches to elevate the lookup time?
 The problem with the routing table was that the data was dependant on each other: 
 SCENARIO ROUTING TABLE: ACCESS CAR A
 1. fetch index of car A - mem request in routingTable (80ns).
 2. use fetched index to fetch car A - cannot begin until step 1 is complete(stall).
 
But what if I can use the MLP mechanic to elevate the lookup between each 2 steps? within each two steps, the data is dependant. which means that for every third step the data is Independant...
So the approach is to "exploit" the MLP mechanic in order to "batch" multiple memory requests for the index array, Then batch multiple memory requests constrained by the return value of step 1.

### Exploiting the MLP Mechanic in the Routing Table Approach
Batching a set of indices to be executed at a constant interlude should break the serial dependency chain in the naive routing table approach because it allows the OoO to look ahead and dispatch multiple memory requests to the CPU's LFBs instead of stalling.
But what is the optimal size for this 'Batch' of requests?   

I have searched the web extensively and consulted variuos LLMs about the number of LFBs present in my cpu. Unfortunately i couldnt figure the Exact number , but seems like most search results yielded between 10 and 16. Nevertheless, I have benchmarked and compared different buffer sizes(8,10,16,32,64,128) and found that the most optimal size was 16, which came on top consistently over other batch sizes.
### [The Code Implementation](/Batch_RoutingTable_Approach.cpp)
#### 1. Laying the foundation - batched indexes Buffer
```cpp
    // ... same datastructure as RoutingTable approach.
    const int BATCH_SIZE = 16; // An attempted Guess at amount of LFBs in my CPU(Extensive tests revealed 16 to perform higher throughput consistentently - i9-14900HX).
    uint32_t batchedIndices[BATCH_SIZE]; // local L1 buffer
```
#### 2. the Batched iterations
```cpp
    //... flush cache between iterations

    for (size_t b = 0; b < NUM_CARS; b += BATCH_SIZE) {
            // Calculate if we have a full 16 batch, or just a small "tail" left over. for this project(1milion cars) there will be no tail.
            size_t currentBatchSize = std::min((size_t)BATCH_SIZE, (size_t)NUM_CARS - b);

            // step 1: 1st batch - MLP for indices
            for (size_t i = 0; i < currentBatchSize; i++) {
                batchedIndices[i] = routingTable[plates[b + i]];
            }

            // step 2: MLP gather of objects 
            for (size_t i = 0; i < currentBatchSize; i++) {
                Car* accessedCar = &carStorage[batchedIndices[i]];
                benchmark::DoNotOptimize(accessedCar->timeStamp);
            }
        }
```
### [Google Benchmark Output](/Gathered%20Results/googleBMResult_1.png): best test yielded ~16.1ms per iteration 
While it cannot beat the Linear approach, the throughput achieved has been improved over the *"naive"* Routing Table  approach. [I have gathered multiple Google BenchMark test results](), testing both results - under chaotic and interrupted invornement by the Vtune profiler , and 'neutral' environement (without Vtune profiling) and the results were mostly consistent. The batched approach has achieved better performance even under heavy workload for the CPU. Furthermore, to prove this wasn't just a "Turbo Boost" thermal anomaly, I ran the benchmarks for a sustained 2 seconds to induce thermal throttling, which means that Even under extreme thermal stress and lowered clock speeds, the Batch approach still came on top.
#### [VTune Telemetry](/Gathered%20Results/RES_BATCH.csv) comparison with the naive approach:
| Metric | Routing Table | Batched Routing Table | Microarchitectural Impact |
| :---   | :---  | :---  | :---   | :---   |
|**CPI Rate**| `8.38` |	`3.12` | cycles per inst. dropping to 3.12 means the processor is executing instructions faster due to less pipeline hazards|
|**Front-End Bound**| `~65%` | `~39.9%` | The ~25% decrease indicates the instruction fetch/decode pipeline is stalling the execution less often |
| **Bad Speculation** | `~62.8%` | `0.0%` | ~62.8~% of the pipeline slots were wasted due to branch mispredictions and the subsequent pipeline flushes in the naive approach |
| **L1 Bound** | `~62.1%` | `~15.3%` | ~46.8% Less L1 Cache Stalls. The CPU spent significantly less time stalled waiting for data from the L1 cache |
| **L2 Bound** | `0.0%`| `~1.4%` | L2 cache stalls have increased by ~1.5% in the batched approach |
| **L3 Bound** | `~12.4%` | `0.0%` | *(12.4% -> 0%)* batched requests successfully pulled data closer to the execution units before it was needed |
|**DRAM Bound**| `~82.6%` | `69.2%` | Recovered ~13% of total execution time that was previously lost waiting for DDR5 latency |

 * For the full csv reports refer to *([Routing Table](/Gathered%20Results/RES_ROUTING.csv))* , *([Batched Routing Table](/Gathered%20Results/RES_BATCH.csv))*

# Conclusions and further optimizations
By understanding the hardware's limitations and microarchitecture, simple algorithms can be elevated to accomplish much more. As the telemetry shows, Memory Level Parallelism (MLP) plays a massive role in actual data throughput. 

Time-critical systems utilize this mechanism to achieve high performance, yet "my trick" required me to know my specific CPU model beforehand and tailor the program to explicitly utilize its exact number of Line Fill Buffers (16). 

So, how do time-critical systems like game engines optimize their code for millions of edge devices without knowing the specific hardware limits of each user?

The answer lies in the structure of the data itself. This project was built around an **Array of Structs (AoS)** data structure. To utilize the MLP mechanism efficiently across any hardware without intervening manually, the industry standard is to pivot to a **Struct of Arrays (SoA)** architecture. 

Instead of creating an array of 16-byte `Car` structs, the object is broken down into parallel, flat arrays for each field. For example, the `licensePlate` and `timeStamp` fields would each get their own dedicated array. 
If a system needs to update all timestamps, it iterates purely through the `timeStamp` array. This achieves perfect **Cache Line Density**—every 64-byte fetch from RAM contains only useful data, with no wasted bytes. Because the access pattern is perfectly linear and independent, the CPU's automatic **Hardware Prefetcher** kicks in, taking control of the Load/Store Unit (LSU) and maximizing MLP in the background, perfectly scaling to whatever hardware it happens to be running on.
# Final words
I started this project as a curious second-year software engineering student. 
The whole experiment was born out of a desire to answer my own questions regarding algorithms and deterministic time complexities after receiving some unsatisfying, theoretical answers in the classroom.

In my first approach, I was fairly convinced that my algorithmic assessment was correct. 
I could have stopped there, but instead, I decided to challenge myself to find the absolute physical limit of the problem. 

Perhaps there are even better ways to manipulate or exploit the hardware to achieve higher throughput on AoS data structures, but I am simply not acquainted enough with microarchitecture at that level yet. 

That is not to say I don't want to be! This project has filled me with awe regarding the complex Out-of-Order Engine and the endless invisible mechanisms that work in complete unison to accomplish our day-to-day code. Researching MLP mechanisms has kept me up at night reading technical papers, digging through Git repositories, and emailing professors out of sheer curiosity.

I had an incredible time building this benchmark suite, and I plan to continue researching low-level hardware mechanics in my future projects. I hope reading this repository fuels the same curiosity for you as it did for me.