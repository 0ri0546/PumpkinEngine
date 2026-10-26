#pragma once

template <Numeral T, std::size_t N>
template <typename... Args>
    requires(sizeof...(Args) == N) && (std::convertible_to<Args, T> && ...)
inline constexpr Vec<T, N>::Vec(Args&&... args) : m_data{static_cast<T>(std::forward<Args>(args))...} {}

template <Numeral T, std::size_t N>
inline constexpr T& Vec<T, N>::operator[](size_type index) {
    return m_data[index];
}

template <Numeral T, std::size_t N>
inline constexpr const T& Vec<T, N>::operator[](size_type index) const {
    return m_data[index];
}

template <Numeral T, std::size_t N>
inline constexpr T* Vec<T, N>::Data() {
    return m_data.data();
}

template <Numeral T, std::size_t N>
inline constexpr const T* Vec<T, N>::Data() const {
    return m_data.data();
}

template <Numeral T, std::size_t N>
inline constexpr std::size_t Vec<T, N>::Size() {
    return N;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr bool Vec<T, N>::operator==(const Vec<U, N>& other) const {
    for (size_type i = 0; i < N; ++i) {
        if ((*this)[i] != other[i]) {
            return false;
        }
    }

    return true;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr Vec<T, N>& Vec<T, N>::operator+=(const Vec<U, N>& other) {
    for (size_type i = 0; i < N; ++i) {
        (*this)[i] += other[i];
    }

    return *this;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr Vec<T, N>& Vec<T, N>::operator-=(const Vec<U, N>& other) {
    for (size_type i = 0; i < N; ++i) {
        (*this)[i] -= other[i];
    }

    return *this;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr Vec<T, N>& Vec<T, N>::operator*=(const Vec<U, N>& other) {
    for (size_type i = 0; i < N; ++i) {
        (*this)[i] *= other[i];
    }

    return *this;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr Vec<T, N>& Vec<T, N>::operator/=(const Vec<U, N>& other) {
    for (size_type i = 0; i < N; ++i) {
        (*this)[i] /= other[i];
    }

    return *this;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr Vec<T, N>& Vec<T, N>::operator*=(U scalar) {
    for (auto& value : m_data) {
        value *= scalar;
    }

    return *this;
}

template <Numeral T, std::size_t N>
template <Numeral U>
inline constexpr Vec<T, N>& Vec<T, N>::operator/=(U scalar) {
    for (auto& value : m_data) {
        value /= scalar;
    }

    return *this;
}

template <Numeral T, std::size_t N>
inline constexpr Vec<T, N> Vec<T, N>::operator+() const {
    return *this;
}

template <Numeral T, std::size_t N>
inline constexpr Vec<T, N> Vec<T, N>::operator-() const {
    Vec result;

    for (size_type i = 0; i < N; ++i) {
        result[i] = -(*this)[i];
    }

    return result;
}

template <Numeral T, std::size_t N>
inline constexpr T Vec<T, N>::LengthSquared() const {
    T result{};

    for (size_type i = 0; i < N; ++i) {
        result += (*this)[i] * (*this)[i];
    }

    return result;
}

template <Numeral T, std::size_t N>
inline constexpr T Vec<T, N>::Length() const {
    return static_cast<T>(std::sqrt(static_cast<double>(LengthSquared())));
}

template <Numeral T, std::size_t N>
inline constexpr Vec<T, N> Vec<T, N>::Normalized() const {
    const T length = Length();

    if (length <= T{0}) {
        return *this;
    }

    return *this * (T{1} / length);
}

template <Numeral T, std::size_t N>
inline constexpr void Vec<T, N>::Normalize() {
    *this = Normalized();
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator+(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(a[i]) + static_cast<R>(b[i]);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator-(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(a[i]) - static_cast<R>(b[i]);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator*(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(a[i]) * static_cast<R>(b[i]);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator/(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(a[i]) / static_cast<R>(b[i]);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator*(const Vec<T, N>& v, U scalar) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(v[i]) * static_cast<R>(scalar);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator*(U scalar, const Vec<T, N>& v) -> VecCommon<T, U, N> {
    return v * scalar;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto operator/(const Vec<T, N>& v, U scalar) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(v[i]) / static_cast<R>(scalar);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Dot(const Vec<T, N>& a, const Vec<U, N>& b) -> std::common_type_t<T, U> {
    using R = std::common_type_t<T, U>;

    R result{};

    for (std::size_t i = 0; i < N; ++i) {
        result += static_cast<R>(a[i]) * static_cast<R>(b[i]);
    }

    return result;
}

template <Numeral T, Numeral U>
inline constexpr auto Cross(const Vec<T, 3>& a, const Vec<U, 3>& b) -> VecCommon<T, U, 3> {
    using R = std::common_type_t<T, U>;

    return VecCommon<T, U, 3>{
        static_cast<R>(a[1]) * static_cast<R>(b[2]) - static_cast<R>(a[2]) * static_cast<R>(b[1]),
        static_cast<R>(a[2]) * static_cast<R>(b[0]) - static_cast<R>(a[0]) * static_cast<R>(b[2]),
        static_cast<R>(a[0]) * static_cast<R>(b[1]) - static_cast<R>(a[1]) * static_cast<R>(b[0])};
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Distance(const Vec<T, N>& a, const Vec<U, N>& b) -> std::common_type_t<T, U> {
    return (a - b).Length();
}

template <Numeral T, Numeral U, Numeral V, std::size_t N>
inline constexpr auto Lerp(const Vec<T, N>& a, const Vec<U, N>& b, V t) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        const R av = static_cast<R>(a[i]);
        const R bv = static_cast<R>(b[i]);
        result[i] = av + (bv - av) * static_cast<R>(t);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Scale(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    return a * b;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Min(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        const R av = static_cast<R>(a[i]);
        const R bv = static_cast<R>(b[i]);
        result[i] = av < bv ? av : bv;
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Max(const Vec<T, N>& a, const Vec<U, N>& b) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        const R av = static_cast<R>(a[i]);
        const R bv = static_cast<R>(b[i]);
        result[i] = av > bv ? av : bv;
    }

    return result;
}

template <Numeral T, Numeral U, Numeral V, std::size_t N>
inline constexpr auto MoveTowards(const Vec<T, N>& current, const Vec<U, N>& target, V maxDistanceDelta)
    -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    const VecCommon<T, U, N> delta = target - current;
    const R distance = delta.Length();

    if (distance <= static_cast<R>(maxDistanceDelta) || distance <= R{0}) {
        VecCommon<T, U, N> result;

        for (std::size_t i = 0; i < N; ++i) {
            result[i] = static_cast<R>(target[i]);
        }

        return result;
    }

    VecCommon<T, U, N> result;

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(current[i]) + delta[i] * (static_cast<R>(maxDistanceDelta) / distance);
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Reflect(const Vec<T, N>& v, const Vec<U, N>& normal) -> VecCommon<T, U, N> {
    using R = std::common_type_t<T, U>;

    VecCommon<T, U, N> result;
    const R factor = R{2} * Dot(v, normal);

    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<R>(v[i]) - static_cast<R>(normal[i]) * factor;
    }

    return result;
}

template <Numeral T, Numeral U, std::size_t N>
inline constexpr auto Angle(const Vec<T, N>& a, const Vec<U, N>& b) -> std::common_type_t<T, U> {
    using R = std::common_type_t<T, U>;

    R lengthSquaredA{};
    R lengthSquaredB{};

    for (std::size_t i = 0; i < N; ++i) {
        lengthSquaredA += static_cast<R>(a[i]) * static_cast<R>(a[i]);
        lengthSquaredB += static_cast<R>(b[i]) * static_cast<R>(b[i]);
    }

    const R denom = static_cast<R>(std::sqrt(static_cast<double>(lengthSquaredA))) *
                    static_cast<R>(std::sqrt(static_cast<double>(lengthSquaredB)));

    if (denom <= R{0}) {
        return R{0};
    }

    R cosAngle = Dot(a, b) / denom;
    cosAngle = cosAngle < R{-1} ? R{-1} : (cosAngle > R{1} ? R{1} : cosAngle);

    return static_cast<R>(std::acos(static_cast<double>(cosAngle)));
}

template <Numeral T>
inline constexpr Vec<T, 2> Perpendicular(const Vec<T, 2>& v) {
    return Vec<T, 2>{-v[1], v[0]};
}

namespace std {
    template <Numeral T, std::size_t N>
    struct formatter<Vec<T, N>> {
        formatter<T> underlying;

        constexpr auto parse(format_parse_context& ctx) {
            return underlying.parse(ctx);
        }

        auto format(const Vec<T, N>& obj, format_context& ctx) const {
            auto out = ctx.out();
            out = format_to(out, "(");

            for (size_t i = 0; i < N; ++i) {
                if (i != 0) {
                    out = std::format_to(out, ", ");
                }
                out = underlying.format(obj[i], ctx);
            }

            return format_to(out, ")");
        }
    };
}
