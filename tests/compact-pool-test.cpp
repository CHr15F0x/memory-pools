#include <kl-compact-pool.h>
#include <gtest.h>

using namespace kl;

// TODO add 32 bit cases

TEST(CompactPool, OffsetAndMark0)
{
    // 0 level tree holds up to 64^1 elements
    internal::cpu_word_t allocated[1] = { 0u };
    
    // Insert 1st element into the tree
    EXPECT_EQ(0u, internal::OffsetAndMark<0u>(allocated));
    EXPECT_EQ(0x01U, allocated[0]);
    
    // Insert 64th element (last)
    allocated[0] = 0x7Fu;
    EXPECT_EQ(7u, internal::OffsetAndMark<0u>(allocated));
    EXPECT_EQ(0xFFu, allocated[0]);
}

TEST(CompactPool, OffsetAndMark1)
{
    // 1 level tree holds up to 64^2 elements
    internal::cpu_word_t allocated[1 + 64] = { 0u };

    // Insert 1st element into the tree
    EXPECT_EQ(0u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0x00u, allocated[0]);
    EXPECT_EQ(0x01u, allocated[1]);

    // Insert 8th
    allocated[1] = 0x7Fu;
    EXPECT_EQ(7u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0x00u, allocated[0]);
    EXPECT_EQ(0xFFu, allocated[1]);
    
    // Insert 9th
    EXPECT_EQ(8u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0x00u, allocated[0]);
    EXPECT_EQ(0x1FFu, allocated[1]);
    
    // Insert 63rd
    allocated[1] = 0x3FFFFFFFFFFFFFFFu;
    EXPECT_EQ(62u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0x00u, allocated[0]);
    EXPECT_EQ(0x7FFFFFFFFFFFFFFFu, allocated[1]);
    
    // Insert 64th
    allocated[1] = 0x7FFFFFFFFFFFFFFFu;
    EXPECT_EQ(63u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0x01u, allocated[0]);
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFu, allocated[1]);

    // Insert 4095th
    memset(allocated, 0xFF, sizeof(allocated));
    allocated[0] = 0x7FFFFFFFFFFFFFFFu;
    allocated[64] = 0x3FFFFFFFFFFFFFFFu;
    EXPECT_EQ(4094u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0x7FFFFFFFFFFFFFFFu, allocated[0]);
    EXPECT_EQ(0x7FFFFFFFFFFFFFFFu, allocated[64]);

    // Insert 4096th (last)
    EXPECT_EQ(4095u, internal::OffsetAndMark<1u>(allocated));
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFu, allocated[0]);
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFu, allocated[64]);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}