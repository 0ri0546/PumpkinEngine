#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <format>
#include <mdspan>
#include <numbers>

#include "Vec.h"
#include "Quaternion.h"

using RowMajor = std::layout_right;
using ColumnMajor = std::layout_left;

template <Numeral T, std::size_t RowCount, std::size_t ColCount, typename Layout = RowMajor>
class Mat {
public:
    using value_type = T;
    using size_type = std::size_t;
    using layout_type = Layout;

    using extents_type = std::extents<size_type, RowCount, ColCount>;
    using mapping_type = Layout::template mapping<extents_type>;
    using mdspan_type = std::mdspan<T, extents_type, Layout>;
    using const_mdspan_type = std::mdspan<const T, extents_type, Layout>;

private:
    static constexpr extents_type m_extents{};
    static constexpr mapping_type m_mapping{ m_extents };
    std::array<T, RowCount* ColCount> m_data{};

public:
    inline constexpr Mat() = default;

    template <typename... Args>
        requires(sizeof...(Args) == RowCount * ColCount) && (std::convertible_to<Args, T> && ...)
    inline constexpr Mat(Args&&... args);

    template <Numeral U, typename OtherLayout>
    inline constexpr Mat(const Mat<U, RowCount, ColCount, OtherLayout>& other);

    inline constexpr T& operator[](size_type row, size_type col);
    inline constexpr const T& operator[](size_type row, size_type col) const;

    inline constexpr T& operator()(size_type row, size_type col);
    inline constexpr const T& operator()(size_type row, size_type col) const;

    inline constexpr T* Data();
    inline constexpr const T* Data() const;

    inline constexpr auto MDSpan();
    inline constexpr auto MDSpan() const;

    static inline constexpr size_type Rows();
    static inline constexpr size_type Cols();
    static inline constexpr size_type Size();

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    inline constexpr void SetRotation(const Quaternion<U>& rotation);

    template <Numeral U>
    inline constexpr bool operator==(const Mat<U, RowCount, ColCount, Layout>& other) const;

    template <Numeral U, typename OtherLayout>
    inline constexpr bool operator==(const Mat<U, RowCount, ColCount, OtherLayout>& other) const;

    template <Numeral U, typename OtherLayout>
    inline constexpr Mat& operator+=(const Mat<U, RowCount, ColCount, OtherLayout>& other);

    template <Numeral U, typename OtherLayout>
    inline constexpr Mat& operator-=(const Mat<U, RowCount, ColCount, OtherLayout>& other);

    template <Numeral U>
    inline constexpr Mat& operator*=(U scalar);

    template <Numeral U>
    inline constexpr Mat& operator/=(U scalar);

    inline constexpr Mat operator+() const;
    inline constexpr Mat operator-() const;

    inline constexpr Mat<T, ColCount, RowCount, Layout> Transpose() const;

    template <std::size_t N = RowCount>
        requires(RowCount == ColCount && N == RowCount)
    static inline constexpr Mat Identity();

    template <std::size_t N = RowCount>
        requires(RowCount == ColCount && N == RowCount)
    inline constexpr auto Determinant() const;

    template <std::size_t N = RowCount>
        requires(RowCount == ColCount && N == RowCount)
    inline constexpr Mat Inverse() const;

