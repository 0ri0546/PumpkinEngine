#include <gtest/gtest.h>

import Pumpkin.Math.Vec;
import Pumpkin.Math.Quaternion;

import std;

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;

    // Absolute-tolerance comparison that reports the caller's line on failure.
    void ExpectNear(double expected, double actual, double tolerance = 1e-4,
        std::source_location loc = std::source_location::current()) {
        ::testing::ScopedTrace trace(loc.file_name(), static_cast<int>(loc.line()), "ExpectNear");
        EXPECT_NEAR(expected, actual, tolerance);
    }

    template <std::floating_point T>
    void ExpectNear(const Quaternion<T>& a, const Quaternion<T>& b, T tolerance = static_cast<T>(1e-4),
        std::source_location loc = std::source_location::current()) {
        ExpectNear(a.X(), b.X(), tolerance, loc);
        ExpectNear(a.Y(), b.Y(), tolerance, loc);
        ExpectNear(a.Z(), b.Z(), tolerance, loc);
        ExpectNear(a.W(), b.W(), tolerance, loc);
    }

    template <std::floating_point T>
    void ExpectNear(const Vec3<T>& a, const Vec3<T>& b, T tolerance = static_cast<T>(1e-4),
        std::source_location loc = std::source_location::current()) {
        ExpectNear(a[0], b[0], tolerance, loc);
        ExpectNear(a[1], b[1], tolerance, loc);
        ExpectNear(a[2], b[2], tolerance, loc);
    }
} // namespace

// ---------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------

TEST(QuaternionTest, DefaultConstruction) {
    constexpr Quaternion<float> q{};

    EXPECT_NEAR(0.0f, q.X(), 1e-4);
    EXPECT_NEAR(0.0f, q.Y(), 1e-4);
    EXPECT_NEAR(0.0f, q.Z(), 1e-4);
    EXPECT_NEAR(1.0f, q.W(), 1e-4);
}

TEST(QuaternionTest, ValueConstruction) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    EXPECT_NEAR(1.0f, q.X(), 1e-4);
    EXPECT_NEAR(2.0f, q.Y(), 1e-4);
    EXPECT_NEAR(3.0f, q.Z(), 1e-4);
    EXPECT_NEAR(4.0f, q.W(), 1e-4);
}

TEST(QuaternionTest, Identity) {
    constexpr Quaternion<float> q = Quaternion<float>::Identity();

    EXPECT_NEAR(0.0f, q.X(), 1e-4);
    EXPECT_NEAR(0.0f, q.Y(), 1e-4);
    EXPECT_NEAR(0.0f, q.Z(), 1e-4);
    EXPECT_NEAR(1.0f, q.W(), 1e-4);
}

