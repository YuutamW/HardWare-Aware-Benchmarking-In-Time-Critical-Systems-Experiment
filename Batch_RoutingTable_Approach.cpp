#include "common.hpp"



/*Batch Routing Table Approach:
*--Introduction-- 
* After benchmarking the Routing table approach and the 1D_Linear
* The Data speaks for itself, the Routing Table managed to access the car within
* ~22ns; That means that the CPU didnt entirely stall for the entire Load process (given that DRAM is ~80ns) 
* BUT unsurprisingly, the absolute winner is the linear array of object(16GB). 
* yet not for the reason Iwould think. The linear array managed to access the object within ~2-3ns. 
* This means that possibly there was no DRAM LOAD operand. but that does not make sense... 
* Turns out, the MLP(Memory Level Parallelism) mechaninc came into affect: 
* The CPU realised the addresses of the cars in the loop are independent of each other, and therefore 
* sent multiple memory requests and accomplishes a "pipeline" of accessed cars around the loop. 
* What that basically means, that the cpu did 'technically' stall for 80ns while the car was being fetched. 
* But the cars were fetched in parallel resulting in an ~2-3ns per lookup. 
* 
* So How can we combine the Two Approaches to elevate the lookup time? 
* The problem with the routing table was that the data was dependant on each other: 
* SCENARIO ROUTING TABLE: LOOKUP CAR A
* 1. fetch index of car A - mem request in routingTable[A.lp](80ns). 
* 2. use fetched index to fetch car A - cannot begin until step 1 is complete(stall).
* 
* But what if we use the MLP mechanic to elevate the lookup between each 2 steps? 
* within each two steps, the data is dependant. 
* which means that for every third step  the data is Independant... 
* So the approach is to manipulate the MLP mechanic in order to "batch" multiple memory requests
* for the index array, Then we batch multiple memory requests constrained by the return value of step 1.
*/

static void BM_BATCH_RoutingTable(benchmark::State& state) {
    auto plates = GenerateRandomTestPlates();
    
    auto carStorage = std::make_unique<Car[]>(NUM_CARS);
    
    // Allocate 1 billion 4-byte integers instead of 8-byte pointers
    auto routingTable = std::make_unique<uint32_t[]>(1000000000);
    
    for (uint32_t i = 0; i < NUM_CARS; ++i) {
        uint32_t lp = plates[i];
        carStorage[i] = Car(lp, 999);
        routingTable[lp] = i; // Store the index, not the address
    }
    
    const int BATCH_SIZE = 16; // An attempted Guess at amount of LFBs in my CPU(Extensive tests revealed 16 to perform higher throughput consistentently - i9-14900HX).
    uint32_t batchedIndices[BATCH_SIZE]; // local L1 buffer
    
    for (auto _ : state) {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        state.ResumeTiming();
        __itt_resume();

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
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_BATCH_RoutingTable)->Name("Batch_RoutingTable")->Unit(benchmark::kMillisecond)->Iterations(1000);