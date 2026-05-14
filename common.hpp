#pragma once
#include <benchmark/benchmark.h>
#include <iostream>
#include <cstdint>
#include <vector>
#include <random>
#include <ittnotify.h>


struct Car
{
    //9 digits license plate - 4 bytes
    uint32_t licensePlate;

    //Time elapsed since program start in nanoseconds
    uint64_t timeStamp;
    
    Car(uint32_t lp = 0,uint64_t Time = 0) : licensePlate(lp) , timeStamp(Time) {}

    //c++ compiler will add another 4 bytes of padding to this struct in order to round to 16 bytes.
};


#define GET_DIGIT(LP , POW) ((LP/POW) % 10)// Math to isolate a single 0-9 digit


const uint32_t TARGET_LP = 123456789;
const uint32_t TARGET_INDEX = 42;
const int CACHE_SIZE = 40 * 1024 * 1024; // 40 MB for L3 Flush - tailored for my L3
const int NUM_CARS = 1000000;

// Helper function to flush the cache
inline void FlushCacheCold() {
    volatile char* cacheTrash = new char[CACHE_SIZE];
    for(int i = 0; i < CACHE_SIZE; i += 64) {
        cacheTrash[i] = 1; 
    }
    delete[] cacheTrash;
}

// BATCH GENERATOR: Create 1,000,000 random unique cars
inline std::vector<uint32_t> GenerateTestPlates() {
    std::vector<uint32_t> plates;
    std::mt19937 rng(42); // Fixed seed for fair comparison across tests
    std::uniform_int_distribution<uint32_t> dist(100000000, 999999999);
    
    for(int i = 0; i < NUM_CARS; ++i) {
        plates.push_back(dist(rng));
    }
    return plates;
}