TEST(QuaternionTest, FromAxisAngle) {
    constexpr Vec3<float> axis{ 0.0f, 1.0f, 0.0f };
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(axis, kPi / 2.0f);

    EXPECT_NEAR(0.0f, q.X(), 1e-4f);
    EXPECT_NEAR(std::sin(kPi / 4.0f), q.Y(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Z(), 1e-4f);
    EXPECT_NEAR(std::cos(kPi / 4.0f), q.W(), 1e-4f);
}

TEST(QuaternionTest, FromAxisAngleNormalizesAxis) {
    constexpr Vec3<float> axis{ 0.0f, 5.0f, 0.0f };
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(axis, kPi / 2.0f);

    EXPECT_NEAR(1.0f, q.Length(), 1e-4f);
    EXPECT_NEAR(std::sin(kPi / 4.0f), q.Y(), 1e-4f);
}

TEST(QuaternionTest, FromAxisAngleZeroAxis) {
    constexpr Vec3<float> axis{ 0.0f, 0.0f, 0.0f };
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(axis, kPi / 2.0f);

    EXPECT_NEAR(0.0f, q.X(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Y(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Z(), 1e-4f);
}

TEST(QuaternionTest, AxisAngleConstructor) {
    constexpr Vec3<float> axis{ 0.0f, 0.0f, 1.0f };
    const Quaternion<float> q{ axis, kPi };

    ExpectNear(q, Quaternion<float>::FromAxisAngle(axis, kPi));
}

TEST(QuaternionTest, FromEulerIdentity) {
    const Quaternion<float> q = Quaternion<float>::FromEuler(0.0f, 0.0f, 0.0f);

    ExpectNear(q, Quaternion<float>::Identity());
}

TEST(QuaternionTest, FromEulerMatchesAxisAngleForSingleAxis) {
    constexpr float angle = kPi / 3.0f;

    const Quaternion<float> fromEuler = Quaternion<float>::FromEuler(angle, 0.0f, 0.0f);
    const Quaternion<float> fromAxisAngle =
        Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 0.0f, 0.0f}, angle);

    ExpectNear(fromEuler, fromAxisAngle);
}

TEST(QuaternionTest, MixedValueTypes) {
    constexpr Quaternion<double> q{ 1, 2.5, 3, 4 };

    EXPECT_NEAR(1.0, q.X(), 1e-9);
    EXPECT_NEAR(2.5, q.Y(), 1e-9);
    EXPECT_NEAR(3.0, q.Z(), 1e-9);
    EXPECT_NEAR(4.0, q.W(), 1e-9);
}

// ---------------------------------------------------------------------
// FromToRotation
// ---------------------------------------------------------------------

TEST(QuaternionTest, FromToRotationIdenticalVectors) {
    constexpr Vec3<float> v{ 1.0f, 0.0f, 0.0f };

    const Quaternion<float> q = Quaternion<float>::FromToRotation(v, v);

    ExpectNear(q, Quaternion<float>::Identity());
}

TEST(QuaternionTest, FromToRotationOpposingVectors) {
    constexpr Vec3<float> from{ 1.0f, 0.0f, 0.0f };
    constexpr Vec3<float> to{ -1.0f, 0.0f, 0.0f };

    const Quaternion<float> q = Quaternion<float>::FromToRotation(from, to);

    EXPECT_NEAR(1.0f, q.Length(), 1e-4f);

    const Vec3<float> rotated = q.RotateVector(from);

    ExpectNear(rotated, to);
}

TEST(QuaternionTest, FromToRotationRotatesFromOntoTo) {
    constexpr Vec3<float> from{ 1.0f, 0.0f, 0.0f };
    constexpr Vec3<float> to{ 0.0f, 1.0f, 0.0f };

    const Quaternion<float> q = Quaternion<float>::FromToRotation(from, to);
    const Vec3<float> rotated = q.RotateVector(from);

    ExpectNear(rotated, to);
}

TEST(QuaternionTest, FromToRotationIsNormalized) {
    constexpr Vec3<float> from{ 1.0f, 0.0f, 0.0f };
    constexpr Vec3<float> to{ 0.0f, 0.0f, 1.0f };

    const Quaternion<float> q = Quaternion<float>::FromToRotation(from, to);

    EXPECT_NEAR(1.0f, q.Length(), 1e-4f);
}

// ---------------------------------------------------------------------
// LookRotation
// ---------------------------------------------------------------------

TEST(QuaternionTest, LookRotationIdentityForForwardZ) {
    constexpr Vec3<float> forward{ 0.0f, 0.0f, 1.0f };
    constexpr Vec3<float> up{ 0.0f, 1.0f, 0.0f };

    const Quaternion<float> q = Quaternion<float>::LookRotation(forward, up);

    ExpectNear(q, Quaternion<float>::Identity());
}

TEST(QuaternionTest, LookRotationRotatesForwardVector) {
    constexpr Vec3<float> forward{ 1.0f, 0.0f, 0.0f };
    constexpr Vec3<float> up{ 0.0f, 1.0f, 0.0f };

    const Quaternion<float> q = Quaternion<float>::LookRotation(forward, up);
    const Vec3<float> rotated = q.RotateVector(Vec3<float>{0.0f, 0.0f, 1.0f});

    ExpectNear(rotated, forward);
}

TEST(QuaternionTest, LookRotationIsNormalized) {
    constexpr Vec3<float> forward{ 0.0f, 0.0f, 1.0f };
    constexpr Vec3<float> up{ 0.0f, 1.0f, 0.0f };

    const Quaternion<float> q = Quaternion<float>::LookRotation(forward, up);

    EXPECT_NEAR(1.0f, q.Length(), 1e-4f);
}

// ---------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------

TEST(QuaternionTest, ComponentAccess) {
    Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    q.X() = 10.0f;
    q.Y() = 20.0f;
    q.Z() = 30.0f;
    q.W() = 40.0f;

    EXPECT_NEAR(10.0f, q.X(), 1e-4);
    EXPECT_NEAR(20.0f, q.Y(), 1e-4);
    EXPECT_NEAR(30.0f, q.Z(), 1e-4);
    EXPECT_NEAR(40.0f, q.W(), 1e-4);
}

TEST(QuaternionTest, ConstComponentAccess) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    EXPECT_NEAR(1.0f, q.X(), 1e-4);
    EXPECT_NEAR(2.0f, q.Y(), 1e-4);
    EXPECT_NEAR(3.0f, q.Z(), 1e-4);
    EXPECT_NEAR(4.0f, q.W(), 1e-4);
}

TEST(QuaternionTest, Data) {
    Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    float* data = q.Data();

    ASSERT_NE(data, nullptr);

    data[0] = 10.0f;
    data[1] = 20.0f;
    data[2] = 30.0f;
    data[3] = 40.0f;

    EXPECT_NEAR(10.0f, q.X(), 1e-4);
    EXPECT_NEAR(20.0f, q.Y(), 1e-4);
    EXPECT_NEAR(30.0f, q.Z(), 1e-4);
    EXPECT_NEAR(40.0f, q.W(), 1e-4);
}

