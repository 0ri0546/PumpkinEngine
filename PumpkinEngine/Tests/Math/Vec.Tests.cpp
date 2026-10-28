// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

import Pumpkin.Math.Vec;

import std;

namespace {
    // Absolute-tolerance comparison that reports the caller's line on failure.
    void ExpectNear(double expected, double actual, double tolerance = 1e-4,
        std::source_location loc = std::source_location::current()) {
        ::testing::ScopedTrace trace(loc.file_name(), static_cast<int>(loc.line()), "ExpectNear");
        EXPECT_NEAR(expected, actual, tolerance);
    }
} // namespace

// ---------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------

TEST(VecTest, DefaultConstruction) {
    constexpr Vec<float, 3> vec{};

    EXPECT_FLOAT_EQ(0.0f, vec[0]);
    EXPECT_FLOAT_EQ(0.0f, vec[1]);
    EXPECT_FLOAT_EQ(0.0f, vec[2]);
}

TEST(VecTest, ValueConstruction) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    EXPECT_FLOAT_EQ(1.0f, vec[0]);
    EXPECT_FLOAT_EQ(2.0f, vec[1]);
    EXPECT_FLOAT_EQ(3.0f, vec[2]);
}

TEST(VecTest, DifferentDimensions) {
    constexpr Vec<float, 2> vec2{ 1, 2 };
    constexpr Vec<float, 3> vec3{ 1, 2, 3 };
    constexpr Vec<float, 4> vec4{ 1, 2, 3, 4 };

    EXPECT_EQ(static_cast<std::size_t>(2), static_cast<std::size_t>(vec2.Size()));
    EXPECT_EQ(static_cast<std::size_t>(3), static_cast<std::size_t>(vec3.Size()));
    EXPECT_EQ(static_cast<std::size_t>(4), static_cast<std::size_t>(vec4.Size()));

    EXPECT_FLOAT_EQ(1.0f, vec2[0]);
    EXPECT_FLOAT_EQ(2.0f, vec2[1]);

    EXPECT_FLOAT_EQ(1.0f, vec3[0]);
    EXPECT_FLOAT_EQ(2.0f, vec3[1]);
    EXPECT_FLOAT_EQ(3.0f, vec3[2]);

    EXPECT_FLOAT_EQ(1.0f, vec4[0]);
    EXPECT_FLOAT_EQ(2.0f, vec4[1]);
    EXPECT_FLOAT_EQ(3.0f, vec4[2]);
    EXPECT_FLOAT_EQ(4.0f, vec4[3]);
}

TEST(VecTest, MixedValueTypes) {
    constexpr Vec<float, 3> vec{ 1, 2.5, 3 };

    EXPECT_NEAR(1.0f, vec[0], 1e-4);
    EXPECT_NEAR(2.5f, vec[1], 1e-4);
    EXPECT_NEAR(3.0f, vec[2], 1e-4);
}

// ---------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------

TEST(VecTest, ElementAccess) {
    Vec<float, 3> vec{ 1, 2, 3 };

    vec[0] = 10;
    vec[1] = 20;
    vec[2] = 30;

    EXPECT_FLOAT_EQ(10.0f, vec[0]);
    EXPECT_FLOAT_EQ(20.0f, vec[1]);
    EXPECT_FLOAT_EQ(30.0f, vec[2]);
}

TEST(VecTest, ConstElementAccess) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    EXPECT_FLOAT_EQ(1.0f, vec[0]);
    EXPECT_FLOAT_EQ(2.0f, vec[1]);
    EXPECT_FLOAT_EQ(3.0f, vec[2]);
}

TEST(VecTest, Data) {
    Vec<float, 3> vec{ 1, 2, 3 };

    float* data = vec.Data();

    ASSERT_NE(data, nullptr);

    data[0] = 10;
    data[1] = 20;
    data[2] = 30;

    EXPECT_FLOAT_EQ(10.0f, vec[0]);
    EXPECT_FLOAT_EQ(20.0f, vec[1]);
    EXPECT_FLOAT_EQ(30.0f, vec[2]);
}