    template <std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount)
    static inline constexpr Mat Zero();

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat RotationX(U angleRadians);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat RotationY(U angleRadians);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat RotationZ(U angleRadians);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat Scale(const Vec3<U>& scale);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat Translate(const Vec3<U>& translation);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat Rotate(const Quaternion<U>& rotation);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat TRS(const Vec3<U>& translation, const Quaternion<U>& rotation, const Vec3<U>& scale);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat Perspective(U fovYRadians, U aspect, U nearPlane, U farPlane);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat Ortho(U left, U right, U bottom, U top, U nearPlane, U farPlane);

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    static inline constexpr Mat LookAt(const Vec3<U>& eye, const Vec3<U>& target, const Vec3<U>& up);

    template <Numeral U, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    inline constexpr Vec3<std::common_type_t<T, U>> MultiplyPoint(const Vec3<U>& point) const;

    template <Numeral U, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    inline constexpr Vec3<std::common_type_t<T, U>> MultiplyVector(const Vec3<U>& vector) const;

    template <std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    inline constexpr Vec3<T> ExtractPosition() const;

    template <std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    inline constexpr Vec3<T> ExtractScale() const;

    template <Numeral U = T, std::size_t R = RowCount, std::size_t C = ColCount>
        requires(R == RowCount && C == ColCount && R == 4 && C == 4)
    inline constexpr Quaternion<U> ExtractRotation() const;
};

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
using MatCommon = Mat<std::common_type_t<T, U>, RowCount, ColCount, Layout>;

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr auto operator+(const Mat<T, RowCount, ColCount, Layout>& a, const Mat<U, RowCount, ColCount, Layout>& b)
-> MatCommon<T, U, RowCount, ColCount, Layout>;

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr auto operator-(const Mat<T, RowCount, ColCount, Layout>& a, const Mat<U, RowCount, ColCount, Layout>& b)
-> MatCommon<T, U, RowCount, ColCount, Layout>;

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr auto operator*(const Mat<T, RowCount, ColCount, Layout>& mat, U scalar)
-> MatCommon<T, U, RowCount, ColCount, Layout>;

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr auto operator*(U scalar, const Mat<T, RowCount, ColCount, Layout>& mat)
-> MatCommon<T, U, RowCount, ColCount, Layout>;

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr auto operator/(const Mat<T, RowCount, ColCount, Layout>& mat, U scalar)
-> MatCommon<T, U, RowCount, ColCount, Layout>;

// (A x B) * (B x C) = (A x C)
template <Numeral T, Numeral U, std::size_t A, std::size_t B, std::size_t C, typename Layout>
inline constexpr auto operator*(const Mat<T, A, B, Layout>& lhs, const Mat<U, B, C, Layout>& rhs)
-> MatCommon<T, U, A, C, Layout>;

template <Numeral T, std::size_t RowCount, std::size_t ColCount, typename FromLayout, typename ToLayout>
inline constexpr Mat<T, RowCount, ColCount, ToLayout> MatCastLayout(const Mat<T, RowCount, ColCount, FromLayout>& mat);

template <Numeral T, Numeral U, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr auto operator*(const Mat<T, RowCount, ColCount, Layout>& mat, const Vec<U, ColCount>& vec)
-> Vec<std::common_type_t<T, U>, RowCount>;

template <Numeral T>
inline constexpr T ToRadians(T degrees);

template <Numeral T, std::size_t RowCount, std::size_t ColCount, typename Layout>
inline constexpr bool ValidTRS(const Mat<T, RowCount, ColCount, Layout>& mat);

template <Numeral T, std::size_t RowCount, std::size_t ColCount>
using MatRowMajor = Mat<T, RowCount, ColCount, RowMajor>;

template <Numeral T, std::size_t RowCount, std::size_t ColCount>
using MatColumnMajor = Mat<T, RowCount, ColCount, ColumnMajor>;

template <Numeral T, std::size_t N>
using SquareMatrix = Mat<T, N, N>;

template <Numeral T, std::size_t N>
using SquareMatrixRowMajor = Mat<T, N, N, RowMajor>;

template <Numeral T, std::size_t N>
using SquareMatrixColumnMajor = Mat<T, N, N, ColumnMajor>;

template <Numeral T>
using Mat2 = Mat<T, 2, 2>;

template <Numeral T>
using Mat2ColumnMajor = Mat<T, 2, 2, ColumnMajor>;

template <Numeral T>
using Mat3 = Mat<T, 3, 3>;

template <Numeral T>
using Mat3ColumnMajor = Mat<T, 3, 3, ColumnMajor>;

template <Numeral T>
using Mat4 = Mat<T, 4, 4>;

template <Numeral T>
using Mat4ColumnMajor = Mat<T, 4, 4, ColumnMajor>;

using Mat2f = Mat2<float>;
using Mat3f = Mat3<float>;
using Mat4f = Mat4<float>;

using Mat2fColumnMajor = Mat2ColumnMajor<float>;
using Mat3fColumnMajor = Mat3ColumnMajor<float>;
using Mat4fColumnMajor = Mat4ColumnMajor<float>;

using Mat2d = Mat2<double>;
using Mat3d = Mat3<double>;
using Mat4d = Mat4<double>;

using Mat2dColumnMajor = Mat2ColumnMajor<double>;
using Mat3dColumnMajor = Mat3ColumnMajor<double>;
using Mat4dColumnMajor = Mat4ColumnMajor<double>;

template <Numeral T, std::size_t RowCount, std::size_t ColCount, typename Layout>
struct std::formatter<Mat<T, RowCount, ColCount, Layout>>;

#include "Mat.inl"
