#include <cstdint>

#include <gtest/gtest.h>

#include "TwoWay.hpp"

struct PairValue {
    uint32_t a;
    uint32_t b;
    bool operator==(const PairValue& other) const {
        return a == other.a && b == other.b;
    }
};

struct Int32ToU32TableTrait {
    using Key = int32_t;
    using Value = uint32_t;

    static uint64_t hash(Key key) {
        return squirrel3(static_cast<uint64_t>(key));
    }
};

struct Int64ToPairTableTrait {
    using Key = int64_t;
    using Value = PairValue;

    static uint64_t hash(Key key) {
        return squirrel3(static_cast<uint64_t>(key));
    }
};

struct U64ToU64TableTrait {
    using Key = uint64_t;
    using Value = uint64_t;

    static uint64_t hash(Key key) {
        return squirrel3(key);
    }
};

TEST(TwoWay, InsertFindContains) {
    TwoWay<Int32ToU32TableTrait, 4> map;
    uint64_t steps = 0;

    map.insert(1, 10);
    map.insert(-2, 20);
    map.insert(7, 30);

    EXPECT_TRUE(map.contains(1, &steps));
    steps = 0;
    EXPECT_TRUE(map.contains(-2, &steps));
    steps = 0;
    EXPECT_TRUE(map.contains(7, &steps));

    steps = 0;
    EXPECT_EQ(map.find(1, &steps), 10u);
    steps = 0;
    EXPECT_EQ(map.find(-2, &steps), 20u);
    steps = 0;
    EXPECT_EQ(map.find(7, &steps), 30u);
}

TEST(TwoWay, EraseRemovesKey) {
    TwoWay<U64ToU64TableTrait, 4> map;
    uint64_t steps = 0;

    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    map.erase(2);

    EXPECT_FALSE(map.contains(2, &steps));
    steps = 0;
    EXPECT_TRUE(map.contains(1, &steps));
    steps = 0;
    EXPECT_TRUE(map.contains(3, &steps));
}

TEST(TwoWay, GrowsAndSumsValues) {
    TwoWay<U64ToU64TableTrait, 4> map;
    uint64_t expected_sum = 0;

    for (uint64_t i = 0; i < 200; i++) {
        map.insert(i + 1, i + 10);
        expected_sum += i + 10;
    }

    EXPECT_EQ(map.size(), 200u);
    EXPECT_EQ(map.sum_all_values(), expected_sum);

    uint64_t steps = 0;
    EXPECT_EQ(map.find(42, &steps), 51u);
}

TEST(TwoWay, HandlesSignedKeysAndStructValues) {
    TwoWay<Int64ToPairTableTrait, 4> map;
    uint64_t steps = 0;

    map.insert(-100, PairValue{1, 2});
    map.insert(5000, PairValue{3, 4});

    EXPECT_TRUE(map.contains(-100, &steps));
    steps = 0;
    EXPECT_TRUE(map.contains(5000, &steps));

    steps = 0;
    EXPECT_EQ(map.find(-100, &steps), (PairValue{1, 2}));
    steps = 0;
    EXPECT_EQ(map.find(5000, &steps), (PairValue{3, 4}));
}

TEST(TwoWay, ClearAndReuse) {
    TwoWay<U64ToU64TableTrait, 4> map;
    uint64_t steps = 0;

    map.insert(10, 100);
    map.insert(20, 200);
    map.clear();

    EXPECT_FALSE(map.contains(10, &steps));
    steps = 0;
    EXPECT_FALSE(map.contains(20, &steps));

    map.insert(30, 300);
    steps = 0;
    EXPECT_TRUE(map.contains(30, &steps));
    steps = 0;
    EXPECT_EQ(map.find(30, &steps), 300u);
}
