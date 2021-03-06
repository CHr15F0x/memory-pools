#include <gtest.h>
#include "liberal-pool.h"

using namespace kl;
using namespace kl::private_internal;

// TODO build and run on 32bits
// TODO build and run on 32bits
// TODO build and run on 32bits

// Expectations for bits set
static const cpu_word_t kNoBitsSet = static_cast<cpu_word_t>(0u);
static const cpu_word_t k1stBitSet = static_cast<cpu_word_t>(1u);
static const cpu_word_t kAllBut1BitsSet = (~kNoBitsSet) >> 1u;

TEST(private_internal, LeastBitClear)
{
    EXPECT_EQ(0u, LeastBitClear(kNoBitsSet));
    EXPECT_EQ(1u, LeastBitClear(k1stBitSet));
    EXPECT_EQ(BITS - 1, LeastBitClear(kAllBut1BitsSet));
}

struct TreeDepthTestData
{
    size_t expected_depth;
    size_t input_capacity;
};

struct TreeDepthTest: public ::testing::TestWithParam<TreeDepthTestData>
{
    TreeDepthTestData test_data;
};

TEST_P(TreeDepthTest, TreeDepth)
{
    EXPECT_EQ(GetParam().expected_depth, TreeDepth(GetParam().input_capacity));
}

TreeDepthTestData kTreeDepthTestCases[] =
{
    {0u, 1u},
    {0u, BITS - 1u},
    {0u, BITS},
    {1u, BITS + 1u},
    {1u, BITS2 - 1u},
    {1u, BITS2},
    {2u, BITS2 + 1u},
    {2u, BITS3 - 1u},
    {2u, BITS3},
    {3u, BITS3 + 1u},
    {3u, BITS4 - 1u},
    {3u, BITS4}
};

INSTANTIATE_TEST_CASE_P(private_internal, TreeDepthTest, ::testing::ValuesIn(kTreeDepthTestCases));

struct TreeSizeTestData
{
    size_t expected_tree_size;
    size_t input_capacity;
};

struct TreeSizeTest: public ::testing::TestWithParam<TreeSizeTestData>
{
    TreeSizeTestData test_data;
};

TEST_P(TreeSizeTest, TreeSize)
{
    EXPECT_EQ(GetParam().expected_tree_size, TreeSize(GetParam().input_capacity, TreeDepth(GetParam().input_capacity)));
}

TreeSizeTestData kTreeSizeTestCases[] =
{
    {1u, 1u},
    {1u, BITS - 1u},
    {1u, BITS},
    {3u, BITS + 1u},
    {3u, BITS * 2},
    {4u, BITS * 2 + 1u},
    {BITS, BITS2 - BITS},
    {BITS + 1u, BITS2 - 1u},
    {BITS + 1u, BITS2},
    {BITS2 + BITS + 1u, BITS3},
    {BITS2 + 2u + BITS2 + BITS + 1u, BITS3 + BITS + 9u},
    {BITS2 + 1u + BITS2 + BITS + 1u, BITS3 + 1u},
    {BITS3 + BITS2 + BITS + 1u, BITS4 - 1u},
    {BITS3 + BITS2 + BITS + 1u, BITS4}
};

INSTANTIATE_TEST_CASE_P(private_internal, TreeSizeTest, ::testing::ValuesIn(kTreeSizeTestCases));

template <size_t Depth>
struct TreeDepthHelper
{
    static constexpr size_t depth = Depth;
    static constexpr size_t max_capacity = Pow(BITS, depth);
    static constexpr size_t masks_size = MaxTreeSize(depth);
};

template<typename TreeDepthType>
struct MarkUnmarkTest : public ::testing::Test
{};

typedef ::testing::Types<
    TreeDepthHelper<0u>,
    TreeDepthHelper<1u>,
    TreeDepthHelper<2u>,
    TreeDepthHelper<3u>
> TreeDepthTypes;

TYPED_TEST_CASE(MarkUnmarkTest, TreeDepthTypes);

TYPED_TEST(MarkUnmarkTest, private_internal_OffsetAndMark_AndThen_Unmark)
{
    cpu_word_t mask_tree[TypeParam::masks_size];
    memset(mask_tree, 0u, sizeof(mask_tree));

    for (size_t i = 0; i < TypeParam::max_capacity; ++i)
    {
        ASSERT_EQ(i, OffsetAndMark<TypeParam::depth>(mask_tree));
        Unmark<TypeParam::depth>(mask_tree, i);
        ASSERT_EQ(i, OffsetAndMark<TypeParam::depth>(mask_tree));
    }
}

