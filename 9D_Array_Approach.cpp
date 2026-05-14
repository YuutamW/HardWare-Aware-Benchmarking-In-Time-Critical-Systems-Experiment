#include "common.hpp"

/*
 * APPROACH 1: 9-Dimensional Object Array (16GB)
 * -The Intuition: 
 * In algorithmic theory, array lookups are O(1). By using the 9 digits of a 
 * license plate as direct indices for a 9D array, we theoretically achieve 
 * instant access without searching.
 * -The Catch:
 * This allocates a staggering 16 Gigabytes of continuous memory. We are putting 
 * the hardware to the test to see if theoretical O(1) time holds up against 
 * extreme TLB (Translation Lookaside Buffer) pressure and massive L1 cache footprint.
 */

//APPROACH 1: MultiArrRAM (16GB Object Array)
using ObjArray = Car[10][10][10][10][10][10][10][10][10];

static void BM_9D_OBJ_Array(benchmark::State& state) {
    auto plates = GenerateTestPlates();
    auto multiArr = new ObjArray();
    for (uint32_t lp : plates) {
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
    for(auto _ : state) {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        __itt_resume();
        state.ResumeTiming();
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
    delete[] multiArr;
}
BENCHMARK(BM_9D_OBJ_Array)->Unit(benchmark::kMillisecond)->Iterations(5000);