TEST(VecTest, Size) {
    EXPECT_EQ(static_cast<std::size_t>(2), static_cast<std::size_t>((Vec<float, 2>::Size())));
    EXPECT_EQ(static_cast<std::size_t>(3), static_cast<std::size_t>((Vec<float, 3>::Size())));
    EXPECT_EQ(static_cast<std::size_t>(4), static_cast<std::size_t>((Vec<float, 4>::Size())));
    EXPECT_EQ(static_cast<std::size_t>(8), static_cast<std::size_t>((Vec<float, 8>::Size())));
}

// ---------------------------------------------------------------------
// Equality
// ---------------------------------------------------------------------

TEST(VecTest, Equality) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 1, 2, 3 };

    EXPECT_TRUE(a == b);
}

TEST(VecTest, Inequality) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 1, 2, 4 };

    EXPECT_FALSE(a == b);
}

TEST(VecTest, EqualityDifferentTypes) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<double, 3> b{ 1.0, 2.0, 3.0 };

    EXPECT_TRUE(a == b);
}

// ---------------------------------------------------------------------
// Addition
// ---------------------------------------------------------------------

TEST(VecTest, Addition) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    constexpr auto result = a + b;

    EXPECT_FLOAT_EQ(5.0f, result[0]);
    EXPECT_FLOAT_EQ(7.0f, result[1]);
    EXPECT_FLOAT_EQ(9.0f, result[2]);
}

TEST(VecTest, AdditionCommonType) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<double, 3> b{ 0.5, 1.5, 2.5 };

    constexpr auto result = a + b;

    static_assert(std::is_same_v<decltype(result), const Vec<double, 3>>);

    EXPECT_NEAR(1.5, result[0], 1e-4);
    EXPECT_NEAR(3.5, result[1], 1e-4);
    EXPECT_NEAR(5.5, result[2], 1e-4);
}

TEST(VecTest, AdditionAssignment) {
    Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    a += b;

    EXPECT_FLOAT_EQ(5.0f, a[0]);
    EXPECT_FLOAT_EQ(7.0f, a[1]);
    EXPECT_FLOAT_EQ(9.0f, a[2]);
}

TEST(VecTest, MixedTypeAdditionAssignment) {
    Vec<double, 3> a{ 1.0, 2.0, 3.0 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    a += b;

    EXPECT_NEAR(5.0, a[0], 1e-4);
    EXPECT_NEAR(7.0, a[1], 1e-4);
    EXPECT_NEAR(9.0, a[2], 1e-4);
}

// ---------------------------------------------------------------------
// Subtraction
// ---------------------------------------------------------------------

TEST(VecTest, Subtraction) {
    constexpr Vec<float, 3> a{ 5, 7, 9 };
    constexpr Vec<float, 3> b{ 1, 2, 3 };

    constexpr auto result = a - b;

    EXPECT_FLOAT_EQ(4.0f, result[0]);
    EXPECT_FLOAT_EQ(5.0f, result[1]);
    EXPECT_FLOAT_EQ(6.0f, result[2]);
}

TEST(VecTest, SubtractionCommonType) {
    constexpr Vec<float, 3> a{ 5, 7, 9 };
    constexpr Vec<double, 3> b{ 0.5, 1.5, 2.5 };

    constexpr auto result = a - b;

    static_assert(std::is_same_v<decltype(result), const Vec<double, 3>>);

    EXPECT_NEAR(4.5, result[0], 1e-4);
    EXPECT_NEAR(5.5, result[1], 1e-4);
    EXPECT_NEAR(6.5, result[2], 1e-4);
}

TEST(VecTest, SubtractionAssignment) {
    Vec<float, 3> a{ 5, 7, 9 };
    constexpr Vec<float, 3> b{ 1, 2, 3 };

    a -= b;

    EXPECT_FLOAT_EQ(4.0f, a[0]);
    EXPECT_FLOAT_EQ(5.0f, a[1]);
    EXPECT_FLOAT_EQ(6.0f, a[2]);
}

// ---------------------------------------------------------------------
// Component-wise multiplication
// ---------------------------------------------------------------------

TEST(VecTest, ComponentMultiplication) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    constexpr auto result = a * b;

    EXPECT_FLOAT_EQ(4.0f, result[0]);
    EXPECT_FLOAT_EQ(10.0f, result[1]);
    EXPECT_FLOAT_EQ(18.0f, result[2]);
}

