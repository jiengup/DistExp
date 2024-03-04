/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GXJ_FREQUENCY_SKETCH_HEADER
#define GXJ_FREQUENCY_SKETCH_HEADER

#include "detail.hpp"

#include <vector>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <unordered_map>
#include <cassert>
#include <thread>

/**
 * A probabilistic set for estimating the popularity (frequency) of an element within an
 * access frequency based time window. The maximum frequency of an element is limited
 * to 15 (4-bits).
 *
 * NOTE: the capacity will be the nearest power of two of the input capacity (for various
 * efficiency and hash distribution gains).
 *
 * This is a slightly altered version of Caffeine's implementation:
 * https://github.com/ben-manes/caffeine
 *
 * The white paper:
 * http://dimacs.rutgers.edu/~graham/pubs/papers/cm-full.pdf
 */
template<typename T>
class gxj_range_frequency_sketch
{
    // Holds 64 bit blocks, each of which holds sixteen 4 bit counters. For simplicity's
    // sake, the 64 bit blocks are partitioned into four 16 bit sub-blocks, and the four
    // counters corresponding to some T is within a single such sub-block.
    // std::vector<std::vector<uint64_t>> dyadic_counters_;
    // std::vector<std::vector<uint64_t>> lazy_counters_;
    // 一个bucket有8个counter，其中前4个给dyadic_counters_，后4个为lazy_counters_
    std::vector<std::vector<uint64_t>> mix_counters_;


    int counter_level_;


    // Incremented with each call to record_access, if the frequency of the item could
    // be incremented, and halved when sampling size is reached.
    int size_;

    int ref_cnt_;

    int capacity_;

    int soft_reset_idx_;

    bool doing_reset = false;


    T granularity;
    T unit_size;

public:
    explicit gxj_range_frequency_sketch(int capacity, T granularity, T unit_size):
    granularity(granularity),
    unit_size(unit_size)
    {
        change_capacity(capacity, granularity, unit_size);
    }

    void change_capacity(const int n, const T granularity, const T unit_size)
    {
        if(n <= 0)
        {
            throw std::invalid_argument("frequency_sketch capacity must be larger than 0");
        }
        counter_level_ = log2(sketchdetail::nearest_power_of_two(unit_size/granularity))+1;

        // printf("counter_level_: %d\n", counter_level_);

        // 重置dyalic_counters_ 大小为0
        // dyadic_counters_.clear();
        // lazy_counters_.clear();
        mix_counters_.clear();
        int table_size = sketchdetail::nearest_power_of_two(n);

        for(int i = 0; i < counter_level_; i++){
            // dyadic_counters_.push_back(std::vector<uint64_t>(table_size, 0));
            // lazy_counters_.push_back(std::vector<uint64_t>(table_size, 0));
            mix_counters_.push_back(std::vector<uint64_t>(table_size, 0));
            table_size/=2;
        }

        soft_reset_idx_ = 0;
        size_ = 0;
        ref_cnt_ = 0;
        capacity_ = n;
    }

    bool contains(const T& t) const noexcept
    {
        return frequency(t) > 0;
    }

    int avg_frequency(size_t unit_id, const T& t, const T& size) const noexcept
    {
        int left = (t%unit_size)/granularity;
        int right = ((t + size)%unit_size)/granularity;
        int cnt = right - left;
        

        int sum_frequency = frequency(unit_id, t, size);

        return sum_frequency/cnt;
    }

    int frequency(size_t unit_id, const T& t, const T& size) const noexcept
    {
        // 首先计算此次访问涉及的多少个粒度
        int sum_frequency = 0;
        int left = (t%unit_size)/granularity;
        int right = ((t + size)%unit_size)/granularity;
        int base = (t/unit_size)*((unit_size + granularity - 1)/granularity);
        int cnt = right - left;
        int start = 0;
        int end = (unit_size + granularity - 1)/granularity;

        // printf("left %d, right %d\n", left, right);

        sum_frequency = inner_frequency(unit_id, base, left, right, 0, end, counter_level_);

        return sum_frequency;
    }
    
    int inner_frequency(size_t unit_id, const int base, const int left, const int right, const int start, const int end, const int upper_level) const noexcept
    {
        // printf("inner_frequency: %d %d %d %d %d\n", left, right, start, end, upper_level);
        assert(start <= left && right <= end);
        int sum_frequency = 0;

        int lazy_add_cnt = get_lazy_add_cnt(unit_id, base, start, end, upper_level-1);
        sum_frequency += lazy_add_cnt * (right - left);

        if(left == start && right == end){
            // 说明刚好覆盖范围
            int dyadic_cnt = get_dyadic_cnt(unit_id, base, start, end, upper_level-1);
            sum_frequency += dyadic_cnt;

            // printf("dyadic_cnt = %d, lazy_add_cnt = %d, range = %d\n", dyadic_cnt, lazy_add_cnt, right - left);

            return  sum_frequency;
        }
        int mid = start + (end - start)/2;

        if (mid > left){
            // 说明左边部分需要计算
            sum_frequency += inner_frequency(unit_id, base, left, std::min(mid, right), start, mid, upper_level-1);
        }
        if(mid < right){
            // 右边部分需要计算
            sum_frequency += inner_frequency(unit_id, base, std::max(left, mid), right, mid, end, upper_level-1);
        }

        return sum_frequency;
    }

    
    // TODO: 
    int get_lazy_add_cnt(size_t unit_id, const int base, const int start, const int end, const int level) const noexcept{
        int range = end - start;
        // printf("level = %d, range = %d\n", level, range);
        // 查询level层的start

        int block_frequency = std::numeric_limits<int>::max();
        const uint32_t hash = sketchdetail::hash(unit_id, base + start);
        for(int j = 0; j < 4; ++j){
            block_frequency = std::min(block_frequency, get_count(mix_counters_[level], true, hash, j));
        }
        // printf("get_lazy_add_cnt at %d %d\n", hash , level);
        return block_frequency;
    }