TEST(QuaternionTest, Size) {
    EXPECT_EQ(static_cast<std::size_t>(4), static_cast<std::size_t>((Quaternion<float>::Size())));
    EXPECT_EQ(static_cast<std::size_t>(4), static_cast<std::size_t>((Quaternion<double>::Size())));
}

// ---------------------------------------------------------------------
// Equality
// ---------------------------------------------------------------------

TEST(QuaternionTest, Equality) {
    constexpr Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<float> b{ 1.0f, 2.0f, 3.0f, 4.0f };

    EXPECT_TRUE(a == b);
}

TEST(QuaternionTest, Inequality) {
    constexpr Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<float> b{ 1.0f, 2.0f, 3.0f, 5.0f };

    EXPECT_FALSE(a == b);
}

TEST(QuaternionTest, EqualityDifferentTypes) {
    constexpr Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<double> b{ 1.0, 2.0, 3.0, 4.0 };

    EXPECT_TRUE(a == b);
}

// ---------------------------------------------------------------------
// Addition
// ---------------------------------------------------------------------

TEST(QuaternionTest, Addition) {
    constexpr Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<float> b{ 4.0f, 5.0f, 6.0f, 7.0f };

    constexpr auto result = a + b;

    EXPECT_NEAR(5.0f, result.X(), 1e-4);
    EXPECT_NEAR(7.0f, result.Y(), 1e-4);
    EXPECT_NEAR(9.0f, result.Z(), 1e-4);
    EXPECT_NEAR(11.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, AdditionCommonType) {
    constexpr Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<double> b{ 0.5, 1.5, 2.5, 3.5 };

    constexpr auto result = a + b;

    static_assert(std::is_same_v<decltype(result), const Quaternion<double>>);

    EXPECT_NEAR(1.5, result.X(), 1e-9);
    EXPECT_NEAR(3.5, result.Y(), 1e-9);
    EXPECT_NEAR(5.5, result.Z(), 1e-9);
    EXPECT_NEAR(7.5, result.W(), 1e-9);
}

TEST(QuaternionTest, AdditionAssignment) {
    Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<float> b{ 4.0f, 5.0f, 6.0f, 7.0f };

    a += b;

    EXPECT_NEAR(5.0f, a.X(), 1e-4);
    EXPECT_NEAR(7.0f, a.Y(), 1e-4);
    EXPECT_NEAR(9.0f, a.Z(), 1e-4);
    EXPECT_NEAR(11.0f, a.W(), 1e-4);
}

TEST(QuaternionTest, MixedTypeAdditionAssignment) {
    Quaternion<double> a{ 1.0, 2.0, 3.0, 4.0 };
    constexpr Quaternion<float> b{ 4.0f, 5.0f, 6.0f, 7.0f };

    a += b;

    EXPECT_NEAR(5.0, a.X(), 1e-9);
    EXPECT_NEAR(7.0, a.Y(), 1e-9);
    EXPECT_NEAR(9.0, a.Z(), 1e-9);
    EXPECT_NEAR(11.0, a.W(), 1e-9);
}

// ---------------------------------------------------------------------
// Subtraction
// ---------------------------------------------------------------------

TEST(QuaternionTest, Subtraction) {
    constexpr Quaternion<float> a{ 5.0f, 7.0f, 9.0f, 11.0f };
    constexpr Quaternion<float> b{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = a - b;

    EXPECT_NEAR(4.0f, result.X(), 1e-4);
    EXPECT_NEAR(5.0f, result.Y(), 1e-4);
    EXPECT_NEAR(6.0f, result.Z(), 1e-4);
    EXPECT_NEAR(7.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, SubtractionCommonType) {
    constexpr Quaternion<float> a{ 5.0f, 7.0f, 9.0f, 11.0f };
    constexpr Quaternion<double> b{ 0.5, 1.5, 2.5, 3.5 };

    constexpr auto result = a - b;

    static_assert(std::is_same_v<decltype(result), const Quaternion<double>>);

    EXPECT_NEAR(4.5, result.X(), 1e-9);
    EXPECT_NEAR(5.5, result.Y(), 1e-9);
    EXPECT_NEAR(6.5, result.Z(), 1e-9);
    EXPECT_NEAR(7.5, result.W(), 1e-9);
}

TEST(QuaternionTest, SubtractionAssignment) {
    Quaternion<float> a{ 5.0f, 7.0f, 9.0f, 11.0f };
    constexpr Quaternion<float> b{ 1.0f, 2.0f, 3.0f, 4.0f };

    a -= b;

    EXPECT_NEAR(4.0f, a.X(), 1e-4);
    EXPECT_NEAR(5.0f, a.Y(), 1e-4);
    EXPECT_NEAR(6.0f, a.Z(), 1e-4);
    EXPECT_NEAR(7.0f, a.W(), 1e-4);
}

// ---------------------------------------------------------------------
// Hamilton product
// ---------------------------------------------------------------------

TEST(QuaternionTest, MultiplicationByIdentity) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<float> identity = Quaternion<float>::Identity();

    constexpr auto result = q * identity;

    ExpectNear(q.X(), result.X());
    ExpectNear(q.Y(), result.Y());
    ExpectNear(q.Z(), result.Z());
    ExpectNear(q.W(), result.W());
}

TEST(QuaternionTest, MultiplicationIsNonCommutative) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 0.0f, 0.0f}, kPi / 2.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);

    const Quaternion<float> ab = a * b;
    const Quaternion<float> ba = b * a;

    EXPECT_FALSE((ab.X() == ba.X()) && (ab.Y() == ba.Y()) && (ab.Z() == ba.Z()) && (ab.W() == ba.W()));
}

