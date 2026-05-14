#include "common.hpp"


// APPROACH 3: OneD_ArrayApproach (8GB Flat Pointer Array)
static void BM_OneDArrayPtr(benchmark::State& state) {
    
    auto plates = GenerateTestPlates();
    // Allocate 1 billion pointers (8GB)
    Car** carArr = new Car*[1000000000](); 
    std::vector<Car*> allocatedCars;
    for (uint32_t lp : plates) {
        Car* newCar = new Car(lp, 999);
        allocatedCars.push_back(newCar);
        carArr[lp] = newCar;
    }

    for (auto _ : state) {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        __itt_resume();
        state.ResumeTiming();

        for (uint32_t lp : plates) {
            Car* accessedCar = carArr[lp];
            benchmark::DoNotOptimize(accessedCar->timeStamp);
        }
        benchmark::ClobberMemory();
    }
    delete[] carArr;
    for (Car* c : allocatedCars) delete c;
}
BENCHMARK(BM_OneDArrayPtr)->Unit(benchmark::kMillisecond)->Iterations(5000);