    // TODO: 
    int get_dyadic_cnt(size_t unit_id, const int base, const int start, const int end, const int level) const noexcept{
        int range = end - start;
        // 查询level层的start
        int block_frequency = std::numeric_limits<int>::max();
        const uint32_t hash = sketchdetail::hash(unit_id, base + start);
        for(int j = 0; j < 4; ++j){
            block_frequency = std::min(block_frequency, get_count(mix_counters_[level], false, hash, j));
        }
        return block_frequency * range;
    }

    void record_access(const size_t unit_id, const T& t, const T& size) noexcept
    {
	if(doing_reset) return;
        // 首先计算此次访问涉及的多少个粒度
        int left = (t%unit_size)/granularity;
        int right = ((t + size)%unit_size)/granularity;
        int base = (t/unit_size)*((unit_size + granularity - 1)/granularity);
        int cnt = right - left;
        int start = 0;
        int end = (unit_size + granularity - 1)/granularity;
        ref_cnt_ += 1;

        inner_record_access(unit_id, base, left, right, 0, end, counter_level_);
    }
    
    // return true with Prob = (right - left)/(end-start)
    bool try_est_add(size_t unit_id, const int base, const int left, const int right, const int start, const int end){
        // const uint32_t hash = sketchdetail::hash(unit_id , size_+10000019);
        const uint32_t hash = ref_cnt_;
        int mask = ((end-start) - 1);
        int rand_v = (hash & mask);
        if (rand_v < (right - left)){
            // printf("True,  %d/%d\n", (right - left), (end-start));
            return true;
        }
        // printf("False, %d/%d\n", (right - left), (end-start));
        return false;
    }

    void inner_record_access(size_t unit_id, const int base, const int left, const int right, const int start, const int end, const int upper_level) noexcept{
        assert(start <= left && right <= end);

        // printf("inner_record_access: %d %d %d %d\n", left, right, start, end);

        if(left == start && right == end){
            // 说明刚好覆盖范围
            inc_lazy_add_cnt(unit_id, base, start, end, upper_level - 1);
            // int lazy_add_cnt = get_lazy_add_cnt(unit_id, base, start, end);

            // int dyadic_cnt = get_dyadic_cnt(unit_id, base, start, end);
            // printf("dyadic_cnt = %d, lazy_add_cnt = %d\n", dyadic_cnt, lazy_add_cnt);
            return;
        }
        // 以(right - left)/(end-start)的概率+1
        if(try_est_add(unit_id, base, left, right, start, end)){
            inc_dyadic_cnt(unit_id, base, start, end, 1, upper_level - 1);
        }
        // inc_dyadic_cnt(unit_id, base, start, end, right - left);
        int mid = start + (end - start)/2;

        if (mid > left){
            // 说明左边部分需要计算
            inner_record_access(unit_id, base, left, std::min(mid, right), start, mid, upper_level - 1);
        }
        if(mid < right){
            // 右边部分需要计算
            inner_record_access(unit_id, base, std::max(left, mid), right, mid, end, upper_level - 1);
        }
    }

    // TODO: 
    void inc_lazy_add_cnt(size_t unit_id, const int base, const int start, const int end, const int level){
        int range = end - start;
        // 查询level层的start
        // printf("inc_lazy_add_cnt: %d %d\n", start, end);
        
        bool was_added = false;
        const uint32_t hash = sketchdetail::hash(unit_id, base + start);
        for(int j = 0; j < 4; ++j){
            was_added |= try_increment_counter_at_with_value(mix_counters_[level], true, hash, j, 1);
        }
        if(was_added){
            // printf("was_added at %d %d\n", hash , level);
            size_ += range;
        }
    }

    // TODO: 
    void inc_dyadic_cnt(size_t unit_id, const int base, const int start, const int end, const int value, const int level){
        int range = end - start;
        // printf("level = %d, range = %d\n", level, range);
        // 查询level层的start

        bool was_added = false;
        const uint32_t hash = sketchdetail::hash(unit_id, base + start);
        for(int j = 0; j < 4; ++j){
            was_added |= try_increment_counter_at_with_value(mix_counters_[level], false, hash, j, value);
        }
        if(was_added){
            size_ += value;
        }
    }

