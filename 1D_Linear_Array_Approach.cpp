#include "common.hpp"

/*1D Linear Object Array (The Brute Force Baseline)
--Introduction-- 
Our profiling of the 9-Dimensional array proved that heavy arithmetic (division and modulo) destroys the CPU pipeline
before it can even fetch the memory.
To achieve true, unobstructed O(1) access, we must eliminate the GET_DIGIT math entirely.
The most direct solution is to flatten the architecture. Since our maximum license plate value is 999,999,999,
we can allocate a single, massive 1-Dimensional array with 1 billion slots.
By using the 9-digit license plate as the literal index (array[licensePlate]), we reduce the access logic to a single,
native CPU instruction: a base-pointer offset. No division, no traversal—just immediate memory addressing.
*/

static void BM_1D_Linear_Array(benchmark::State& state)
{
    auto plates = GenerateTestPlates();
    auto multiArr = std::make_unique<Car[]>(1000000000ULL); // allocate 1 Bilion cars
    for (uint32_t lp : plates) // Fill 1 milion random plates into the database.
    {
        multiArr[lp] = Car(lp, 999);
    }
    for (auto _ : state)
    {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        state.ResumeTiming();
        __itt_resume();

        for (uint32_t lp : plates)
        {
            Car& accessedCar = multiArr[lp]; // access the car directly in the array - 1 memory request.
            benchmark::DoNotOptimize(accessedCar.timeStamp);
        }
        
        benchmark::ClobberMemory();
    }
    
}
BENCHMARK(BM_1D_Linear_Array)->Unit(benchmark::kMillisecond)->Iterations(1000);