TEST(VecTest, ComponentMultiplicationCommonType) {
    constexpr Vec<float, 3> a{ 2, 3, 4 };
    constexpr Vec<double, 3> b{ 0.5, 1.5, 2.5 };

    constexpr auto result = a * b;

    static_assert(std::is_same_v<decltype(result), const Vec<double, 3>>);

    EXPECT_NEAR(1.0, result[0], 1e-4);
    EXPECT_NEAR(4.5, result[1], 1e-4);
    EXPECT_NEAR(10.0, result[2], 1e-4);
}

TEST(VecTest, ComponentMultiplicationAssignment) {
    Vec<float, 3> a{ 2, 3, 4 };
    constexpr Vec<float, 3> b{ 5, 6, 7 };

    a *= b;

    EXPECT_FLOAT_EQ(10.0f, a[0]);
    EXPECT_FLOAT_EQ(18.0f, a[1]);
    EXPECT_FLOAT_EQ(28.0f, a[2]);
}

// ---------------------------------------------------------------------
// Component-wise division
// ---------------------------------------------------------------------

TEST(VecTest, ComponentDivision) {
    constexpr Vec<float, 3> a{ 10, 20, 30 };
    constexpr Vec<float, 3> b{ 2, 4, 5 };

    constexpr auto result = a / b;

    EXPECT_FLOAT_EQ(5.0f, result[0]);
    EXPECT_FLOAT_EQ(5.0f, result[1]);
    EXPECT_FLOAT_EQ(6.0f, result[2]);
}

TEST(VecTest, ComponentDivisionCommonType) {
    constexpr Vec<float, 3> a{ 1, 3, 5 };
    constexpr Vec<double, 3> b{ 2.0, 2.0, 2.0 };

    constexpr auto result = a / b;

    static_assert(std::is_same_v<decltype(result), const Vec<double, 3>>);

    EXPECT_NEAR(0.5, result[0], 1e-4);
    EXPECT_NEAR(1.5, result[1], 1e-4);
    EXPECT_NEAR(2.5, result[2], 1e-4);
}

TEST(VecTest, ComponentDivisionAssignment) {
    Vec<float, 3> a{ 10, 20, 30 };
    constexpr Vec<float, 3> b{ 2, 4, 5 };

    a /= b;

    EXPECT_FLOAT_EQ(5.0f, a[0]);
    EXPECT_FLOAT_EQ(5.0f, a[1]);
    EXPECT_FLOAT_EQ(6.0f, a[2]);
}

// ---------------------------------------------------------------------
// Scalar multiplication
// ---------------------------------------------------------------------

TEST(VecTest, ScalarMultiplication) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    constexpr auto result = vec * 2.0f;

    EXPECT_FLOAT_EQ(2.0f, result[0]);
    EXPECT_FLOAT_EQ(4.0f, result[1]);
    EXPECT_FLOAT_EQ(6.0f, result[2]);
}

TEST(VecTest, ScalarMultiplicationCommonType) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    constexpr auto result = vec * 0.5;

    static_assert(std::is_same_v<decltype(result), const Vec<double, 3>>);

    EXPECT_NEAR(0.5, result[0], 1e-4);
    EXPECT_NEAR(1.0, result[1], 1e-4);
    EXPECT_NEAR(1.5, result[2], 1e-4);
}

TEST(VecTest, ScalarMultiplicationReversed) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    constexpr auto result = 2.0f * vec;

    EXPECT_FLOAT_EQ(2.0f, result[0]);
    EXPECT_FLOAT_EQ(4.0f, result[1]);
    EXPECT_FLOAT_EQ(6.0f, result[2]);
}

TEST(VecTest, ScalarMultiplicationAssignment) {
    Vec<float, 3> vec{ 1, 2, 3 };

    vec *= 3.0f;

    EXPECT_FLOAT_EQ(3.0f, vec[0]);
    EXPECT_FLOAT_EQ(6.0f, vec[1]);
    EXPECT_FLOAT_EQ(9.0f, vec[2]);
}

// ---------------------------------------------------------------------
// Scalar division
// ---------------------------------------------------------------------

TEST(VecTest, ScalarDivision) {
    constexpr Vec<float, 3> vec{ 10, 20, 30 };

    constexpr auto result = vec / 2.0f;

    EXPECT_FLOAT_EQ(5.0f, result[0]);
    EXPECT_FLOAT_EQ(10.0f, result[1]);
    EXPECT_FLOAT_EQ(15.0f, result[2]);
}

