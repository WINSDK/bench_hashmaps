#pragma once

#include "base.h"

#include <algorithm>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

template <typename T>
concept TableTrait =
    requires {
        typename T::Key;
        typename T::Value;

        { T::hash(std::declval<typename T::Key>()) } -> std::convertible_to<uint64_t>;
    } && std::is_trivially_copyable_v<typename T::Key> &&
    std::is_trivially_copyable_v<typename T::Value>;

template <TableTrait TableTrait, uint64_t BUCKET>
struct TwoWayBaseline {
    using Key = typename TableTrait::Key;
    using Value = typename TableTrait::Value;

    static constexpr Key EMPTY = std::numeric_limits<Key>::max();

    TwoWayBaseline() : size_(0), capacity(8) {
        data = reinterpret_cast<Slot*>(__aligned_alloc(CACHE_LINE, sizeof(Slot) * capacity));
        fill_empty(data, capacity);
    }
    ~TwoWayBaseline() {
        __aligned_free(data);
    }

    // assumes key is not in the map
    void insert(Key key, Value value) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        uint64_t n_1 = 0;
        uint64_t n_2 = 0;
        while (slot_1->keys[n_1] < EMPTY && ++n_1 < BUCKET)
            ;
        while (slot_2->keys[n_2] < EMPTY && ++n_2 < BUCKET)
            ;
        if (n_1 == BUCKET && n_2 == BUCKET) {
            grow();
            insert(key, value);
            return;
        }
        if (n_1 <= n_2) {
            slot_1->keys[n_1] = key;
            slot_1->values[n_1] = value;
        } else {
            slot_2->keys[n_2] = key;
            slot_2->values[n_2] = value;
        }
        size_++;
    }

    Value find(Key key, uint64_t* steps) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t idx = 0;; idx++) {
            if (slot_1->keys[idx] == key)
                return slot_1->values[idx];
            (*steps)++;
            if (slot_2->keys[idx] == key)
                return slot_2->values[idx];
            (*steps)++;
        }
    }

    bool contains(Key key, uint64_t* steps) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t idx = 0;
             idx < BUCKET && (slot_1->keys[idx] != EMPTY || slot_2->keys[idx] != EMPTY);
             idx++) {
            if (slot_1->keys[idx] == key)
                return true;
            (*steps)++;
            if (slot_2->keys[idx] == key)
                return true;
            (*steps)++;
        }
        return false;
    }

    void erase(Key key) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t idx = 0;; idx++) {
            if (slot_1->keys[idx] == key) {
                for (uint64_t j = idx; j < BUCKET - 1; j++) {
                    slot_1->keys[j] = slot_1->keys[j + 1];
                    slot_1->values[j] = slot_1->values[j + 1];
                }
                slot_1->keys[BUCKET - 1] = EMPTY;
                size_--;
                return;
            }
            if (slot_2->keys[idx] == key) {
                for (uint64_t j = idx; j < BUCKET - 1; j++) {
                    slot_2->keys[j] = slot_2->keys[j + 1];
                    slot_2->values[j] = slot_2->values[j + 1];
                }
                slot_2->keys[BUCKET - 1] = EMPTY;
                size_--;
                return;
            }
        }
    }

    void grow() {
        uint64_t old_capacity = capacity;
        Slot* old_data = data;
        size_ = 0;
        capacity *= 2;
        data = reinterpret_cast<Slot*>(__aligned_alloc(CACHE_LINE, sizeof(Slot) * capacity));
        fill_empty(data, capacity);
        for (uint64_t idx = 0; idx < old_capacity; idx++) {
            Slot* slot = &old_data[idx];
            for (uint64_t j = 0; j < BUCKET && slot->keys[j] != EMPTY; j++) {
                insert(slot->keys[j], slot->values[j]);
            }
        }
        __aligned_free(old_data);
    }

    void clear() {
        size_ = 0;
        fill_empty(data, capacity);
    }

    uint64_t index_for(Key key) {
        uint64_t hash = TableTrait::hash(key);
        return hash;
    }
    uint64_t prefetch(Key key) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        prefetch(data[index_1].keys);
        prefetch(data[index_1].values);
        prefetch(data[index_2].keys);
        prefetch(data[index_2].values);
        return hash;
    }
    Value find_indexed(Key key, uint64_t hash, uint64_t* steps) {
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t idx = 0;; idx++) {
            if (slot_1->keys[idx] == key)
                return slot_1->values[idx];
            (*steps)++;
            if (slot_2->keys[idx] == key)
                return slot_2->values[idx];
            (*steps)++;
        }
    }

    uint64_t size() {
        return size_;
    }

    uint64_t memory_usage() {
        return sizeof(Slot) * capacity + sizeof(TwoWayBaseline);
    }

    Value sum_all_values() {
        Value sum{};
        for (uint64_t idx = 0; idx < capacity; idx++) {
            Slot* slot = &data[idx];
            for (uint64_t j = 0; j < BUCKET && slot->keys[j] != EMPTY; j++) {
                sum += slot->values[j];
            }
        }
        return sum;
    }

    struct Slot {
        Key keys[BUCKET];
        Value values[BUCKET];
    };

    static void fill_empty(Slot* slots, uint64_t count) {
        for (uint64_t idx = 0; idx < count; idx++) {
            std::fill_n(slots[idx].keys, BUCKET, EMPTY);
        }
    }

    Slot* data;
    uint64_t capacity;
    uint64_t size_;
};

