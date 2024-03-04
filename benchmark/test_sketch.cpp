#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include "structures/gxj_frequency_sketch.hpp"
#include "structures/count_min_sketch.hpp"
#include "structures/exact.hpp"

#define CACHE_SIZE_GB 389ul
#define BLOCK_SIZE 4*1024ul
#define UNIT_SIZE 1024*1024ul
#define RANGE_SKETCH_CAP CACHE_SIZE_GB * 1024 * 16
#define COUNT_MIN_CAP 3186688
#define EXACT_COUNT_CAP 63753421


int main(void) {
    exact_count<uint64_t> exact(EXACT_COUNT_CAP, BLOCK_SIZE, UNIT_SIZE);
    exact.record_access(0, 0, UNIT_SIZE-BLOCK_SIZE);
    return 0;
}