TEST(QuaternionTest, MultiplicationCombinesRotations) {
    // Two 90 degree rotations around Z should equal one 180 degree rotation around Z.
    const Quaternion<float> quarterTurn =
        Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    const Quaternion<float> halfTurn = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi);

    const Quaternion<float> combined = quarterTurn * quarterTurn;

    ExpectNear(combined, halfTurn);
}

TEST(QuaternionTest, MultiplicationCommonType) {
    constexpr Quaternion<float> a{ 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr Quaternion<double> b{ 1.0, 0.0, 0.0, 0.0 };

    constexpr auto result = a * b;

    static_assert(std::is_same_v<decltype(result), const Quaternion<double>>);

    EXPECT_NEAR(1.0, result.X(), 1e-9);
    EXPECT_NEAR(0.0, result.Y(), 1e-9);
    EXPECT_NEAR(0.0, result.Z(), 1e-9);
    EXPECT_NEAR(0.0, result.W(), 1e-9);
}

TEST(QuaternionTest, MultiplicationAssignment) {
    Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    a *= b;

    ExpectNear(a, Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi));
}

// ---------------------------------------------------------------------
// Scalar multiplication
// ---------------------------------------------------------------------

TEST(QuaternionTest, ScalarMultiplication) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = q * 2.0f;

    EXPECT_NEAR(2.0f, result.X(), 1e-4);
    EXPECT_NEAR(4.0f, result.Y(), 1e-4);
    EXPECT_NEAR(6.0f, result.Z(), 1e-4);
    EXPECT_NEAR(8.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, ScalarMultiplicationCommonType) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = q * 0.5;

    static_assert(std::is_same_v<decltype(result), const Quaternion<double>>);

    EXPECT_NEAR(0.5, result.X(), 1e-9);
    EXPECT_NEAR(1.0, result.Y(), 1e-9);
    EXPECT_NEAR(1.5, result.Z(), 1e-9);
    EXPECT_NEAR(2.0, result.W(), 1e-9);
}

TEST(QuaternionTest, ScalarMultiplicationReversed) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = 2.0f * q;

    EXPECT_NEAR(2.0f, result.X(), 1e-4);
    EXPECT_NEAR(4.0f, result.Y(), 1e-4);
    EXPECT_NEAR(6.0f, result.Z(), 1e-4);
    EXPECT_NEAR(8.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, ScalarMultiplicationAssignment) {
    Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    q *= 3.0f;

    EXPECT_NEAR(3.0f, q.X(), 1e-4);
    EXPECT_NEAR(6.0f, q.Y(), 1e-4);
    EXPECT_NEAR(9.0f, q.Z(), 1e-4);
    EXPECT_NEAR(12.0f, q.W(), 1e-4);
}

// ---------------------------------------------------------------------
// Scalar division
// ---------------------------------------------------------------------

TEST(QuaternionTest, ScalarDivision) {
    constexpr Quaternion<float> q{ 2.0f, 4.0f, 6.0f, 8.0f };

    constexpr auto result = q / 2.0f;

    EXPECT_NEAR(1.0f, result.X(), 1e-4);
    EXPECT_NEAR(2.0f, result.Y(), 1e-4);
    EXPECT_NEAR(3.0f, result.Z(), 1e-4);
    EXPECT_NEAR(4.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, ScalarDivisionCommonType) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = q / 2.0;

    static_assert(std::is_same_v<decltype(result), const Quaternion<double>>);

    EXPECT_NEAR(0.5, result.X(), 1e-9);
    EXPECT_NEAR(1.0, result.Y(), 1e-9);
    EXPECT_NEAR(1.5, result.Z(), 1e-9);
    EXPECT_NEAR(2.0, result.W(), 1e-9);
}