TEST(VecTest, ScalarDivisionCommonType) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    constexpr auto result = vec / 2.0;

    static_assert(std::is_same_v<decltype(result), const Vec<double, 3>>);

    EXPECT_NEAR(0.5, result[0], 1e-4);
    EXPECT_NEAR(1.0, result[1], 1e-4);
    EXPECT_NEAR(1.5, result[2], 1e-4);
}

TEST(VecTest, ScalarDivisionAssignment) {
    Vec<float, 3> vec{ 10, 20, 30 };

    vec /= 2.0f;

    EXPECT_FLOAT_EQ(5.0f, vec[0]);
    EXPECT_FLOAT_EQ(10.0f, vec[1]);
    EXPECT_FLOAT_EQ(15.0f, vec[2]);
}

// ---------------------------------------------------------------------
// Unary operators
// ---------------------------------------------------------------------

TEST(VecTest, UnaryPlus) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    constexpr auto result = +vec;

    EXPECT_FLOAT_EQ(1.0f, result[0]);
    EXPECT_FLOAT_EQ(2.0f, result[1]);
    EXPECT_FLOAT_EQ(3.0f, result[2]);
}

TEST(VecTest, UnaryMinus) {
    constexpr Vec<float, 3> vec{ 1, -2, 3 };

    constexpr auto result = -vec;

    EXPECT_FLOAT_EQ(-1.0f, result[0]);
    EXPECT_FLOAT_EQ(2.0f, result[1]);
    EXPECT_FLOAT_EQ(-3.0f, result[2]);
}

// ---------------------------------------------------------------------
// Aliases
// ---------------------------------------------------------------------

TEST(VecTest, VecAliases) {
    static_assert(std::is_same_v<Vec2<double>, Vec<double, 2>>);
    static_assert(std::is_same_v<Vec3<double>, Vec<double, 3>>);
    static_assert(std::is_same_v<Vec4<double>, Vec<double, 4>>);

    static_assert(std::is_same_v<Vec2f, Vec<float, 2>>);
    static_assert(std::is_same_v<Vec3f, Vec<float, 3>>);
    static_assert(std::is_same_v<Vec4f, Vec<float, 4>>);
}

// ---------------------------------------------------------------------
// Common type
// ---------------------------------------------------------------------

TEST(VecTest, CommonType) {
    static_assert(std::is_same_v<VecCommon<float, double, 2>, Vec<double, 2>>);

    static_assert(std::is_same_v<VecCommon<double, float, 3>, Vec<double, 3>>);
}

// ---------------------------------------------------------------------
// Constexpr
// ---------------------------------------------------------------------

TEST(VecTest, ConstexprConstruction) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    static_assert(vec[0] == 1);
    static_assert(vec[1] == 2);
    static_assert(vec[2] == 3);
}

TEST(VecTest, ConstexprArithmetic) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    constexpr auto sum = a + b;
    constexpr auto difference = a - b;
    constexpr auto product = a * b;

    static_assert(sum[0] == 5);
    static_assert(sum[1] == 7);
    static_assert(sum[2] == 9);

    static_assert(difference[0] == -3);
    static_assert(difference[1] == -3);
    static_assert(difference[2] == -3);

    static_assert(product[0] == 4);
    static_assert(product[1] == 10);
    static_assert(product[2] == 18);
}

// ---------------------------------------------------------------------
// Length / Normalize
// ---------------------------------------------------------------------

TEST(VecTest, LengthSquared) {
    constexpr Vec<float, 3> vec{ 3, 4, 0 };

    EXPECT_FLOAT_EQ(25.0f, vec.LengthSquared());
}

TEST(VecTest, Length) {
    const Vec<float, 3> vec{ 3.0f, 4.0f, 0.0f };

    EXPECT_NEAR(5.0f, vec.Length(), 1e-4);
}

TEST(VecTest, LengthOfZeroVector) {
    constexpr Vec<float, 3> vec{ 0.0f, 0.0f, 0.0f };

    EXPECT_NEAR(0.0f, vec.LengthSquared(), 1e-4);
    EXPECT_NEAR(0.0f, vec.Length(), 1e-4);
}

