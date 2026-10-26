#include <gtest/gtest.h>

TEST(AddTest, Default) {
    constexpr int a = 1;
    constexpr int b = 4;

    EXPECT_EQ(a + b, 5);
    EXPECT_TRUE(a + b == 5);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