struct A
{
    cpu_word_t data;
};

template <typename PoolType>
static void PoolTestFn(PoolType &pool)
{
    const size_t CAPACITY = pool.Capacity();

    EXPECT_EQ(0u, pool.Size());
    EXPECT_EQ(0u, pool.MaxUtilisation());

    std::vector<A *> ptrs(CAPACITY);
    std::vector<size_t> order_of_frees(CAPACITY);
    // Fill with 0, 1, 2, ...
    std::iota(order_of_frees.begin(), order_of_frees.end(), 0u);
    std::random_device rd;
    std::mt19937 gen(rd());
    // Make it random
    std::shuffle(order_of_frees.begin(), order_of_frees.end(), gen);

    for (size_t i = 0; i < order_of_frees.size(); ++i)
    {
        ASSERT_EQ(i, pool.Size());
        ptrs[i] = pool.Alloc();
        // Some fake data
        ptrs[i]->data = i;
        // Size increased
        ASSERT_EQ(i + 1, pool.Size());
    }

    // Full
    EXPECT_EQ(nullptr, pool.Alloc());

    for (int i = CAPACITY - 1; i >= 0; --i)
    {
        ASSERT_EQ(static_cast<size_t>(i + 1), pool.Size());
        ASSERT_EQ(static_cast<cpu_word_t>(order_of_frees[i]), ptrs[order_of_frees[i]]->data);
        pool.Free(ptrs[i]);
        // Free does not wipe the data, so check if previous free has not corrupted this entry
        ASSERT_EQ(static_cast<cpu_word_t>(order_of_frees[i]), ptrs[order_of_frees[i]]->data);
        // Now we can clear it
        ptrs[order_of_frees[i]]->data = 0u;
        // Size decreased
        ASSERT_EQ(static_cast<size_t>(i), pool.Size());
    }

    // Re-shuffle
    std::shuffle(order_of_frees.begin(), order_of_frees.end(), gen);

    for (size_t i = 0; i < order_of_frees.size(); ++i)
    {
        ASSERT_EQ(i, pool.Size());
        ptrs[i] = pool.Alloc();
        // Other fake data
        ptrs[i]->data = i * i;
        // Size increased
        ASSERT_EQ(i + 1, pool.Size());
    }

    for (int i = CAPACITY - 1; i >= 0; --i)
    {
        ASSERT_EQ(static_cast<size_t>(i + 1), pool.Size());
        ASSERT_EQ(static_cast<cpu_word_t>(order_of_frees[i]) * static_cast<cpu_word_t>(order_of_frees[i]), ptrs[order_of_frees[i]]->data);
        pool.Free(ptrs[i]);
        // Free does not wipe the data, so check if previous free has not corrupted this entry
        ASSERT_EQ(static_cast<cpu_word_t>(order_of_frees[i]) * static_cast<cpu_word_t>(order_of_frees[i]), ptrs[order_of_frees[i]]->data);
        // Now we can clear it
        ptrs[order_of_frees[i]]->data = 0u;
        // Size decreased
        ASSERT_EQ(static_cast<size_t>(i), pool.Size());
    }
}

template<typename PoolType>
struct StaticLiberalPoolTest : public ::testing::Test
{};

typedef ::testing::Types<
    StaticLiberalPool<A, 1u>,
    StaticLiberalPool<A, 13u>,
    StaticLiberalPool<A, BITS>,
    StaticLiberalPool<A, 811u>,
    StaticLiberalPool<A, BITS2>,
    StaticLiberalPool<A, 11113u>,
    StaticLiberalPool<A, BITS3>,
    StaticLiberalPool<A, 262217u>,
    StaticLiberalPool<A, BITS4>
> PoolTypes;

TYPED_TEST_CASE(StaticLiberalPoolTest, PoolTypes);

TYPED_TEST(StaticLiberalPoolTest, AllocAndRandomFree_Twice_NoMemoryCorruption)
{
    // This kind of pool has to be static, using it on the stack could easily lead to overflow
    static TypeParam pool;
    PoolTestFn(pool);
}

TEST(LiberalPoolTest, AllocAndRandomFree_Twice_NoMemoryCorruption)
{
    const size_t CAPCITIES[] =
    {
        1u, 13u, BITS, 811u, BITS2, 11113u, BITS3, 262217u, BITS4
    };

    for (size_t k = 0; k < sizeof(CAPCITIES) / sizeof(CAPCITIES[0]); ++k)
    {
        LiberalPool<A> pool(CAPCITIES[k]);
        PoolTestFn(pool);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