TEST(VecTest, Normalized) {
    const Vec<float, 3> vec{ 3.0f, 4.0f, 0.0f };

    const auto result = vec.Normalized();

    EXPECT_NEAR(1.0f, result.Length(), 1e-4);
    EXPECT_NEAR(0.6f, result[0], 1e-4);
    EXPECT_NEAR(0.8f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

TEST(VecTest, NormalizedDoesNotMutate) {
    const Vec<float, 3> vec{ 3.0f, 4.0f, 0.0f };

    [[maybe_unused]] const auto result = vec.Normalized();

    EXPECT_NEAR(3.0f, vec[0], 1e-4);
    EXPECT_NEAR(4.0f, vec[1], 1e-4);
}

TEST(VecTest, NormalizedZeroVector) {
    constexpr Vec<float, 3> vec{ 0.0f, 0.0f, 0.0f };

    const auto result = vec.Normalized();

    EXPECT_NEAR(0.0f, result[0], 1e-4);
    EXPECT_NEAR(0.0f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

TEST(VecTest, Normalize) {
    Vec<float, 3> vec{ 3.0f, 4.0f, 0.0f };

    vec.Normalize();

    EXPECT_NEAR(1.0f, vec.Length(), 1e-4);
    EXPECT_NEAR(0.6f, vec[0], 1e-4);
    EXPECT_NEAR(0.8f, vec[1], 1e-4);
}

// ---------------------------------------------------------------------
// Dot
// ---------------------------------------------------------------------

TEST(VecTest, Dot) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    EXPECT_FLOAT_EQ(32.0f, Dot(a, b));
}

TEST(VecTest, DotOfOrthogonalVectors) {
    constexpr Vec<float, 3> a{ 1, 0, 0 };
    constexpr Vec<float, 3> b{ 0, 1, 0 };

    EXPECT_FLOAT_EQ(0.0f, Dot(a, b));
}

TEST(VecTest, DotCommonType) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<double, 3> b{ 0.5, 0.5, 0.5 };

    constexpr auto result = Dot(a, b);

    static_assert(std::is_same_v<decltype(result), const double>);

    EXPECT_NEAR(3.0, result, 1e-4);
}

// ---------------------------------------------------------------------
// Cross
// ---------------------------------------------------------------------

TEST(VecTest, Cross) {
    constexpr Vec<float, 3> a{ 1, 0, 0 };
    constexpr Vec<float, 3> b{ 0, 1, 0 };

    constexpr auto result = Cross(a, b);

    EXPECT_FLOAT_EQ(0.0f, result[0]);
    EXPECT_FLOAT_EQ(0.0f, result[1]);
    EXPECT_FLOAT_EQ(1.0f, result[2]);
}

TEST(VecTest, CrossOfParallelVectorsIsZero) {
    constexpr Vec<float, 3> a{ 2, 4, 6 };
    constexpr Vec<float, 3> b{ 1, 2, 3 };

    constexpr auto result = Cross(a, b);

    EXPECT_FLOAT_EQ(0.0f, result[0]);
    EXPECT_FLOAT_EQ(0.0f, result[1]);
    EXPECT_FLOAT_EQ(0.0f, result[2]);
}

TEST(VecTest, CrossIsAnticommutative) {
    constexpr Vec<float, 3> a{ 1, 2, 3 };
    constexpr Vec<float, 3> b{ 4, 5, 6 };

    constexpr auto ab = Cross(a, b);
    constexpr auto ba = Cross(b, a);

    EXPECT_TRUE(ab[0] == -ba[0]);
    EXPECT_TRUE(ab[1] == -ba[1]);
    EXPECT_TRUE(ab[2] == -ba[2]);
}

// ---------------------------------------------------------------------
// Distance
// ---------------------------------------------------------------------

TEST(VecTest, Distance) {
    constexpr Vec<float, 3> a{ 0.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ 3.0f, 4.0f, 0.0f };

    EXPECT_NEAR(5.0f, Distance(a, b), 1e-4);
}

TEST(VecTest, DistanceToSelfIsZero) {
    constexpr Vec<float, 3> a{ 1.0f, 2.0f, 3.0f };

    EXPECT_NEAR(0.0f, Distance(a, a), 1e-4);
}

TEST(VecTest, DistanceIsSymmetric) {
    constexpr Vec<float, 3> a{ 1.0f, 2.0f, 3.0f };
    constexpr Vec<float, 3> b{ 4.0f, 6.0f, 3.0f };

    ExpectNear(Distance(b, a), Distance(a, b));
}

// ---------------------------------------------------------------------
// Angle
// ---------------------------------------------------------------------

TEST(VecTest, AngleBetweenIdenticalVectors) {
    constexpr Vec<float, 3> a{ 1.0f, 0.0f, 0.0f };

    EXPECT_NEAR(0.0f, Angle(a, a), 1e-5f);
}

TEST(VecTest, AngleBetweenPerpendicularVectors) {
    constexpr Vec<float, 3> a{ 1.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ 0.0f, 1.0f, 0.0f };

    EXPECT_NEAR(std::numbers::pi_v<float> / 2.0f, Angle(a, b), 1e-5f);
}

TEST(VecTest, AngleBetweenOpposingVectors) {
    constexpr Vec<float, 3> a{ 1.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ -1.0f, 0.0f, 0.0f };

    EXPECT_NEAR(std::numbers::pi_v<float>, Angle(a, b), 1e-5f);
}

TEST(VecTest, AngleIsScaleInvariant) {
    constexpr Vec<float, 3> a{ 1.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ 0.0f, 5.0f, 0.0f };

    EXPECT_NEAR(std::numbers::pi_v<float> / 2.0f, Angle(a, b), 1e-5f);
}

// ---------------------------------------------------------------------
// Lerp
// ---------------------------------------------------------------------

TEST(VecTest, LerpAtStart) {
    constexpr Vec<float, 3> a{ 0.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ 10.0f, 20.0f, 30.0f };

    const auto result = Lerp(a, b, 0.0f);

    EXPECT_NEAR(0.0f, result[0], 1e-4);
    EXPECT_NEAR(0.0f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

TEST(VecTest, LerpAtEnd) {
    constexpr Vec<float, 3> a{ 0.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ 10.0f, 20.0f, 30.0f };

    const auto result = Lerp(a, b, 1.0f);

    EXPECT_NEAR(10.0f, result[0], 1e-4);
    EXPECT_NEAR(20.0f, result[1], 1e-4);
    EXPECT_NEAR(30.0f, result[2], 1e-4);
}

TEST(VecTest, LerpAtMidpoint) {
    constexpr Vec<float, 3> a{ 0.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> b{ 10.0f, 20.0f, 30.0f };

    const auto result = Lerp(a, b, 0.5f);

    EXPECT_NEAR(5.0f, result[0], 1e-4);
    EXPECT_NEAR(10.0f, result[1], 1e-4);
    EXPECT_NEAR(15.0f, result[2], 1e-4);
}

// ---------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------

TEST(VecTest, Scale) {
    constexpr Vec<float, 3> a{ 2, 3, 4 };
    constexpr Vec<float, 3> b{ 5, 6, 7 };

    constexpr auto result = Scale(a, b);

    EXPECT_FLOAT_EQ(10.0f, result[0]);
    EXPECT_FLOAT_EQ(18.0f, result[1]);
    EXPECT_FLOAT_EQ(28.0f, result[2]);
}

TEST(VecTest, ScaleMatchesComponentMultiplication) {
    constexpr Vec<float, 3> a{ 2, 3, 4 };
    constexpr Vec<float, 3> b{ 5, 6, 7 };

    EXPECT_TRUE(Scale(a, b) == (a * b));
}

// ---------------------------------------------------------------------
// Min / Max
// ---------------------------------------------------------------------

TEST(VecTest, Min) {
    constexpr Vec<float, 3> a{ 1, 5, 3 };
    constexpr Vec<float, 3> b{ 4, 2, 6 };

    constexpr auto result = Min(a, b);

    EXPECT_FLOAT_EQ(1.0f, result[0]);
    EXPECT_FLOAT_EQ(2.0f, result[1]);
    EXPECT_FLOAT_EQ(3.0f, result[2]);
}

TEST(VecTest, Max) {
    constexpr Vec<float, 3> a{ 1, 5, 3 };
    constexpr Vec<float, 3> b{ 4, 2, 6 };

    constexpr auto result = Max(a, b);

    EXPECT_FLOAT_EQ(4.0f, result[0]);
    EXPECT_FLOAT_EQ(5.0f, result[1]);
    EXPECT_FLOAT_EQ(6.0f, result[2]);
}

// ---------------------------------------------------------------------
// MoveTowards
// ---------------------------------------------------------------------

TEST(VecTest, MoveTowardsPartialStep) {
    constexpr Vec<float, 3> current{ 0.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> target{ 10.0f, 0.0f, 0.0f };

    const auto result = MoveTowards(current, target, 4.0f);

    EXPECT_NEAR(4.0f, result[0], 1e-4);
    EXPECT_NEAR(0.0f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

TEST(VecTest, MoveTowardsOvershootClampsToTarget) {
    constexpr Vec<float, 3> current{ 0.0f, 0.0f, 0.0f };
    constexpr Vec<float, 3> target{ 10.0f, 0.0f, 0.0f };

    const auto result = MoveTowards(current, target, 100.0f);

    EXPECT_NEAR(10.0f, result[0], 1e-4);
    EXPECT_NEAR(0.0f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

TEST(VecTest, MoveTowardsSamePointReturnsTarget) {
    constexpr Vec<float, 3> current{ 5.0f, 5.0f, 5.0f };
    constexpr Vec<float, 3> target{ 5.0f, 5.0f, 5.0f };

    const auto result = MoveTowards(current, target, 1.0f);

    EXPECT_NEAR(5.0f, result[0], 1e-4);
    EXPECT_NEAR(5.0f, result[1], 1e-4);
    EXPECT_NEAR(5.0f, result[2], 1e-4);
}

// ---------------------------------------------------------------------
// Reflect
// ---------------------------------------------------------------------

TEST(VecTest, ReflectOffFlatSurface) {
    constexpr Vec<float, 3> v{ 1.0f, -1.0f, 0.0f };
    constexpr Vec<float, 3> normal{ 0.0f, 1.0f, 0.0f };

    const auto result = Reflect(v, normal);

    EXPECT_NEAR(1.0f, result[0], 1e-4);
    EXPECT_NEAR(1.0f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

TEST(VecTest, ReflectStraightOnBouncesBack) {
    constexpr Vec<float, 3> v{ 0.0f, -1.0f, 0.0f };
    constexpr Vec<float, 3> normal{ 0.0f, 1.0f, 0.0f };

    const auto result = Reflect(v, normal);

    EXPECT_NEAR(0.0f, result[0], 1e-4);
    EXPECT_NEAR(1.0f, result[1], 1e-4);
    EXPECT_NEAR(0.0f, result[2], 1e-4);
}

// ---------------------------------------------------------------------
// Perpendicular
// ---------------------------------------------------------------------

TEST(VecTest, Perpendicular) {
    constexpr Vec<float, 2> v{ 1.0f, 0.0f };

    constexpr auto result = Perpendicular(v);

    EXPECT_NEAR(0.0f, result[0], 1e-4);
    EXPECT_NEAR(1.0f, result[1], 1e-4);
}

TEST(VecTest, PerpendicularIsOrthogonal) {
    constexpr Vec<float, 2> v{ 3.0f, 4.0f };

    constexpr auto result = Perpendicular(v);

    EXPECT_NEAR(0.0f, Dot(v, result), 1e-4);
}

TEST(VecTest, PerpendicularPreservesLength) {
    const Vec<float, 2> v{ 3.0f, 4.0f };

    const auto result = Perpendicular(v);

    ExpectNear(v.Length(), result.Length());
}

// ---------------------------------------------------------------------
// Higher dimensions
// ---------------------------------------------------------------------

TEST(VecTest, HigherDimension) {
    Vec<float, 8> vec{ 1, 2, 3, 4, 5, 6, 7, 8 };

    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_FLOAT_EQ(static_cast<float>(i + 1), vec[i]);
    }
}

// ---------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------

TEST(VecTest, Formatting) {
    constexpr Vec<float, 3> vec{ 1, 2, 3 };

    EXPECT_EQ(std::string("(1, 2, 3)"), std::format("{}", vec));
}

TEST(VecTest, FormattingFloatingPoint) {
    constexpr Vec<float, 4> vec{ 1.0f, 2.5f, 3.25f, 4.0f };

    EXPECT_EQ(std::string("(1, 2.5, 3.25, 4)"), std::format("{}", vec));
}
