#include <gtest/gtest.h>

import Pumpkin.Math.Mat;
import Pumpkin.Math.Quaternion;
import Pumpkin.Math.Vec;

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

TEST(MatTest, DefaultConstruction) {
    constexpr Mat<float, 2, 3> mat;

    for (std::size_t row = 0; row < 2; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            EXPECT_FLOAT_EQ(0.0f, (mat[row, col]));
        }
    }
}

TEST(MatTest, ValueConstruction) {
    constexpr Mat<float, 2, 3> mat{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };

    EXPECT_FLOAT_EQ(1.0f, (mat[0, 0]));
    EXPECT_FLOAT_EQ(2.0f, (mat[0, 1]));
    EXPECT_FLOAT_EQ(3.0f, (mat[0, 2]));

    EXPECT_FLOAT_EQ(4.0f, (mat[1, 0]));
    EXPECT_FLOAT_EQ(5.0f, (mat[1, 1]));
    EXPECT_FLOAT_EQ(6.0f, (mat[1, 2]));
}

TEST(MatTest, ValueConstructionWithConversion) {
    constexpr Mat<float, 2, 2> mat{ 1.0f, 2.0f, 3.0f, 4.0f };

    EXPECT_FLOAT_EQ(1.0f, (mat[0, 0]));
    EXPECT_FLOAT_EQ(2.0f, (mat[0, 1]));
    EXPECT_FLOAT_EQ(3.0f, (mat[1, 0]));
    EXPECT_FLOAT_EQ(4.0f, (mat[1, 1]));
}

// ---------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------

TEST(MatTest, BracketAccess) {
    Mat<float, 3, 3> mat{};

    mat[1, 2] = 42.0f;

    EXPECT_FLOAT_EQ(42.0f, (mat[1, 2]));
}

TEST(MatTest, ParenthesisAccess) {
    Mat<float, 3, 3> mat{};

    mat(2, 1) = 42.0f;

    EXPECT_FLOAT_EQ(42.0f, (mat(2, 1)));
}

