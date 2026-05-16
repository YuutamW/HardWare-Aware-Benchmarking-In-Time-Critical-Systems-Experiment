#include "common.hpp"

/*--Introduction--
 * 1D Routing Table - Data-Oriented Design (4GB)
 * -The Evolution:
 * Even without the math bottleneck, an 8GB array of 8-byte pointers causes heavy L1 
 * cache misses under high traffic. 
 * -This improved approach implements Data-Oriented Design. We use a 4GB flat array of 
 * 4-byte integers (indices) to act as a "Routing Table." These indices point to a 
 * tightly packed, contiguous `carStorage` array. 
 * -The Intuition:
 * By halving the routing data size to 4-bytes, we fit twice as many paths into the 
 * CPU cache line. Furthermore, the contiguous storage array allows the hardware 
 * prefetcher to load data before we even request it.
 * 
 * -The Caveat (The "Cheat"):
 * To isolate the memory bandwidth for this benchmark, we assumed the exact number 
 * of active cars (1,000,000) beforehand to pre-allocate a perfectly tight `carStorage` 
 * array. In a real-world system with 1 billion possible 9-digit plates, guaranteeing 
 * this perfect, continuous density dynamically is impossible without implementing 
 * custom memory arenas or page-based pool allocators. We intentionally traded 
 * dynamic flexibility for absolute hardware speed.
 */

// APPROACH 4: 1D Routing Table (4GB - 4 bytes per slot)

static void BM_RoutingTable(benchmark::State& state) {
    auto plates = GenerateTestPlates();
    auto carStorage = std::make_unique<Car[]>(NUM_CARS);
    
    // Allocate 1 billion 4-byte integers instead of 16-byte objects - 4GB
    auto routingTable = std::make_unique<uint32_t[]>(1000000000);

    for (uint32_t i = 0; i < NUM_CARS; ++i) {
        uint32_t lp = plates[i];
        carStorage[i] = Car(lp, 999);
        routingTable[lp] = i; // Store the index, not the address
    }

    for (auto _ : state) {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        __itt_resume();
        state.ResumeTiming();

        
            for (uint32_t lp : plates) {
                //Step 1: Fetch the 4-byte index
                uint32_t fetchedIndex = routingTable[lp];

                //Step 2: Access the dense array
                Car* accessedCar = &carStorage[fetchedIndex];

                benchmark::DoNotOptimize(accessedCar->timeStamp);
            }
        
    benchmark::ClobberMemory();

    }
}

BENCHMARK(BM_RoutingTable)->Unit(benchmark::kMillisecond)->Iterations(1000);
