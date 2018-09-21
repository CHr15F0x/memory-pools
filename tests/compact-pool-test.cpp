#include <kl-compact-pool.h>
#include <gtest.h>

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

struct GetLevelTestData
{
    size_t expected_depth;
    size_t input_capacity;
};

struct GetLevelTest: public ::testing::TestWithParam<GetLevelTestData>
{
    GetLevelTestData test_data;
};

TEST_P(GetLevelTest, GetLevel)
{
    EXPECT_EQ(GetParam().expected_depth, GetLevel(GetParam().input_capacity));
}

GetLevelTestData kGetLevelTestCases[] =
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

INSTANTIATE_TEST_CASE_P(private_internal, GetLevelTest, ::testing::ValuesIn(kGetLevelTestCases));

struct GetTreeSizeTestData
{
    size_t expected_tree_size;
    size_t input_capacity;
};

struct GetTreeSizeTest: public ::testing::TestWithParam<GetTreeSizeTestData>
{
    GetTreeSizeTestData test_data;
};

TEST_P(GetTreeSizeTest, GetTreeSize)
{
    EXPECT_EQ(GetParam().expected_tree_size, OptimalAllocatedSize2(GetParam().input_capacity, GetLevel(GetParam().input_capacity)));
}

GetTreeSizeTestData kGetTreeSizeTestCases[] =
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

INSTANTIATE_TEST_CASE_P(private_internal, GetTreeSizeTest, ::testing::ValuesIn(kGetTreeSizeTestCases));

template <size_t Depth>
struct TreeDepth
{
    static constexpr size_t depth = Depth;
    static constexpr size_t max_capacity = Pow(BITS, depth);
    static constexpr size_t masks_size = MaxTreeSize(depth);
};

template<typename TreeDepthType>
struct TreeDepthTest : public ::testing::Test
{};

typedef ::testing::Types<
    TreeDepth<0u>,
    TreeDepth<1u>,
    TreeDepth<2u>,
    TreeDepth<3u>
> TreeDepthTypes;

TYPED_TEST_CASE(TreeDepthTest, TreeDepthTypes);

TYPED_TEST(TreeDepthTest, private_internal_OffsetAndMark_AndThen_Unmark)
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

template<typename CompactPoolType>
struct CompactPoolTest : public ::testing::Test
{};

typedef ::testing::Types<
    CompactPool<A, 1u>,
    CompactPool<A, 13u>,
    CompactPool<A, BITS>,
    CompactPool<A, 811u>,
    CompactPool<A, BITS2>,
    CompactPool<A, 11113u>,
    CompactPool<A, BITS3>,
    CompactPool<A, 262217u>,
    CompactPool<A, BITS4>
> CompactPoolTypes;

TYPED_TEST_CASE(CompactPoolTest, CompactPoolTypes);

TYPED_TEST(CompactPoolTest, AllocAndRandomFree_Twice_NoMemoryCorruption)
{
    // This kind of pool has to be static, using it on the stack could easily lead to overflow
    static TypeParam pool;
    PoolTestFn(pool);
}

TEST(CompactPool2Test, AllocAndRandomFree_Twice_NoMemoryCorruption)
{
    const size_t CAPCITIES[] =
    {
        1u, 13u, BITS, 811u, BITS2, 11113u, BITS3, 262217u, BITS4
    };

    for (size_t k = 0; k < sizeof(CAPCITIES) / sizeof(CAPCITIES[0]); ++k)
    {
        CompactPool2<A> pool(CAPCITIES[k]);
        PoolTestFn(pool);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