TEST(MatTest, ConstAccess) {
    constexpr Mat<float, 2, 2> mat{ 1.0f, 2.0f, 3.0f, 4.0f };

    EXPECT_FLOAT_EQ(1.0f, (mat[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (mat[1, 1]));
}

TEST(MatTest, Data) {
    Mat<float, 2, 3> mat{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };

    ASSERT_NE(mat.Data(), nullptr);

    mat.Data()[0] = 42.0f;

    EXPECT_FLOAT_EQ(42.0f, (mat[0, 0]));
}

// ---------------------------------------------------------------------
// Dimensions
// ---------------------------------------------------------------------

TEST(MatTest, Dimensions) {
    using Matrix = Mat<float, 3, 4>;

    EXPECT_EQ(static_cast<std::size_t>(3), Matrix::Rows());
    EXPECT_EQ(static_cast<std::size_t>(4), Matrix::Cols());
    EXPECT_EQ(static_cast<std::size_t>(12), Matrix::Size());
}

TEST(MatTest, DimensionsAreCompileTimeConstants) {
    using Matrix = Mat<float, 3, 4>;

    static_assert(Matrix::Rows() == 3);
    static_assert(Matrix::Cols() == 4);
    static_assert(Matrix::Size() == 12);
}

// ---------------------------------------------------------------------
// MDSpan
// ---------------------------------------------------------------------

TEST(MatTest, MDSpan) {
    Mat<float, 2, 3> mat{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };

    const auto span = mat.MDSpan();

    EXPECT_EQ(static_cast<std::size_t>(2), span.extent(0));
    EXPECT_EQ(static_cast<std::size_t>(3), span.extent(1));

    EXPECT_FLOAT_EQ(1.0f, (span[0, 0]));
    EXPECT_FLOAT_EQ(3.0f, (span[0, 2]));
    EXPECT_FLOAT_EQ(4.0f, (span[1, 0]));
    EXPECT_FLOAT_EQ(6.0f, (span[1, 2]));

    span[1, 1] = 42.0f;

    EXPECT_FLOAT_EQ(42.0f, (mat[1, 1]));
}

TEST(MatTest, ConstMDSpan) {
    constexpr Mat<float, 2, 2> mat{ 1.0f, 2.0f, 3.0f, 4.0f };

    const auto span = mat.MDSpan();

    EXPECT_EQ(static_cast<std::size_t>(2), span.extent(0));
    EXPECT_EQ(static_cast<std::size_t>(2), span.extent(1));

    EXPECT_FLOAT_EQ(1.0f, (span[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (span[1, 1]));
}

// ---------------------------------------------------------------------
// Equality
// ---------------------------------------------------------------------

TEST(MatTest, Equality) {
    constexpr Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr Mat<float, 2, 2> b{ 1, 2, 3, 4 };

    constexpr Mat<float, 2, 2> c{ 1, 2, 3, 5 };

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(MatTest, EqualityDifferentNumericTypes) {
    constexpr Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr Mat<double, 2, 2> b{ 1.0, 2.0, 3.0, 4.0 };

    EXPECT_TRUE(a == b);
}

TEST(MatTest, EqualityDifferentLayouts) {
    constexpr MatRowMajor<float, 2, 2> row{ 1, 2, 3, 4 };

    constexpr MatColumnMajor<float, 2, 2> column{ 1, 2, 3, 4 };

    EXPECT_TRUE(row == column);
}

// ---------------------------------------------------------------------
// Addition
// ---------------------------------------------------------------------

TEST(MatTest, Addition) {
    constexpr Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr Mat<float, 2, 2> b{ 5, 6, 7, 8 };

    constexpr auto result = a + b;

    static_assert(std::is_same_v<decltype(result), const Mat<float, 2, 2>>);

    EXPECT_FLOAT_EQ(6.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(8.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(10.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(12.0f, (result[1, 1]));
}

TEST(MatTest, AdditionDifferentNumericTypes) {
    constexpr Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr Mat<double, 2, 2> b{ 0.5, 1.5, 2.5, 3.5 };

    constexpr auto result = a + b;

    static_assert(std::is_same_v<decltype(result), const Mat<double, 2, 2>>);

    EXPECT_NEAR(1.5, (result[0, 0]), 1e-4);
    EXPECT_NEAR(3.5, (result[0, 1]), 1e-4);
    EXPECT_NEAR(5.5, (result[1, 0]), 1e-4);
    EXPECT_NEAR(7.5, (result[1, 1]), 1e-4);
}

TEST(MatTest, AdditionAssignment) {
    Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr Mat<float, 2, 2> b{ 5, 6, 7, 8 };

    a += b;

    EXPECT_FLOAT_EQ(6.0f, (a[0, 0]));
    EXPECT_FLOAT_EQ(8.0f, (a[0, 1]));
    EXPECT_FLOAT_EQ(10.0f, (a[1, 0]));
    EXPECT_FLOAT_EQ(12.0f, (a[1, 1]));
}

// ---------------------------------------------------------------------
// Subtraction
// ---------------------------------------------------------------------

TEST(MatTest, Subtraction) {
    constexpr Mat<float, 2, 2> a{ 5, 6, 7, 8 };

    constexpr Mat<float, 2, 2> b{ 1, 2, 3, 4 };

    constexpr auto result = a - b;

    EXPECT_FLOAT_EQ(4.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(4.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[1, 1]));
}

TEST(MatTest, SubtractionAssignment) {
    Mat<float, 2, 2> a{ 5, 6, 7, 8 };

    constexpr Mat<float, 2, 2> b{ 1, 2, 3, 4 };

    a -= b;

    EXPECT_FLOAT_EQ(4.0f, (a[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (a[0, 1]));
    EXPECT_FLOAT_EQ(4.0f, (a[1, 0]));
    EXPECT_FLOAT_EQ(4.0f, (a[1, 1]));
}

// ---------------------------------------------------------------------
// Unary operators
// ---------------------------------------------------------------------

TEST(MatTest, UnaryPlus) {
    constexpr Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr auto result = +a;

    EXPECT_TRUE(result == a);
}

TEST(MatTest, UnaryMinus) {
    constexpr Mat<float, 2, 2> a{ 1, -2, 3, -4 };

    constexpr auto result = -a;

    EXPECT_FLOAT_EQ(-1.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(2.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(-3.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[1, 1]));
}

// ---------------------------------------------------------------------
// Scalar multiplication
// ---------------------------------------------------------------------

TEST(MatTest, ScalarMultiplication) {
    constexpr Mat<float, 2, 2> mat{ 1, 2, 3, 4 };

    constexpr auto result = mat * 2.0f;

    EXPECT_FLOAT_EQ(2.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(6.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(8.0f, (result[1, 1]));
}

TEST(MatTest, ScalarMultiplicationReversed) {
    constexpr Mat<float, 2, 2> mat{ 1, 2, 3, 4 };

    constexpr auto result = 2.0f * mat;

    EXPECT_FLOAT_EQ(2.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(6.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(8.0f, (result[1, 1]));
}

TEST(MatTest, ScalarMultiplicationDifferentType) {
    constexpr Mat<float, 2, 2> mat{ 1, 2, 3, 4 };

    constexpr auto result = mat * 0.5;

    static_assert(std::is_same_v<decltype(result), const Mat<double, 2, 2>>);

    EXPECT_NEAR(0.5, (result[0, 0]), 1e-4);
    EXPECT_NEAR(1.0, (result[0, 1]), 1e-4);
    EXPECT_NEAR(1.5, (result[1, 0]), 1e-4);
    EXPECT_NEAR(2.0, (result[1, 1]), 1e-4);
}

TEST(MatTest, ScalarMultiplicationAssignment) {
    Mat<float, 2, 2> mat{ 1, 2, 3, 4 };

    mat *= 2.0f;

    EXPECT_FLOAT_EQ(2.0f, (mat[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (mat[0, 1]));
    EXPECT_FLOAT_EQ(6.0f, (mat[1, 0]));
    EXPECT_FLOAT_EQ(8.0f, (mat[1, 1]));
}

// ---------------------------------------------------------------------
// Scalar division
// ---------------------------------------------------------------------

TEST(MatTest, ScalarDivision) {
    constexpr Mat<float, 2, 2> mat{ 2, 4, 6, 8 };

    constexpr auto result = mat / 2.0f;

    EXPECT_NEAR(1.0f, (result[0, 0]), 1e-4);
    EXPECT_NEAR(2.0f, (result[0, 1]), 1e-4);
    EXPECT_NEAR(3.0f, (result[1, 0]), 1e-4);
    EXPECT_NEAR(4.0f, (result[1, 1]), 1e-4);
}

TEST(MatTest, ScalarDivisionDifferentType) {
    constexpr Mat<float, 2, 2> mat{ 2, 4, 6, 8 };

    constexpr auto result = mat / 2.0;

    static_assert(std::is_same_v<decltype(result), const Mat<double, 2, 2>>);

    EXPECT_NEAR(1.0, (result[0, 0]), 1e-4);
    EXPECT_NEAR(2.0, (result[0, 1]), 1e-4);
    EXPECT_NEAR(3.0, (result[1, 0]), 1e-4);
    EXPECT_NEAR(4.0, (result[1, 1]), 1e-4);
}

TEST(MatTest, ScalarDivisionAssignment) {
    Mat<float, 2, 2> mat{ 2, 4, 6, 8 };

    mat /= 2.0f;

    EXPECT_NEAR(1.0f, (mat[0, 0]), 1e-4);
    EXPECT_NEAR(2.0f, (mat[0, 1]), 1e-4);
    EXPECT_NEAR(3.0f, (mat[1, 0]), 1e-4);
    EXPECT_NEAR(4.0f, (mat[1, 1]), 1e-4);
}

// ---------------------------------------------------------------------
// Matrix multiplication
// ---------------------------------------------------------------------

TEST(MatTest, MatrixMultiplication) {
    constexpr Mat<float, 2, 3> a{ 1, 2, 3, 4, 5, 6 };

    constexpr Mat<float, 3, 2> b{ 7, 8, 9, 10, 11, 12 };

    constexpr auto result = a * b;

    static_assert(std::is_same_v<decltype(result), const Mat<float, 2, 2>>);

    EXPECT_FLOAT_EQ(58.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(64.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(139.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(154.0f, (result[1, 1]));
}

TEST(MatTest, MatrixMultiplicationDifferentNumericTypes) {
    constexpr Mat<float, 2, 3> a{ 1, 2, 3, 4, 5, 6 };

    constexpr Mat<double, 3, 2> b{ 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };

    constexpr auto result = a * b;

    static_assert(std::is_same_v<decltype(result), const Mat<double, 2, 2>>);

    EXPECT_NEAR(22.0, (result[0, 0]), 1e-4);
    EXPECT_NEAR(28.0, (result[0, 1]), 1e-4);
    EXPECT_NEAR(49.0, (result[1, 0]), 1e-4);
    EXPECT_NEAR(64.0, (result[1, 1]), 1e-4);
}

TEST(MatTest, SquareMatrixMultiplication) {
    constexpr Mat<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr Mat<float, 2, 2> b{ 5, 6, 7, 8 };

    constexpr auto result = a * b;

    EXPECT_FLOAT_EQ(19.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(22.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(43.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(50.0f, (result[1, 1]));
}

// ---------------------------------------------------------------------
// Transpose
// ---------------------------------------------------------------------

TEST(MatTest, Transpose) {
    constexpr Mat<float, 2, 3> mat{ 1, 2, 3, 4, 5, 6 };

    constexpr auto result = mat.Transpose();

    static_assert(std::is_same_v<decltype(result), const Mat<float, 3, 2>>);

    EXPECT_FLOAT_EQ(1.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[0, 1]));

    EXPECT_FLOAT_EQ(2.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(5.0f, (result[1, 1]));

    EXPECT_FLOAT_EQ(3.0f, (result[2, 0]));
    EXPECT_FLOAT_EQ(6.0f, (result[2, 1]));
}

TEST(MatTest, SquareTranspose) {
    constexpr Mat<float, 3, 3> mat{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    constexpr auto result = mat.Transpose();

    EXPECT_FLOAT_EQ(1.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(4.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(7.0f, (result[0, 2]));

    EXPECT_FLOAT_EQ(2.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(5.0f, (result[1, 1]));
    EXPECT_FLOAT_EQ(8.0f, (result[1, 2]));

    EXPECT_FLOAT_EQ(3.0f, (result[2, 0]));
    EXPECT_FLOAT_EQ(6.0f, (result[2, 1]));
    EXPECT_FLOAT_EQ(9.0f, (result[2, 2]));
}

// ---------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------

TEST(MatTest, Identity2x2) {
    constexpr auto identity = SquareMatrix<float, 2>::Identity();

    EXPECT_FLOAT_EQ(1.0f, (identity[0, 0]));
    EXPECT_FLOAT_EQ(0.0f, (identity[0, 1]));
    EXPECT_FLOAT_EQ(0.0f, (identity[1, 0]));
    EXPECT_FLOAT_EQ(1.0f, (identity[1, 1]));
}

TEST(MatTest, Identity4x4) {
    constexpr auto identity = SquareMatrix<float, 4>::Identity();

    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            if (row == col) {
                EXPECT_NEAR(1.0f, (identity[row, col]), 1e-4);
            }
            else {
                EXPECT_NEAR(0.0f, (identity[row, col]), 1e-4);
            }
        }
    }
}

TEST(MatTest, IdentityIsNeutralForMultiplication) {
    constexpr Mat<float, 3, 3> mat{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    constexpr auto identity = SquareMatrix<float, 3>::Identity();

    EXPECT_TRUE(mat * identity == mat);
    EXPECT_TRUE(identity * mat == mat);
}

// ---------------------------------------------------------------------
// Determinant
// ---------------------------------------------------------------------

TEST(MatTest, Determinant1x1) {
    constexpr Mat<float, 1, 1> mat{ 5.0f };

    EXPECT_NEAR(5.0f, mat.Determinant(), 1e-4);
}

TEST(MatTest, Determinant2x2) {
    constexpr Mat<float, 2, 2> mat{ 1, 2, 3, 4 };

    EXPECT_NEAR(-2.0f, mat.Determinant(), 1e-4);
}

TEST(MatTest, Determinant3x3) {
    constexpr Mat<float, 3, 3> mat{ 6, 1, 1, 4, -2, 5, 2, 8, 7 };

    EXPECT_NEAR(-306.0f, mat.Determinant(), 1e-4);
}

TEST(MatTest, DeterminantIdentity) {
    constexpr auto identity = SquareMatrix<float, 4>::Identity();

    EXPECT_NEAR(1.0f, identity.Determinant(), 1e-4);
}

TEST(MatTest, DeterminantSingular) {
    constexpr Mat<float, 2, 2> mat{ 1, 2, 2, 4 };

    EXPECT_NEAR(0.0f, mat.Determinant(), 1e-4);
}

// ---------------------------------------------------------------------
// Inverse
// ---------------------------------------------------------------------

TEST(MatTest, Inverse2x2) {
    constexpr Mat<float, 2, 2> mat{ 4.0, 7.0, 2.0, 6.0 };

    constexpr auto inverse = mat.Inverse();

    EXPECT_NEAR(0.6, (inverse[0, 0]), 1e-5);
    EXPECT_NEAR(-0.7, (inverse[0, 1]), 1e-5);
    EXPECT_NEAR(-0.2, (inverse[1, 0]), 1e-5);
    EXPECT_NEAR(0.4, (inverse[1, 1]), 1e-5);
}

TEST(MatTest, InverseIdentity) {
    constexpr auto identity = SquareMatrix<float, 4>::Identity();

    constexpr auto inverse = identity.Inverse();

    EXPECT_TRUE(inverse == identity);
}

TEST(MatTest, MatrixTimesInverseIsIdentity) {
    constexpr Mat<float, 2, 2> mat{ 4.0, 7.0, 2.0, 6.0 };

    constexpr auto inverse = mat.Inverse();
    constexpr auto result = mat * inverse;

    constexpr auto identity = SquareMatrix<float, 2>::Identity();

    for (std::size_t row = 0; row < 2; ++row) {
        for (std::size_t col = 0; col < 2; ++col) {
            ExpectNear((identity[row, col]), (result[row, col]), 1e-5);
        }
    }
}

TEST(MatTest, SingularMatrixInverse) {
    constexpr Mat<float, 2, 2> mat{ 1.0, 2.0, 2.0, 4.0 };

    constexpr auto inverse = mat.Inverse();
    constexpr auto b = Mat<float, 2, 2>{};

    EXPECT_TRUE(inverse == b);
}

// ---------------------------------------------------------------------
// Zero
// ---------------------------------------------------------------------

TEST(MatTest, Zero) {
    constexpr auto mat = Mat4f::Zero();

    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            EXPECT_NEAR(0.0f, (mat[row, col]), 1e-4);
        }
    }
}

// ---------------------------------------------------------------------
// Rotation factories
// ---------------------------------------------------------------------

TEST(MatTest, RotationXRotatesYIntoZ) {
    const Mat4f rot = Mat4f::RotationX(kPi / 2.0f);
    constexpr Vec3f v{ 0.0f, 1.0f, 0.0f };

    const Vec3f result = rot.MultiplyVector(v);

    ExpectNear(result, Vec3f{ 0.0f, 0.0f, 1.0f });
}

TEST(MatTest, RotationYRotatesZIntoX) {
    const Mat4f rot = Mat4f::RotationY(kPi / 2.0f);
    constexpr Vec3f v{ 0.0f, 0.0f, 1.0f };

    const Vec3f result = rot.MultiplyVector(v);

    ExpectNear(result, Vec3f{ 1.0f, 0.0f, 0.0f });
}

TEST(MatTest, RotationZRotatesXIntoY) {
    const Mat4f rot = Mat4f::RotationZ(kPi / 2.0f);
    constexpr Vec3f v{ 1.0f, 0.0f, 0.0f };

    const Vec3f result = rot.MultiplyVector(v);

    ExpectNear(result, Vec3f{ 0.0f, 1.0f, 0.0f });
}

TEST(MatTest, RotationXByZeroIsIdentity) {
    const Mat4f rot = Mat4f::RotationX(0.0f);
    const Mat4f identity = Mat4f::Identity();

    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            ExpectNear((identity[row, col]), (rot[row, col]), 1e-6f);
        }
    }
}

// ---------------------------------------------------------------------
// Scale / Translate factories
// ---------------------------------------------------------------------

TEST(MatTest, ScaleFactoryScalesPoint) {
    const Mat4f mat = Mat4f::Scale(Vec3f{ 2.0f, 3.0f, 4.0f });
    constexpr Vec3f point{ 1.0f, 1.0f, 1.0f };

    const Vec3f result = mat.MultiplyPoint(point);

    ExpectNear(result, Vec3f{ 2.0f, 3.0f, 4.0f });
}

TEST(MatTest, TranslateFactoryTranslatesPoint) {
    const Mat4f mat = Mat4f::Translate(Vec3f{ 1.0f, 2.0f, 3.0f });
    constexpr Vec3f point{ 0.0f, 0.0f, 0.0f };

    const Vec3f result = mat.MultiplyPoint(point);

    ExpectNear(result, Vec3f{ 1.0f, 2.0f, 3.0f });
}

TEST(MatTest, TranslateFactoryDoesNotAffectVector) {
    const Mat4f mat = Mat4f::Translate(Vec3f{ 1.0f, 2.0f, 3.0f });
    constexpr Vec3f direction{ 5.0f, 0.0f, 0.0f };

    const Vec3f result = mat.MultiplyVector(direction);

    ExpectNear(result, direction);
}

// ---------------------------------------------------------------------
// Rotate (from quaternion)
// ---------------------------------------------------------------------

TEST(MatTest, RotateMatchesQuaternionRotateVector) {
    const Quaternionf rotation = Quaternionf::FromAxisAngle(Vec3f{ 0.0f, 1.0f, 0.0f }, kPi / 3.0f);
    const Mat4f mat = Mat4f::Rotate(rotation);

    constexpr Vec3f v{ 1.0f, 0.0f, 0.0f };

    ExpectNear(mat.MultiplyVector(v), rotation.RotateVector(v));
}

TEST(MatTest, RotateIdentityQuaternionIsIdentityMatrix) {
    const Mat4f mat = Mat4f::Rotate(Quaternionf::Identity());
    const Mat4f identity = Mat4f::Identity();

    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            ExpectNear((identity[row, col]), (mat[row, col]), 1e-6f);
        }
    }
}

// ---------------------------------------------------------------------
// TRS
// ---------------------------------------------------------------------

TEST(MatTest, TRSAppliesScaleRotateTranslateInOrder) {
    constexpr Vec3f translation{ 10.0f, 0.0f, 0.0f };
    const Quaternionf rotation = Quaternionf::Identity();
    constexpr Vec3f scale{ 2.0f, 1.0f, 1.0f };

    const Mat4f trs = Mat4f::TRS(translation, rotation, scale);
    constexpr Vec3f localPoint{ 1.0f, 0.0f, 0.0f };

    const Vec3f worldPoint = trs.MultiplyPoint(localPoint);

    ExpectNear(worldPoint, Vec3f{ 12.0f, 0.0f, 0.0f });
}

TEST(MatTest, TRSWithIdentityComponentsIsIdentity) {
    const Mat4f trs = Mat4f::TRS(Vec3f{ 0.0f, 0.0f, 0.0f }, Quaternionf::Identity(), Vec3f{ 1.0f, 1.0f, 1.0f });
    const Mat4f identity = Mat4f::Identity();

    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            ExpectNear((identity[row, col]), (trs[row, col]), 1e-6f);
        }
    }
}

// ---------------------------------------------------------------------
// Perspective / Ortho / LookAt
// ---------------------------------------------------------------------

TEST(MatTest, PerspectiveMapsNearPlaneToNegativeOneNDC) {
    const Mat4f persp = Mat4f::Perspective(kPi / 2.0f, 1.0f, 0.1f, 100.0f);

    const Vec4f nearPoint{ 0.0f, 0.0f, -0.1f, 1.0f };
    const Vec4f clip = persp * nearPoint;

    EXPECT_NEAR(-1.0f, clip[2] / clip[3], 1e-4f);
}

TEST(MatTest, PerspectiveMapsFarPlaneToPositiveOneNDC) {
    const Mat4f persp = Mat4f::Perspective(kPi / 2.0f, 1.0f, 0.1f, 100.0f);

    const Vec4f farPoint{ 0.0f, 0.0f, -100.0f, 1.0f };
    const Vec4f clip = persp * farPoint;

    EXPECT_NEAR(1.0f, clip[2] / clip[3], 1e-4f);
}

TEST(MatTest, OrthoMapsNearPlaneToNegativeOne) {
    const Mat4f ortho = Mat4f::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);

    const Vec3f nearPoint{ 0.0f, 0.0f, -0.1f };
    const Vec3f mapped = ortho.MultiplyPoint(nearPoint);

    EXPECT_NEAR(-1.0f, mapped[2], 1e-4f);
}

TEST(MatTest, OrthoMapsFarPlaneToPositiveOne) {
    const Mat4f ortho = Mat4f::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);

    const Vec3f farPoint{ 0.0f, 0.0f, -100.0f };
    const Vec3f mapped = ortho.MultiplyPoint(farPoint);

    EXPECT_NEAR(1.0f, mapped[2], 1e-4f);
}

TEST(MatTest, OrthoMapsSideBoundsToUnitRange) {
    const Mat4f ortho = Mat4f::Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);

    const Vec3f corner{ 1.0f, 1.0f, -0.1f };
    const Vec3f mapped = ortho.MultiplyPoint(corner);

    EXPECT_NEAR(1.0f, mapped[0], 1e-4f);
    EXPECT_NEAR(1.0f, mapped[1], 1e-4f);
}

TEST(MatTest, LookAtMapsEyeToOrigin) {
    constexpr Vec3f eye{ 0.0f, 0.0f, 5.0f };
    constexpr Vec3f target{ 0.0f, 0.0f, 0.0f };
    constexpr Vec3f up{ 0.0f, 1.0f, 0.0f };

    const Mat4f lookAt = Mat4f::LookAt(eye, target, up);
    const Vec3f eyeInView = lookAt.MultiplyPoint(eye);

    ExpectNear(eyeInView, Vec3f{ 0.0f, 0.0f, 0.0f });
}

TEST(MatTest, LookAtRotationIsOrthonormal) {
    constexpr Vec3f eye{ 0.0f, 0.0f, 5.0f };
    constexpr Vec3f target{ 0.0f, 0.0f, 0.0f };
    constexpr Vec3f up{ 0.0f, 1.0f, 0.0f };

    const Mat4f lookAt = Mat4f::LookAt(eye, target, up);

    const Vec3f r0{ lookAt[0, 0], lookAt[0, 1], lookAt[0, 2] };
    const Vec3f r1{ lookAt[1, 0], lookAt[1, 1], lookAt[1, 2] };
    const Vec3f r2{ lookAt[2, 0], lookAt[2, 1], lookAt[2, 2] };

    EXPECT_NEAR(1.0f, r0.Length(), 1e-4f);
    EXPECT_NEAR(1.0f, r1.Length(), 1e-4f);
    EXPECT_NEAR(1.0f, r2.Length(), 1e-4f);

    EXPECT_NEAR(0.0f, Dot(r0, r1), 1e-4f);
    EXPECT_NEAR(0.0f, Dot(r0, r2), 1e-4f);
    EXPECT_NEAR(0.0f, Dot(r1, r2), 1e-4f);
}

// ---------------------------------------------------------------------
// MultiplyPoint / MultiplyVector
// ---------------------------------------------------------------------

TEST(MatTest, MultiplyPointAppliesTranslation) {
    const Mat4f mat = Mat4f::Translate(Vec3f{ 1.0f, 2.0f, 3.0f });
    constexpr Vec3f point{ 1.0f, 1.0f, 1.0f };

    const Vec3f result = mat.MultiplyPoint(point);

    ExpectNear(result, Vec3f{ 2.0f, 3.0f, 4.0f });
}

TEST(MatTest, MultiplyVectorIgnoresTranslation) {
    const Mat4f mat = Mat4f::Translate(Vec3f{ 1.0f, 2.0f, 3.0f });
    constexpr Vec3f vector{ 1.0f, 1.0f, 1.0f };

    const Vec3f result = mat.MultiplyVector(vector);

    ExpectNear(result, vector);
}

// ---------------------------------------------------------------------
// Extract Position / Scale / Rotation
// ---------------------------------------------------------------------

TEST(MatTest, ExtractPositionFromTRS) {
    constexpr Vec3f translation{ 1.0f, 2.0f, 3.0f };
    const Mat4f trs = Mat4f::TRS(translation, Quaternionf::Identity(), Vec3f{ 1.0f, 1.0f, 1.0f });

    ExpectNear(trs.ExtractPosition(), translation);
}

TEST(MatTest, ExtractScaleFromTRS) {
    constexpr Vec3f scale{ 2.0f, 3.0f, 4.0f };
    const Mat4f trs = Mat4f::TRS(Vec3f{ 0.0f, 0.0f, 0.0f }, Quaternionf::Identity(), scale);

    ExpectNear(trs.ExtractScale(), scale);
}

TEST(MatTest, ExtractRotationFromTRS) {
    const Quaternionf rotation = Quaternionf::FromAxisAngle(Vec3f{ 0.0f, 1.0f, 0.0f }, kPi / 4.0f);
    const Mat4f trs = Mat4f::TRS(Vec3f{ 0.0f, 0.0f, 0.0f }, rotation, Vec3f{ 1.0f, 1.0f, 1.0f });

    const Quaternionf extracted = trs.ExtractRotation();

    ExpectNear(rotation.X(), extracted.X(), 1e-4f);
    ExpectNear(rotation.Y(), extracted.Y(), 1e-4f);
    ExpectNear(rotation.Z(), extracted.Z(), 1e-4f);
    ExpectNear(rotation.W(), extracted.W(), 1e-4f);
}

TEST(MatTest, ExtractRoundTripsFullTRS) {
    constexpr Vec3f translation{ 5.0f, -2.0f, 1.0f };
    const Quaternionf rotation = Quaternionf::FromAxisAngle(Vec3f{ 1.0f, 0.0f, 0.0f }, kPi / 6.0f);
    constexpr Vec3f scale{ 2.0f, 1.0f, 3.0f };

    const Mat4f trs = Mat4f::TRS(translation, rotation, scale);

    ExpectNear(trs.ExtractPosition(), translation);
    ExpectNear(trs.ExtractScale(), scale);
}

// ---------------------------------------------------------------------
// Matrix-vector multiplication
// ---------------------------------------------------------------------

TEST(MatTest, IdentityTimesVectorIsUnchanged) {
    const Mat4f identity = Mat4f::Identity();
    constexpr Vec4f v{ 1.0f, 2.0f, 3.0f, 1.0f };

    const Vec4f result = identity * v;

    EXPECT_NEAR(1.0f, result[0], 1e-4);
    EXPECT_NEAR(2.0f, result[1], 1e-4);
    EXPECT_NEAR(3.0f, result[2], 1e-4);
    EXPECT_NEAR(1.0f, result[3], 1e-4);
}

TEST(MatTest, MatrixTimesVectorAppliesTransform) {
    const Mat4f translate = Mat4f::Translate(Vec3f{ 1.0f, 2.0f, 3.0f });
    constexpr Vec4f v{ 0.0f, 0.0f, 0.0f, 1.0f };

    const Vec4f result = translate * v;

    EXPECT_NEAR(1.0f, result[0], 1e-4);
    EXPECT_NEAR(2.0f, result[1], 1e-4);
    EXPECT_NEAR(3.0f, result[2], 1e-4);
    EXPECT_NEAR(1.0f, result[3], 1e-4);
}

// ---------------------------------------------------------------------
// ToRadians
// ---------------------------------------------------------------------

TEST(MatTest, ToRadiansZero) {
    EXPECT_NEAR(0.0f, ToRadians(0.0f), 1e-4);
}

TEST(MatTest, ToRadiansOneEighty) {
    EXPECT_NEAR(kPi, ToRadians(180.0f), 1e-4);
}

TEST(MatTest, ToRadiansNinety) {
    EXPECT_NEAR(kPi / 2.0f, ToRadians(90.0f), 1e-5f);
}

// ---------------------------------------------------------------------
// ValidTRS
// ---------------------------------------------------------------------

TEST(MatTest, ValidTRSAcceptsIdentity) {
    EXPECT_TRUE(ValidTRS(Mat4f::Identity()));
}

TEST(MatTest, ValidTRSAcceptsProperTRS) {
    const Mat4f trs =
        Mat4f::TRS(Vec3f{ 1.0f, 2.0f, 3.0f }, Quaternionf::FromAxisAngle(Vec3f{ 0.0f, 1.0f, 0.0f }, kPi / 4.0f),
            Vec3f{ 2.0f, 2.0f, 2.0f });

    EXPECT_TRUE(ValidTRS(trs));
}

TEST(MatTest, ValidTRSRejectsZeroMatrix) {
    EXPECT_FALSE(ValidTRS(Mat4f::Zero()));
}

TEST(MatTest, ValidTRSRejectsNonAffineBottomRow) {
    Mat4f mat = Mat4f::Identity();
    mat[3, 0] = 1.0f;

    EXPECT_FALSE(ValidTRS(mat));
}

// ---------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------

TEST(MatTest, RowMajorMemoryLayout) {
    constexpr MatRowMajor<float, 2, 3> mat{ 1, 2, 3, 4, 5, 6 };

    EXPECT_FLOAT_EQ(1.0f, mat.Data()[0]);
    EXPECT_FLOAT_EQ(2.0f, mat.Data()[1]);
    EXPECT_FLOAT_EQ(3.0f, mat.Data()[2]);
    EXPECT_FLOAT_EQ(4.0f, mat.Data()[3]);
    EXPECT_FLOAT_EQ(5.0f, mat.Data()[4]);
    EXPECT_FLOAT_EQ(6.0f, mat.Data()[5]);
}

TEST(MatTest, ColumnMajorMemoryLayout) {
    constexpr MatColumnMajor<float, 2, 3> mat{ 1, 2, 3, 4, 5, 6 };

    EXPECT_FLOAT_EQ(1.0f, mat.Data()[0]);
    EXPECT_FLOAT_EQ(4.0f, mat.Data()[1]);
    EXPECT_FLOAT_EQ(2.0f, mat.Data()[2]);
    EXPECT_FLOAT_EQ(5.0f, mat.Data()[3]);
    EXPECT_FLOAT_EQ(3.0f, mat.Data()[4]);
    EXPECT_FLOAT_EQ(6.0f, mat.Data()[5]);
}

TEST(MatTest, LayoutDoesNotChangeMathematicalIndexing) {
    constexpr MatRowMajor<float, 2, 3> row{ 1, 2, 3, 4, 5, 6 };

    constexpr MatColumnMajor<float, 2, 3> column{ 1, 2, 3, 4, 5, 6 };

    for (std::size_t r = 0; r < 2; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            EXPECT_TRUE((row[r, c]) == (column[r, c]));
        }
    }
}

// ---------------------------------------------------------------------
// Layout conversion
// ---------------------------------------------------------------------

TEST(MatTest, RowToColumnMajorConversion) {
    constexpr MatRowMajor<float, 2, 3> row{ 1, 2, 3, 4, 5, 6 };

    constexpr auto column = MatCastLayout<float, 2, 3, RowMajor, ColumnMajor>(row);

    EXPECT_FLOAT_EQ(1.0f, (column[0, 0]));
    EXPECT_FLOAT_EQ(2.0f, (column[0, 1]));
    EXPECT_FLOAT_EQ(3.0f, (column[0, 2]));

    EXPECT_FLOAT_EQ(4.0f, (column[1, 0]));
    EXPECT_FLOAT_EQ(5.0f, (column[1, 1]));
    EXPECT_FLOAT_EQ(6.0f, (column[1, 2]));

    EXPECT_FLOAT_EQ(1.0f, column.Data()[0]);
    EXPECT_FLOAT_EQ(4.0f, column.Data()[1]);
    EXPECT_FLOAT_EQ(2.0f, column.Data()[2]);
    EXPECT_FLOAT_EQ(5.0f, column.Data()[3]);
    EXPECT_FLOAT_EQ(3.0f, column.Data()[4]);
    EXPECT_FLOAT_EQ(6.0f, column.Data()[5]);
}

TEST(MatTest, ColumnToRowMajorConversion) {
    constexpr MatColumnMajor<float, 2, 3> column{ 1, 2, 3, 4, 5, 6 };

    constexpr auto row = MatCastLayout<float, 2, 3, ColumnMajor, RowMajor>(column);

    EXPECT_FLOAT_EQ(1.0f, (row[0, 0]));
    EXPECT_FLOAT_EQ(2.0f, (row[0, 1]));
    EXPECT_FLOAT_EQ(3.0f, (row[0, 2]));

    EXPECT_FLOAT_EQ(4.0f, (row[1, 0]));
    EXPECT_FLOAT_EQ(5.0f, (row[1, 1]));
    EXPECT_FLOAT_EQ(6.0f, (row[1, 2]));

    EXPECT_FLOAT_EQ(1.0f, row.Data()[0]);
    EXPECT_FLOAT_EQ(2.0f, row.Data()[1]);
    EXPECT_FLOAT_EQ(3.0f, row.Data()[2]);
    EXPECT_FLOAT_EQ(4.0f, row.Data()[3]);
    EXPECT_FLOAT_EQ(5.0f, row.Data()[4]);
    EXPECT_FLOAT_EQ(6.0f, row.Data()[5]);
}

TEST(MatTest, LayoutConversionConstructor) {
    constexpr MatRowMajor<float, 2, 2> row{ 1, 2, 3, 4 };

    constexpr MatColumnMajor<float, 2, 2> column{ row };

    EXPECT_FLOAT_EQ(1.0f, (column[0, 0]));
    EXPECT_FLOAT_EQ(2.0f, (column[0, 1]));
    EXPECT_FLOAT_EQ(3.0f, (column[1, 0]));
    EXPECT_FLOAT_EQ(4.0f, (column[1, 1]));

    EXPECT_FLOAT_EQ(1.0f, column.Data()[0]);
    EXPECT_FLOAT_EQ(3.0f, column.Data()[1]);
    EXPECT_FLOAT_EQ(2.0f, column.Data()[2]);
    EXPECT_FLOAT_EQ(4.0f, column.Data()[3]);
}

// ---------------------------------------------------------------------
// Layout-aware operations
// ---------------------------------------------------------------------

TEST(MatTest, RowMajorMultiplication) {
    constexpr MatRowMajor<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr MatRowMajor<float, 2, 2> b{ 5, 6, 7, 8 };

    constexpr auto result = a * b;

    EXPECT_FLOAT_EQ(19.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(22.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(43.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(50.0f, (result[1, 1]));
}

TEST(MatTest, ColumnMajorMultiplication) {
    constexpr MatColumnMajor<float, 2, 2> a{ 1, 2, 3, 4 };

    constexpr MatColumnMajor<float, 2, 2> b{ 5, 6, 7, 8 };

    constexpr auto result = a * b;

    EXPECT_FLOAT_EQ(19.0f, (result[0, 0]));
    EXPECT_FLOAT_EQ(22.0f, (result[0, 1]));
    EXPECT_FLOAT_EQ(43.0f, (result[1, 0]));
    EXPECT_FLOAT_EQ(50.0f, (result[1, 1]));
}

// ---------------------------------------------------------------------
// Numeric conversion
// ---------------------------------------------------------------------

TEST(MatTest, NumericConversion) {
    constexpr Mat<float, 2, 2> source{ 1, 2, 3, 4 };

    constexpr Mat<double, 2, 2> result{ source };

    EXPECT_NEAR(1.0, (result[0, 0]), 1e-4);
    EXPECT_NEAR(2.0, (result[0, 1]), 1e-4);
    EXPECT_NEAR(3.0, (result[1, 0]), 1e-4);
    EXPECT_NEAR(4.0, (result[1, 1]), 1e-4);
}

TEST(MatTest, NumericAndLayoutConversion) {
    constexpr MatRowMajor<float, 2, 2> source{ 1, 2, 3, 4 };

    constexpr MatColumnMajor<double, 2, 2> result{ source };

    EXPECT_NEAR(1.0, (result[0, 0]), 1e-4);
    EXPECT_NEAR(2.0, (result[0, 1]), 1e-4);
    EXPECT_NEAR(3.0, (result[1, 0]), 1e-4);
    EXPECT_NEAR(4.0, (result[1, 1]), 1e-4);

    EXPECT_NEAR(1.0, result.Data()[0], 1e-4);
    EXPECT_NEAR(3.0, result.Data()[1], 1e-4);
    EXPECT_NEAR(2.0, result.Data()[2], 1e-4);
    EXPECT_NEAR(4.0, result.Data()[3], 1e-4);
}

// ---------------------------------------------------------------------
// Aliases
// ---------------------------------------------------------------------

TEST(MatTest, Aliases) {
    static_assert(std::is_same_v<Mat2f, SquareMatrix<float, 2>>);
    static_assert(std::is_same_v<Mat3f, SquareMatrix<float, 3>>);
    static_assert(std::is_same_v<Mat4f, SquareMatrix<float, 4>>);

    static_assert(std::is_same_v<SquareMatrixRowMajor<float, 4>, Mat<float, 4, 4, RowMajor>>);
    static_assert(std::is_same_v<SquareMatrixColumnMajor<float, 4>, Mat<float, 4, 4, ColumnMajor>>);

    static_assert(std::is_same_v<MatRowMajor<float, 3, 4>, Mat<float, 3, 4, RowMajor>>);
    static_assert(std::is_same_v<MatColumnMajor<float, 3, 4>, Mat<float, 3, 4, ColumnMajor>>);
}

// ---------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------

TEST(MatTest, Formatting) {
    constexpr Mat<float, 2, 2> mat{ 1, 2, 3, 4 };

    EXPECT_EQ(std::string("[[1, 2], [3, 4]]"), std::format("{}", mat));
}

TEST(MatTest, FormattingFloatingPoint) {
    constexpr Mat<float, 2, 2> mat{ 1.0f, 2.5f, 3.25f, 4.0f };

    EXPECT_EQ(std::string("[[1, 2.5], [3.25, 4]]"), std::format("{}", mat));
}
