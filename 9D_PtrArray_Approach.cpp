#include "common.hpp"

/* * APPROACH 2: 9-Dimensional Pointer Array (8GB)
 * -The Evolution:
 * The first approach used 16GB because it stored the actual 16-byte Car objects 
 * inside the massive grid. To relieve the physical memory footprint, this approach 
 * stores 8-byte pointers instead, dynamically allocating the objects on the heap.
 * -The Catch:
 * While we cut the memory requirement in half (8GB), we introduce "Pointer Chasing."
 * The CPU must now fetch the pointer from the array, and then jump to a random, 
 * fragmented location on the heap to find the actual data.
 */

//APPROACH 2: MultiArrayApproach (8GB Pointer Array)
using PtrArray = Car*[10][10][10][10][10][10][10][10][10];

static void BM_9D_PtrArray_Approach(benchmark::State& state) {
    auto plates = GenerateRandomTestPlates();
    
    auto multiArr = new PtrArray();
    std::vector<Car*> allocatedCars;

    for (uint32_t lp : plates) {
        Car* newCar = new Car(lp, 999);
        allocatedCars.push_back(newCar);
        multiArr
        [GET_DIGIT(lp, 100000000)]
        [GET_DIGIT(lp, 10000000)]
        [GET_DIGIT(lp, 1000000)]
        [GET_DIGIT(lp, 100000)]
        [GET_DIGIT(lp, 10000)]
        [GET_DIGIT(lp, 1000)]
        [GET_DIGIT(lp, 100)]
        [GET_DIGIT(lp, 10)]
        [GET_DIGIT(lp, 1)] = newCar;

    }

    for (auto _ : state) {
        state.PauseTiming();
        __itt_pause();
        FlushCacheCold();
        state.ResumeTiming();
        __itt_resume();
        for (uint32_t lp : plates)  {
            Car* accessedCar = multiArr
            [GET_DIGIT(lp, 100000000)]
            [GET_DIGIT(lp, 10000000)]
            [GET_DIGIT(lp, 1000000)]
            [GET_DIGIT(lp, 100000)]
            [GET_DIGIT(lp, 10000)]
            [GET_DIGIT(lp, 1000)]
            [GET_DIGIT(lp, 100)]
            [GET_DIGIT(lp, 10)]
            [GET_DIGIT(lp, 1)];

            benchmark::DoNotOptimize(accessedCar->timeStamp);
        }
    benchmark::ClobberMemory();
    }
    delete[] multiArr;
    for (Car* c : allocatedCars) delete c;
}
BENCHMARK(BM_9D_PtrArray_Approach)->Unit(benchmark::kMillisecond)->Iterations(5000);