    // TODO: 
    int get_count(const std::vector<uint64_t> & table, bool is_lazy, const uint32_t hash, const int counter_index)const noexcept{
        const int index = table_index(table, hash, counter_index);
        const int offset = counter_offset(hash, counter_index, is_lazy);
        return (table[index] >> offset) & 0xfL;
    }

    // TODO: 
    /**
     * Increments ${counter_index}th counter by inc_value if it's below the maximum value (15).
     * Returns true if the counter was incremented.
     */
    bool try_increment_counter_at_with_value(std::vector<uint64_t> & table, bool is_lazy, const uint32_t hash, const int counter_index, const int inc_value){
        const int index = table_index(table, hash, counter_index);
        const int offset = counter_offset(hash, counter_index, is_lazy);
        int now_value = (table[index] >> offset) & 0xfL;
        // printf("now value : %d\n", now_value);
        
        if(now_value < 0xfL)
        {
            now_value += inc_value;
            if(now_value >= 0xfL){
                // printf("aa\n");
                table[index] |= 0xfL << offset;
            }
            else{
                // printf("bb\n");
                table[index] += (1L*inc_value) << offset;
            }
            int after_value = (table[index] >> offset) & 0xfL;
            // printf("after value : %d\n", after_value);
            // printf("inc_value = %d, offset = %d\n", inc_value, offset);
            return true;
        }
        return true;
    }

    void soft_reset(){
        
        int reset_idx = soft_reset_idx_;
        for(auto & table: mix_counters_){
            for(auto& counters : table)
            {
                // Do a 'bitwise_and' on each (4 bit) counter with 0111 (7) so as to
                // eliminate the bit that got shifted over from the counter to the left to
                // the leftmost position of the current counter.
                counters = (counters >> 1) & 0x7777777777777777L;
            }
        }
        size_ /= 2;
        ref_cnt_ /= 2;

    }

    bool try_reset(){
        if(size_ >= sampling_size()){
	    doing_reset = true;
	    auto process = [this]() {
                // 在这里编写你的代码逻辑
		reset();
		doing_reset = false;
            };

            // 创建线程对象并启动线程
    	    std::thread thread(process);

            // 分离线程
            thread.detach();
            return true;
        }
        return false;
    }

    void must_reset(){
        reset(); // 外部要感知这个reset
    }

    

private:

    /**
     * Returns the table index where the counter associated with $hash at
     * $counter_index resides (since each item is mapped to four different counters in
     * $table_, an index is necessary to differentiate between each).
     */
    int table_index(const std::vector<uint64_t> & table, const uint32_t hash, const int counter_index) const noexcept
    {
        static constexpr uint64_t seeds[] = {
            0xc3a5c85c97cb3127L,
            0xb492b66fbe98f273L,
            0x9ae16a3b2f90404fL,
            0xcbf29ce484222325L 
        };
        uint64_t h = seeds[counter_index] * hash;
        h += h >> 32;
        return h & (table.size() - 1);

        // uint64_t h = seeds[counter_index] * hash;
        // h += h >> 32;
        // return h & (table.size() - 1);
    }


    /**
     * $table_ holds 64 bit blocks, while counters are 4 bit wide, i.e. there are 16
     * counters in a block.
     * This function determines the start offset of the ${counter_index}th counter
     * associated with $hash.
     * Offset may be [0, 60] and is a multiple of 4. $counter_index must be [0, 3].
     */
    int counter_offset(const uint32_t hash, const int counter_index, bool is_lazy) const noexcept
    {
        int offset = (offset_multiplier(hash) + counter_index) << 2;
        if(is_lazy){
            offset += 32;
        }
        // printf("is_lazy %d, counter_offset = %d\n", is_lazy, offset);
        return offset;
    }

    /**
     * $table_ holds 64 bit blocks, and each block is partitioned into four 16 bit
     * parts, starting at 0, 16, 32 and 48. Each part is further divided into four 4 bit
     * sub-parts (e.g. 0, 4, 8, 12), which are the start offsets of the counters.
     *
     * All counters of an item are within the same logical 16 bit part (though most
     * likely not in the same 64 bit block if the hash does its job). Which 16 bit part
     * an item is placed into is determined by its two least significant bits, which
     * this function determines.
     *
     * The return value may be 0, 4.
     */
    int offset_multiplier(const uint32_t hash) const noexcept
    {
        return (hash & 1) << 2;
    }

    /** Halves every counter and adjusts $size_. */
    void reset() noexcept
    {
        for(auto & table: mix_counters_){
            for(auto& counters : table)
            {
                // Do a 'bitwise_and' on each (4 bit) counter with 0111 (7) so as to
                // eliminate the bit that got shifted over from the counter to the left to
                // the leftmost position of the current counter.
                counters = (counters >> 1) & 0x7777777777777777L;
            }
        }
        size_ /= 2;
        ref_cnt_ /= 2;
    }

    /**
     * The reset operation is launched when $size_ reaches the value returned by this
     * function.
     */
    int sampling_size() const noexcept
    {
        return capacity_ * 2;
    }
};

#endif
