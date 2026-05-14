#include "common.hpp"

static void BM_1D_Linear_Array(benchmark::State& state)
{
    auto plates = GenerateTestPlates();
    Car* multiArr = new Car[1000000000ULL]();
    for (uint32_t lp : plates)
    {
        multiArr[lp] = Car(lp, 999);
    }
    for (auto _ : state)
    {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        __itt_resume();
        state.ResumeTiming();

        for (uint32_t lp : plates)
        {
            Car& accessedCar = multiArr[lp];
            benchmark::DoNotOptimize(accessedCar.timeStamp);
        }
        
        benchmark::ClobberMemory();
    }
    delete[] multiArr;
}
BENCHMARK(BM_1D_Linear_Array)->Unit(benchmark::kMillisecond)->Iterations(1000);