TEST(QuaternionTest, ScalarDivisionAssignment) {
    Quaternion<float> q{ 2.0f, 4.0f, 6.0f, 8.0f };

    q /= 2.0f;

    EXPECT_NEAR(1.0f, q.X(), 1e-4);
    EXPECT_NEAR(2.0f, q.Y(), 1e-4);
    EXPECT_NEAR(3.0f, q.Z(), 1e-4);
    EXPECT_NEAR(4.0f, q.W(), 1e-4);
}

// ---------------------------------------------------------------------
// Unary operators
// ---------------------------------------------------------------------

TEST(QuaternionTest, UnaryPlus) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = +q;

    EXPECT_NEAR(1.0f, result.X(), 1e-4);
    EXPECT_NEAR(2.0f, result.Y(), 1e-4);
    EXPECT_NEAR(3.0f, result.Z(), 1e-4);
    EXPECT_NEAR(4.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, UnaryMinus) {
    constexpr Quaternion<float> q{ 1.0f, -2.0f, 3.0f, -4.0f };

    constexpr auto result = -q;

    EXPECT_NEAR(-1.0f, result.X(), 1e-4);
    EXPECT_NEAR(2.0f, result.Y(), 1e-4);
    EXPECT_NEAR(-3.0f, result.Z(), 1e-4);
    EXPECT_NEAR(4.0f, result.W(), 1e-4);
}

// ---------------------------------------------------------------------
// Length / normalization
// ---------------------------------------------------------------------

TEST(QuaternionTest, LengthSquared) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 2.0f, 4.0f };

    EXPECT_NEAR(25.0f, q.LengthSquared(), 1e-4);
}

TEST(QuaternionTest, Length) {
    constexpr Quaternion<float> q{ 0.0f, 0.0f, 0.0f, 3.0f };

    EXPECT_NEAR(3.0f, q.Length(), 1e-4);
}

TEST(QuaternionTest, IdentityLengthIsOne) {
    constexpr Quaternion<float> q = Quaternion<float>::Identity();

    EXPECT_NEAR(1.0f, q.Length(), 1e-4);
}

TEST(QuaternionTest, Normalized) {
    constexpr Quaternion<float> q{ 0.0f, 0.0f, 0.0f, 2.0f };

    const Quaternion<float> result = q.Normalized();

    EXPECT_NEAR(1.0f, result.Length(), 1e-4);
    EXPECT_NEAR(1.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, NormalizedDoesNotMutate) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    const Quaternion<float> result = q.Normalized();

    EXPECT_NEAR(1.0f, q.X(), 1e-4);
    EXPECT_NEAR(2.0f, q.Y(), 1e-4);
    EXPECT_NEAR(3.0f, q.Z(), 1e-4);
    EXPECT_NEAR(4.0f, q.W(), 1e-4);
    EXPECT_NEAR(1.0f, result.Length(), 1e-4f);
}

TEST(QuaternionTest, NormalizedZeroQuaternion) {
    constexpr Quaternion<float> q{ 0.0f, 0.0f, 0.0f, 0.0f };

    const Quaternion<float> result = q.Normalized();

    EXPECT_NEAR(0.0f, result.X(), 1e-4);
    EXPECT_NEAR(0.0f, result.Y(), 1e-4);
    EXPECT_NEAR(0.0f, result.Z(), 1e-4);
    EXPECT_NEAR(0.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, Normalize) {
    Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    q.Normalize();

    EXPECT_NEAR(1.0f, q.Length(), 1e-4f);
}

// ---------------------------------------------------------------------
// Conjugate / inverse
// ---------------------------------------------------------------------

TEST(QuaternionTest, Conjugate) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    constexpr auto result = q.Conjugate();

    EXPECT_NEAR(-1.0f, result.X(), 1e-4);
    EXPECT_NEAR(-2.0f, result.Y(), 1e-4);
    EXPECT_NEAR(-3.0f, result.Z(), 1e-4);
    EXPECT_NEAR(4.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, InverseOfUnitQuaternionEqualsConjugate) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 3.0f);

    ExpectNear(q.Inverse(), q.Conjugate());
}

TEST(QuaternionTest, InverseTimesSelfIsIdentity) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 1.0f, 0.0f}, kPi / 4.0f);

    const Quaternion<float> result = q * q.Inverse();

    ExpectNear(result, Quaternion<float>::Identity());
}

TEST(QuaternionTest, InverseOfZeroQuaternionReturnsConjugate) {
    constexpr Quaternion<float> q{ 0.0f, 0.0f, 0.0f, 0.0f };

    constexpr auto result = q.Inverse();

    EXPECT_NEAR(0.0f, result.X(), 1e-4);
    EXPECT_NEAR(0.0f, result.Y(), 1e-4);
    EXPECT_NEAR(0.0f, result.Z(), 1e-4);
    EXPECT_NEAR(0.0f, result.W(), 1e-4);
}

