#include <benchmark/benchmark.h>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include "structures/gxj_frequency_sketch.hpp"
#include "structures/count_min_sketch.hpp"
#include "structures/exact.hpp"
#include "structures/interval_sketch.hpp"

#define CACHE_SIZE_GB 389ul
#define BLOCK_SIZE 4*1024ul
#define UNIT_SIZE 1024*1024ul
#define RANGE_SKETCH_CAP CACHE_SIZE_GB * 1024 * 16
#define COUNT_MIN_CAP 3186688
#define EXACT_COUNT_CAP 63753421
#define INTERVAL_SKETCH_CAP CACHE_SIZE_GB * 1024 * 16

// op1: avg_frequency(size_t vid, off64_t t, off64_t size)
// op2: record_access(size_t vid, off64_t t, off64_t size)
static void BM_IntervalSketchAccess(benchmark::State& state) {
    // state.PauseTiming();

    auto size = static_cast<uint64_t>(state.range(0));
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
	interval_sketch<uint64_t> i_sketch(INTERVAL_SKETCH_CAP, BLOCK_SIZE, UNIT_SIZE);

    // state.ResumeTiming();;
    uint64_t t = UNIT_SIZE;

    for (auto _ : state) {
        // auto in_volume_offset = distribution(gen);
        i_sketch.record_access(0, t, size);
		t += UNIT_SIZE;
    }

}

// BENCHMARK(BM_CMSketchAccess)->RangeMultiplier(2)->Range(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE);
// BENCHMARK(BM_IntervalSketchAccess)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);
// BENCHMARK(BM_CMSketchAccess)->RangeMultiplier(2)->Range(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE)->Repetitions(10)->ReportAggregatesOnly();

static void BM_IntervalSketchFrequency(benchmark::State& state) {
    // state.PauseTiming();
    
    auto size = static_cast<uint64_t>(state.range(0));
    interval_sketch<uint64_t> i_sketch(COUNT_MIN_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
    

    // state.ResumeTiming();;
	uint64_t t = UNIT_SIZE;

    for (auto _ : state) {
        // auto in_volume_offset = distribution(gen);
        auto result = i_sketch.avg_frequency(0, t, size);
		benchmark::DoNotOptimize(result);
		t += UNIT_SIZE;
    }
}

// BENCHMARK(BM_IntervalSketchFrequency)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);

static void BM_CMSketchAccess(benchmark::State& state) {
    // // state.PauseTiming();

    auto size = static_cast<uint64_t>(state.range(0));
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
	count_min_sketch<uint64_t> count_min_sketch(COUNT_MIN_CAP, BLOCK_SIZE, UNIT_SIZE);

    // // state.ResumeTiming();;
    uint64_t t = UNIT_SIZE;

    for (auto _ : state) {
        // auto in_volume_offset = distribution(gen);
        count_min_sketch.record_access(0, t, size);
		t += UNIT_SIZE;
    }

}

// BENCHMARK(BM_CMSketchAccess)->RangeMultiplier(2)->Range(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE);
// BENCHMARK(BM_CMSketchAccess)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);
// BENCHMARK(BM_CMSketchAccess)->RangeMultiplier(2)->Range(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE)->Repetitions(10)->ReportAggregatesOnly();

static void BM_CMSketchFrequency(benchmark::State& state) {
    // state.PauseTiming();
    
    auto size = static_cast<uint64_t>(state.range(0));
    count_min_sketch<uint64_t> count_min_sketch(COUNT_MIN_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
    

    // state.ResumeTiming();;
	uint64_t t = UNIT_SIZE;

    for (auto _ : state) {
        // auto in_volume_offset = distribution(gen);
        auto result = count_min_sketch.avg_frequency(0, t, size);
		benchmark::DoNotOptimize(result);
		t += UNIT_SIZE;
    }
}

// BENCHMARK(BM_CMSketchFrequency)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);

static void BM_RangeSketchAccess(benchmark::State& state) {
    // state.PauseTiming();

    auto size = static_cast<uint64_t>(state.range(0));
    gxj_range_frequency_sketch<uint64_t> range_sketch(RANGE_SKETCH_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);

    // state.ResumeTiming();;

	uint64_t t = UNIT_SIZE;

    for (auto _ : state) {
        range_sketch.record_access(0, t, size);
		t += UNIT_SIZE;
    }
}

// BENCHMARK(BM_RangeSketchAccess)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);

static void BM_RangeSketchFrequency(benchmark::State& state) {
    // state.PauseTiming();
    auto size = static_cast<uint64_t>(state.range(0));
    gxj_range_frequency_sketch<uint64_t> range_sketch(RANGE_SKETCH_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
    

    // state.ResumeTiming();;
	uint64_t t = UNIT_SIZE;
    for (auto _ : state) {
        // auto in_volume_offset = distribution(gen);
        auto result = range_sketch.avg_frequency(0, t, size);
		benchmark::DoNotOptimize(result);
        t += UNIT_SIZE;
    }
}

// BENCHMARK(BM_RangeSketchFrequency)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);

static void BM_ExactAccess(benchmark::State& state) {
    // state.PauseTiming();

    auto size = static_cast<uint64_t>(state.range(0));
    exact_count<uint64_t> exact(EXACT_COUNT_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);

    // state.ResumeTiming();;

	uint64_t t = UNIT_SIZE;

    for (auto _ : state) {
        if ((t+size)/BLOCK_SIZE > EXACT_COUNT_CAP*16) {
            t = UNIT_SIZE;
        }
        exact.record_access(0, t, size);
        t += UNIT_SIZE;
    }
}

BENCHMARK(BM_ExactAccess)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);

static void BM_ExactFrequency(benchmark::State& state) {
    // state.PauseTiming();
    
    auto size = static_cast<uint64_t>(state.range(0));
    exact_count<uint64_t> exact(EXACT_COUNT_CAP, BLOCK_SIZE, UNIT_SIZE);

    // TODO: inject
    // std::random_device rd;  // 用于获取真正的随机数种子
    // std::mt19937 gen(rd());
    // uint64_t min_value = std::numeric_limits<uint64_t>::min();
    // uint64_t max_value = std::numeric_limits<uint64_t>::max();
    // std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
    

    // state.ResumeTiming();;
	uint64_t t = UNIT_SIZE;
    for (auto _ : state) {
        // auto in_volume_offset = distribution(gen);
        if ((t+size)/BLOCK_SIZE > EXACT_COUNT_CAP*16) {
            t = UNIT_SIZE;
        }
        auto result = exact.avg_frequency(0, t, size);
		benchmark::DoNotOptimize(result);
		t += UNIT_SIZE;
    }
}

BENCHMARK(BM_ExactFrequency)->DenseRange(BLOCK_SIZE, UNIT_SIZE-BLOCK_SIZE, BLOCK_SIZE);
// BENCHMARK(BM_ExactFrequency)->Arg(BLOCK_SIZE);

BENCHMARK_MAIN();
