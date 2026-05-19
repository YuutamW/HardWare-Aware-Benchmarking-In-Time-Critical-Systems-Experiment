#pragma once
#include <benchmark/benchmark.h>
#include <iostream>
#include <cstdint>
#include <vector>
#include <random>
#include <ittnotify.h>
#include <memory>
#include <unordered_set>
#include <algorithm>

struct Car
{
    //9 digits license plate - 4 bytes
    uint32_t licensePlate;

    //Time elapsed since program start in nanoseconds. Ended up Using it by ensuring the lookup succeeded with donotoptimize directive. (misleading var naming).
    uint64_t timeStamp; // 8 bytes
    
    Car(uint32_t lp = 0,uint64_t Time = 0) : licensePlate(lp) , timeStamp(Time) {}

    //c++ compiler will add another 4 bytes of padding to this struct in order to round to 16 bytes.
};


#define GET_DIGIT(LP , POW) ((LP/POW) % 10)// Math to isolate a single 0-9 digit


const uint32_t TARGET_LP = 123456789;
const uint32_t TARGET_INDEX = 42;
const int CACHE_SIZE = 40 * 1024 * 1024; // 40 MB for L3 Flush - tailored for my L3 (~36MB)
const int NUM_CARS = 1000000;

// Helper function to flush the cache
inline void FlushCacheCold() {
    auto cacheTrash = std::make_unique<char[]>(CACHE_SIZE);
    volatile char* rawTrash = cacheTrash.get(); //Extract a raw volatile pointer purely to bypass compiler optimization
    for(int i = 0; i < CACHE_SIZE; i += 64) {
        rawTrash[i] = 1;
    }
    
}

// BATCH GENERATOR: Create 1,000,000 random unique cars
inline std::vector<uint32_t> GenerateRandomTestPlates() {
    std::unordered_set<uint32_t> uniqueSet;

    // Pre-allocate the hash map:
    // This prevents the set from brutally re-allocating and re-hashing 
    // memory every time it grows, saving massive setup time.
    uniqueSet.reserve(NUM_CARS);

    std::mt19937 rng(TARGET_INDEX); // Fixed seed for deterministic benchmarks
    std::uniform_int_distribution<uint32_t> dist(100000000, 999999999);

    

    // Rejection Sampling: Keep generating until we hit exactly 1,000,000
    while (uniqueSet.size() < NUM_CARS) {
        uniqueSet.insert(dist(rng)); // Duplicates are automatically ignored
    }

    // Transfer the guaranteed unique numbers to a contiguous, cache-friendly array
    std::vector<uint32_t> plates(uniqueSet.begin(), uniqueSet.end());

    // Shuffle the vector so the access pattern is more likely to be random
    std::shuffle(plates.begin(), plates.end(), rng);

    return plates;
}