TEST(QuaternionTest, InverseOfNonUnitQuaternion) {
    constexpr Quaternion<float> q{ 0.0f, 0.0f, 0.0f, 2.0f };

    constexpr Quaternion<float> result = q.Inverse();

    EXPECT_NEAR(0.5f, result.W(), 1e-4f);
}

// ---------------------------------------------------------------------
// Vector rotation
// ---------------------------------------------------------------------

TEST(QuaternionTest, RotateVectorByIdentity) {
    constexpr Quaternion<float> identity = Quaternion<float>::Identity();
    constexpr Vec3<float> v{ 1.0f, 2.0f, 3.0f };

    constexpr Vec3<float> result = identity.RotateVector(v);

    ExpectNear(result, v);
}

TEST(QuaternionTest, RotateVectorNinetyDegreesAroundZ) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    constexpr Vec3<float> v{ 1.0f, 0.0f, 0.0f };

    const Vec3<float> result = q.RotateVector(v);

    ExpectNear(result, Vec3<float>{0.0f, 1.0f, 0.0f});
}

TEST(QuaternionTest, RotateVectorOneEightyAroundX) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 0.0f, 0.0f}, kPi);
    constexpr Vec3<float> v{ 0.0f, 1.0f, 0.0f };

    const Vec3<float> result = q.RotateVector(v);

    ExpectNear(result, Vec3<float>{0.0f, -1.0f, 0.0f});
}

TEST(QuaternionTest, RotateVectorPreservesLength) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 1.0f, 1.0f}, kPi / 5.0f);
    constexpr Vec3<float> v{ 3.0f, 4.0f, 0.0f };

    const Vec3<float> result = q.RotateVector(v);

    const float originalLength = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    const float rotatedLength = std::sqrt(result[0] * result[0] + result[1] * result[1] + result[2] * result[2]);

    ExpectNear(rotatedLength, originalLength, 1e-4f);
}

TEST(QuaternionTest, OperatorMultiplyRotatesVector) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    constexpr Vec3<float> v{ 1.0f, 0.0f, 0.0f };

    const Vec3<float> result = q * v;

    ExpectNear(result, q.RotateVector(v));
}

TEST(QuaternionTest, OperatorMultiplyRotateVectorCommonType) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    constexpr Vec3<double> v{ 1.0, 0.0, 0.0 };

    const auto result = q * v;

    static_assert(std::is_same_v<decltype(result), const Vec3<double>>);

    EXPECT_NEAR(0.0, result[0], 1e-4);
    EXPECT_NEAR(1.0, result[1], 1e-4);
}

// ---------------------------------------------------------------------
// Euler conversions
// ---------------------------------------------------------------------

