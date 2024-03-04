#include <benchmark/benchmark.h>
#include <iostream>
#include <random>
#include "structures/gxj_frequency_sketch.hpp"

#define CACHE_SIZE_GB 389ul
#define BLOCK_SIZE 4*1024ul
#define UNIT_SIZE 1024*1024ul
#define RANGE_SKETCH_CAP CACHE_SIZE_GB * 1024 * 16
#define MAX_LBA 4178143346688ul

// op1: avg_frequency(size_t vid, off64_t t, off64_t size)
// op2: record_access(size_t vid, off64_t t, off64_t size)

// 定义 Google Benchmark 测试
static void BM_RangeSketchAccess(benchmark::State& state) {
    state.PauseTiming();
    
    auto size = static_cast<uint64_t>(state.range(0));
    gxj_range_frequency_sketch<uint64_t> range_sketch(RANGE_SKETCH_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    std::random_device rd;  // 用于获取真正的随机数种子
    std::mt19937 gen(rd());
    uint64_t min_value = std::numeric_limits<uint64_t>::min();
    uint64_t max_value = MAX_LBA;
    std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
    

    state.ResumeTiming();

    for (auto _ : state) {
        auto in_volume_offset = distribution(gen);
        range_sketch.record_access(0, in_volume_offset, size);
    }

    // 设置测试参数范围和单位
    state.SetComplexityN(state.range(0));
}

// 注册测试，设置测试参数范围
// BENCHMARK(BM_RangeSketchAccess)->DenseRange(BLOCK_SIZE, UNIT_SIZE, BLOCK_SIZE)->Repetitions(100)->ReportAggregatesOnly();

static void BM_RangeSketchFrequency(benchmark::State& state) {
    state.PauseTiming();
    
    auto size = static_cast<uint64_t>(state.range(0));
    gxj_range_frequency_sketch<uint64_t> range_sketch(RANGE_SKETCH_CAP, BLOCK_SIZE, UNIT_SIZE);
    // TODO: inject
    std::random_device rd;  // 用于获取真正的随机数种子
    std::mt19937 gen(rd());
    uint64_t min_value = std::numeric_limits<uint64_t>::min();
    uint64_t max_value = MAX_LBA;
    std::uniform_int_distribution<uint64_t> distribution(min_value, max_value);
    

    state.ResumeTiming();

    for (auto _ : state) {
        auto in_volume_offset = distribution(gen);
        auto result = range_sketch.avg_frequency(0, in_volume_offset, size);
	benchmark::DoNotOptimize(result);
    }
}

// 注册测试，设置测试参数范围
// BENCHMARK(BM_RangeSketchFrequency)->DenseRange(BLOCK_SIZE, UNIT_SIZE, BLOCK_SIZE)->Repetitions(100)->ReportAggregatesOnly();
BENCHMARK(BM_RangeSketchFrequency)->Arg(1044480)->Repetitions(100)->ReportAggregatesOnly();
// BENCHMARK(BM_RangeSketchFrequency)->Arg(1048575)->Repetitions(100)->ReportAggregatesOnly();


BENCHMARK_MAIN();