template <TableTrait TableTrait, uint64_t BUCKET>
struct TwoWay {
    using Key = typename TableTrait::Key;
    using Value = typename TableTrait::Value;

    static constexpr Key EMPTY = std::numeric_limits<Key>::max();

    TwoWay() : size_(0), capacity(8) {
        data = reinterpret_cast<Slot*>(__aligned_alloc(CACHE_LINE, sizeof(Slot) * capacity));
        fill_empty(data, capacity);
    }
    ~TwoWay() {
        __aligned_free(data);
    }

    // assumes key is not in the map
    void insert(Key key, Value value) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        uint64_t n_1 = 0;
        uint64_t n_2 = 0;
        while (slot_1->keys[n_1] < EMPTY && ++n_1 < BUCKET)
            ;
        while (slot_2->keys[n_2] < EMPTY && ++n_2 < BUCKET)
            ;
        if (n_1 == BUCKET && n_2 == BUCKET) {
            grow();
            insert(key, value);
            return;
        }
        if (n_1 <= n_2) {
            slot_1->keys[n_1] = key;
            slot_1->values[n_1] = value;
        } else {
            slot_2->keys[n_2] = key;
            slot_2->values[n_2] = value;
        }
        size_++;
    }

    Value find(Key key, uint64_t* steps) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t i = 0;; i++) {
            if (slot_1->keys[i] == key)
                return slot_1->values[i];
            (*steps)++;
            if (slot_2->keys[i] == key)
                return slot_2->values[i];
            (*steps)++;
        }
    }

    bool contains(Key key, uint64_t* steps) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t i = 0; i < BUCKET && (slot_1->keys[i] != EMPTY || slot_2->keys[i] != EMPTY);
             i++) {
            if (slot_1->keys[i] == key)
                return true;
            (*steps)++;
            if (slot_2->keys[i] == key)
                return true;
            (*steps)++;
        }
        return false;
    }

    void erase(Key key) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t i = 0;; i++) {
            if (slot_1->keys[i] == key) {
                for (uint64_t j = i; j < BUCKET - 1; j++) {
                    slot_1->keys[j] = slot_1->keys[j + 1];
                    slot_1->values[j] = slot_1->values[j + 1];
                }
                slot_1->keys[BUCKET - 1] = EMPTY;
                size_--;
                return;
            }
            if (slot_2->keys[i] == key) {
                for (uint64_t j = i; j < BUCKET - 1; j++) {
                    slot_2->keys[j] = slot_2->keys[j + 1];
                    slot_2->values[j] = slot_2->values[j + 1];
                }
                slot_2->keys[BUCKET - 1] = EMPTY;
                size_--;
                return;
            }
        }
    }

    void grow() {
        uint64_t old_capacity = capacity;
        Slot* old_data = data;
        size_ = 0;
        capacity *= 2;
        data = reinterpret_cast<Slot*>(__aligned_alloc(CACHE_LINE, sizeof(Slot) * capacity));
        fill_empty(data, capacity);
        for (uint64_t i = 0; i < old_capacity; i++) {
            Slot* slot = &old_data[i];
            for (uint64_t j = 0; j < BUCKET && slot->keys[j] != EMPTY; j++) {
                insert(slot->keys[j], slot->values[j]);
            }
        }
        __aligned_free(old_data);
    }

    void clear() {
        size_ = 0;
        fill_empty(data, capacity);
    }

    uint64_t index_for(Key key) {
        uint64_t hash = TableTrait::hash(key);
        return hash;
    }
    uint64_t prefetch(Key key) {
        uint64_t hash = TableTrait::hash(key);
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        prefetch(data[index_1].keys);
        prefetch(data[index_1].values);
        prefetch(data[index_2].keys);
        prefetch(data[index_2].values);
        return hash;
    }
    Value find_indexed(Key key, uint64_t hash, uint64_t* steps) {
        uint64_t index_1 = hash & (capacity - 1);
        uint64_t index_2 = (hash >> 32) & (capacity - 1);
        Slot* slot_1 = &data[index_1];
        Slot* slot_2 = &data[index_2];
        for (uint64_t i = 0;; i++) {
            if (slot_1->keys[i] == key)
                return slot_1->values[i];
            (*steps)++;
            if (slot_2->keys[i] == key)
                return slot_2->values[i];
            (*steps)++;
        }
    }

    uint64_t size() {
        return size_;
    }

    uint64_t memory_usage() {
        return sizeof(Slot) * capacity + sizeof(TwoWay);
    }

    Value sum_all_values() {
        Value sum{};
        for (uint64_t i = 0; i < capacity; i++) {
            Slot* slot = &data[i];
            for (uint64_t j = 0; j < BUCKET && slot->keys[j] != EMPTY; j++) {
                sum += slot->values[j];
            }
        }
        return sum;
    }

    struct Slot {
        Key keys[BUCKET];
        Value values[BUCKET];
    };

    static void fill_empty(Slot* slots, uint64_t count) {
        for (uint64_t idx = 0; idx < count; idx++) {
            std::fill_n(slots[idx].keys, BUCKET, EMPTY);
        }
    }

    Slot* data;
    uint64_t capacity;
    uint64_t size_;
};