TEST(QuaternionTest, EulerRoundTripPitch) {
    const Quaternion<float> q = Quaternion<float>::FromEuler(kPi / 6.0f, 0.0f, 0.0f);

    EXPECT_NEAR(kPi / 6.0f, q.Pitch(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Yaw(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Roll(), 1e-4f);
}

TEST(QuaternionTest, EulerRoundTripYaw) {
    const Quaternion<float> q = Quaternion<float>::FromEuler(0.0f, kPi / 6.0f, 0.0f);

    EXPECT_NEAR(0.0f, q.Pitch(), 1e-4f);
    EXPECT_NEAR(kPi / 6.0f, q.Yaw(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Roll(), 1e-4f);
}

TEST(QuaternionTest, EulerRoundTripRoll) {
    const Quaternion<float> q = Quaternion<float>::FromEuler(0.0f, 0.0f, kPi / 6.0f);

    EXPECT_NEAR(0.0f, q.Pitch(), 1e-4f);
    EXPECT_NEAR(0.0f, q.Yaw(), 1e-4f);
    EXPECT_NEAR(kPi / 6.0f, q.Roll(), 1e-4f);
}

TEST(QuaternionTest, ToEulerMatchesComponentAccessors) {
    const Quaternion<float> q = Quaternion<float>::FromEuler(kPi / 8.0f, kPi / 5.0f, kPi / 7.0f);

    const Vec3<float> euler = q.ToEuler();

    ExpectNear(q.Pitch(), euler[0], 1e-4f);
    ExpectNear(q.Yaw(), euler[1], 1e-4f);
    ExpectNear(q.Roll(), euler[2], 1e-4f);
}

TEST(QuaternionTest, YawClampsAtGimbalLock) {
    const Quaternion<float> q = Quaternion<float>::FromEuler(0.0f, kPi / 2.0f, 0.0f);

    EXPECT_NEAR(kPi / 2.0f, q.Yaw(), 1e-3f);
}

// ---------------------------------------------------------------------
// ToAxisAngle
// ---------------------------------------------------------------------

TEST(QuaternionTest, ToAxisAngleRoundTripsFromAxisAngle) {
    constexpr Vec3<float> axis{ 0.0f, 1.0f, 0.0f };
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(axis, kPi / 2.0f);

    Vec3<float> outAxis;
    float outAngle{};
    q.ToAxisAngle(outAxis, outAngle);

    ExpectNear(outAxis, axis);
    EXPECT_NEAR(kPi / 2.0f, outAngle, 1e-4f);
}

TEST(QuaternionTest, ToAxisAngleIdentityHasZeroAngle) {
    const Quaternion<float> q = Quaternion<float>::Identity();

    Vec3<float> outAxis;
    float outAngle{};
    q.ToAxisAngle(outAxis, outAngle);

    EXPECT_NEAR(0.0f, outAngle, 1e-4f);
}

TEST(QuaternionTest, ToAxisAngleFullTurn) {
    constexpr Vec3<float> axis{ 1.0f, 0.0f, 0.0f };
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(axis, kPi);

    Vec3<float> outAxis;
    float outAngle{};
    q.ToAxisAngle(outAxis, outAngle);

    ExpectNear(outAxis, axis);
    EXPECT_NEAR(kPi, outAngle, 1e-4f);
}

// ---------------------------------------------------------------------
// Dot / Lerp / Slerp
// ---------------------------------------------------------------------

TEST(QuaternionTest, DotOfIdenticalQuaternionsEqualsLengthSquared) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    ExpectNear(q.LengthSquared(), Dot(q, q));
}

TEST(QuaternionTest, DotOfOrthogonalQuaternionsIsZero) {
    constexpr Quaternion<float> a{ 1.0f, 0.0f, 0.0f, 0.0f };
    constexpr Quaternion<float> b{ 0.0f, 1.0f, 0.0f, 0.0f };

    EXPECT_NEAR(0.0f, Dot(a, b), 1e-4);
}

TEST(QuaternionTest, LerpAtStart) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    const Quaternion<float> result = Lerp(a, b, 0.0f);

    ExpectNear(result, a);
}

TEST(QuaternionTest, LerpAtEnd) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    const Quaternion<float> result = Lerp(a, b, 1.0f);

    ExpectNear(result, b);
}

TEST(QuaternionTest, LerpIsNormalized) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 0.0f, 0.0f}, kPi / 2.0f);

    const Quaternion<float> result = Lerp(a, b, 0.5f);

    EXPECT_NEAR(1.0f, result.Length(), 1e-4f);
}

TEST(QuaternionTest, SlerpAtStart) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);

    const Quaternion<float> result = Slerp(a, b, 0.0f);

    ExpectNear(result, a);
}

TEST(QuaternionTest, SlerpAtEnd) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);

    const Quaternion<float> result = Slerp(a, b, 1.0f);

    ExpectNear(result, b);
}

TEST(QuaternionTest, SlerpAtMidpointMatchesHalfAngle) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);
    const Quaternion<float> expected =
        Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 4.0f);

    const Quaternion<float> result = Slerp(a, b, 0.5f);

    ExpectNear(result, expected);
}

TEST(QuaternionTest, SlerpIsNormalized) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{1.0f, 0.0f, 0.0f}, kPi / 2.0f);

    const Quaternion<float> result = Slerp(a, b, 0.3f);

    EXPECT_NEAR(1.0f, result.Length(), 1e-4f);
}

TEST(QuaternionTest, SlerpTakesShortestPath) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    b = -b; // Negate to represent the same rotation via the opposite hemisphere.

    const Quaternion<float> result = Slerp(a, b, 0.5f);
    const Quaternion<float> expected =
        Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 4.0f);

    ExpectNear(result, expected);
}

TEST(QuaternionTest, SlerpNearIdenticalQuaternionsFallsBackToLerp) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 1e-7f);

    const Quaternion<float> result = Slerp(a, b, 0.5f);

    EXPECT_NEAR(1.0f, result.Length(), 1e-4f);
}

// ---------------------------------------------------------------------
// LerpUnclamped / SlerpUnclamped
// ---------------------------------------------------------------------

TEST(QuaternionTest, LerpUnclampedMatchesLerpWithinRange) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    ExpectNear(LerpUnclamped(a, b, 0.5f), Lerp(a, b, 0.5f));
}

TEST(QuaternionTest, LerpUnclampedExtrapolatesPastEnd) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    const Quaternion<float> clamped = Lerp(a, b, 1.5f);
    const Quaternion<float> unclamped = LerpUnclamped(a, b, 1.5f);

    EXPECT_NEAR(1.0f, unclamped.Length(), 1e-4f);
    EXPECT_TRUE(std::abs(unclamped.Z() - clamped.Z()) > 1e-4f);
}

TEST(QuaternionTest, SlerpUnclampedMatchesSlerpWithinRange) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);

    ExpectNear(SlerpUnclamped(a, b, 0.5f), Slerp(a, b, 0.5f));
}

TEST(QuaternionTest, SlerpUnclampedIsNormalizedPastEnd) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, 0.0f);
    const Quaternion<float> b = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);

    const Quaternion<float> result = SlerpUnclamped(a, b, 1.5f);

    EXPECT_NEAR(1.0f, result.Length(), 1e-4f);
}

// ---------------------------------------------------------------------
// Angle
// ---------------------------------------------------------------------

TEST(QuaternionTest, AngleBetweenIdenticalQuaternionsIsZero) {
    const Quaternion<float> q = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 3.0f);

    EXPECT_NEAR(0.0f, Angle(q, q), 1e-4f);
}

TEST(QuaternionTest, AngleBetweenIdentityAndRotatedQuaternion) {
    const Quaternion<float> identity = Quaternion<float>::Identity();
    const Quaternion<float> rotated = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);

    EXPECT_NEAR(kPi / 2.0f, Angle(identity, rotated), 1e-4f);
}

TEST(QuaternionTest, AngleIsSignInvariant) {
    const Quaternion<float> a = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 1.0f, 0.0f}, kPi / 2.0f);
    const Quaternion<float> b = -a;

    // acos loses precision near its domain boundary, so this needs a looser tolerance than usual.
    EXPECT_NEAR(0.0f, Angle(a, b), 1e-2f);
}

// ---------------------------------------------------------------------
// RotateTowards
// ---------------------------------------------------------------------

TEST(QuaternionTest, RotateTowardsPartialStep) {
    const Quaternion<float> from = Quaternion<float>::Identity();
    const Quaternion<float> to = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    const Quaternion<float> result = RotateTowards(from, to, kPi / 4.0f);

    EXPECT_NEAR(kPi / 4.0f, Angle(from, result), 1e-3f);
}

TEST(QuaternionTest, RotateTowardsOvershootClampsToTarget) {
    const Quaternion<float> from = Quaternion<float>::Identity();
    const Quaternion<float> to = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 2.0f);

    const Quaternion<float> result = RotateTowards(from, to, kPi);

    ExpectNear(result, to);
}

TEST(QuaternionTest, RotateTowardsSameRotationReturnsTarget) {
    const Quaternion<float> from = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 3.0f);
    const Quaternion<float> to = Quaternion<float>::FromAxisAngle(Vec3<float>{0.0f, 0.0f, 1.0f}, kPi / 3.0f);

    const Quaternion<float> result = RotateTowards(from, to, kPi / 4.0f);

    ExpectNear(result, to);
}

// ---------------------------------------------------------------------
// Aliases
// ---------------------------------------------------------------------

TEST(QuaternionTest, QuaternionAliases) {
    static_assert(std::is_same_v<Quaternionf, Quaternion<float>>);
    static_assert(std::is_same_v<Quaterniond, Quaternion<double>>);
}

// ---------------------------------------------------------------------
// Common type
// ---------------------------------------------------------------------

TEST(QuaternionTest, CommonType) {
    static_assert(std::is_same_v<QuaternionCommon<float, double>, Quaternion<double>>);
}

// ---------------------------------------------------------------------
// Constexpr
// ---------------------------------------------------------------------

TEST(QuaternionTest, ConstexprConstruction) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    static_assert(q.X() == 1.0f);
    static_assert(q.Y() == 2.0f);
    static_assert(q.Z() == 3.0f);
    static_assert(q.W() == 4.0f);
}

TEST(QuaternionTest, ConstexprArithmetic) {
    constexpr Quaternion<float> a{ 1.0f, 2.0f, 3.0f, 4.0f };
    constexpr Quaternion<float> b{ 4.0f, 5.0f, 6.0f, 7.0f };

    constexpr auto sum = a + b;
    constexpr auto difference = a - b;

    static_assert(sum.X() == 5.0f);
    static_assert(sum.Y() == 7.0f);
    static_assert(sum.Z() == 9.0f);
    static_assert(sum.W() == 11.0f);

    static_assert(difference.X() == -3.0f);
    static_assert(difference.Y() == -3.0f);
    static_assert(difference.Z() == -3.0f);
    static_assert(difference.W() == -3.0f);
}

// ---------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------

TEST(QuaternionTest, Formatting) {
    constexpr Quaternion<float> q{ 1.0f, 2.0f, 3.0f, 4.0f };

    EXPECT_EQ(std::string("(1, 2, 3, 4)"), std::format("{}", q));
}

TEST(QuaternionTest, FormattingFloatingPoint) {
    constexpr Quaternion<float> q{ 1.0f, 2.5f, 3.25f, 4.0f };

    EXPECT_EQ(std::string("(1, 2.5, 3.25, 4)"), std::format("{}", q));
}
