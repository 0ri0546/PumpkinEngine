#pragma once

template <Numeral T>
inline constexpr Quaternion<T>::Quaternion(T x, T y, T z, T w) : m_x(x),
                                                            m_y(y),
                                                            m_z(z),
                                                            m_w(w) {}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T>::Quaternion(const Vec3<U>& axis, U angleRadians) {
    *this = FromAxisAngle(axis, angleRadians);
}

template <Numeral T>
inline constexpr Quaternion<T> Quaternion<T>::Identity() {
    return Quaternion(T{0}, T{0}, T{0}, T{1});
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T> Quaternion<T>::FromEuler(U pitch, U yaw, U roll) {
    const U halfPitch = pitch * U{0.5};
    const U halfYaw = yaw * U{0.5};
    const U halfRoll = roll * U{0.5};

    const U sp = std::sin(halfPitch);
    const U cp = std::cos(halfPitch);
    const U sy = std::sin(halfYaw);
    const U cy = std::cos(halfYaw);
    const U sr = std::sin(halfRoll);
    const U cr = std::cos(halfRoll);

    return Quaternion(static_cast<T>(sp * cy * cr - cp * sy * sr), static_cast<T>(cp * sy * cr + sp * cy * sr),
                        static_cast<T>(cp * cy * sr - sp * sy * cr), static_cast<T>(cp * cy * cr + sp * sy * sr));
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T> Quaternion<T>::FromAxisAngle(const Vec3<U>& axis, U angleRadians) {
    const U halfAngle = angleRadians * U{0.5};
    const U s = std::sin(halfAngle);
    const U c = std::cos(halfAngle);

    Vec3<U> normalizedAxis = axis;
    const U lengthSquared = normalizedAxis[0] * normalizedAxis[0] + normalizedAxis[1] * normalizedAxis[1] +
                            normalizedAxis[2] * normalizedAxis[2];

    if (lengthSquared > U{0}) {
        const U invLength = U{1} / std::sqrt(lengthSquared);
        normalizedAxis *= invLength;
    }

    return Quaternion(static_cast<T>(normalizedAxis[0] * s), static_cast<T>(normalizedAxis[1] * s),
                        static_cast<T>(normalizedAxis[2] * s), static_cast<T>(c));
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T> Quaternion<T>::FromToRotation(const Vec3<U>& from, const Vec3<U>& to) {
    const Vec3<U> nFrom = from.Normalized();
    const Vec3<U> nTo = to.Normalized();

    const U cosAngle = Dot(nFrom, nTo);

    if (cosAngle > U{1} - static_cast<U>(1e-6)) {
        return Identity();
    }

    if (cosAngle < U{-1} + static_cast<U>(1e-6)) {
        Vec3<U> axis = Cross(Vec3<U>{U{1}, U{0}, U{0}}, nFrom);

        if (axis.LengthSquared() < static_cast<U>(1e-12)) {
            axis = Cross(Vec3<U>{U{0}, U{1}, U{0}}, nFrom);
        }

        return FromAxisAngle(axis.Normalized(), std::numbers::pi_v<U>);
    }

    const Vec3<U> axis = Cross(nFrom, nTo);
    const U s = std::sqrt((U{1} + cosAngle) * U{2});
    const U invS = U{1} / s;

    return Quaternion(static_cast<T>(axis[0] * invS), static_cast<T>(axis[1] * invS),
                        static_cast<T>(axis[2] * invS), static_cast<T>(s * U{0.5}))
        .Normalized();
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T> Quaternion<T>::LookRotation(const Vec3<U>& forward, const Vec3<U>& up) {
    const Vec3<U> f = forward.Normalized();
    const Vec3<U> right = Cross(up, f).Normalized();
    const Vec3<U> trueUp = Cross(f, right);

    const U m00 = right[0], m01 = trueUp[0], m02 = f[0];
    const U m10 = right[1], m11 = trueUp[1], m12 = f[1];
    const U m20 = right[2], m21 = trueUp[2], m22 = f[2];

    const U trace = m00 + m11 + m22;

    if (trace > U{0}) {
        const U s = std::sqrt(trace + U{1}) * U{2};
        return Quaternion(static_cast<T>((m21 - m12) / s), static_cast<T>((m02 - m20) / s),
                            static_cast<T>((m10 - m01) / s), static_cast<T>(s * U{0.25}));
    }

    if (m00 > m11 && m00 > m22) {
        const U s = std::sqrt(U{1} + m00 - m11 - m22) * U{2};
        return Quaternion(static_cast<T>(s * U{0.25}), static_cast<T>((m01 + m10) / s),
                            static_cast<T>((m02 + m20) / s), static_cast<T>((m21 - m12) / s));
    }

    if (m11 > m22) {
        const U s = std::sqrt(U{1} + m11 - m00 - m22) * U{2};
        return Quaternion(static_cast<T>((m01 + m10) / s), static_cast<T>(s * U{0.25}),
                            static_cast<T>((m12 + m21) / s), static_cast<T>((m02 - m20) / s));
    }

    const U s = std::sqrt(U{1} + m22 - m00 - m11) * U{2};
    return Quaternion(static_cast<T>((m02 + m20) / s), static_cast<T>((m12 + m21) / s), static_cast<T>(s * U{0.25}),
                        static_cast<T>((m10 - m01) / s));
}

template <Numeral T>
inline constexpr T& Quaternion<T>::X() {
    return m_x;
}

template <Numeral T>
inline constexpr const T& Quaternion<T>::X() const {
    return m_x;
}

template <Numeral T>
inline constexpr T& Quaternion<T>::Y() {
    return m_y;
}

template <Numeral T>
inline constexpr const T& Quaternion<T>::Y() const {
    return m_y;
}

template <Numeral T>
inline constexpr T& Quaternion<T>::Z() {
    return m_z;
}

template <Numeral T>
inline constexpr const T& Quaternion<T>::Z() const {
    return m_z;
}

template <Numeral T>
inline constexpr T& Quaternion<T>::W() {
    return m_w;
}

template <Numeral T>
inline constexpr const T& Quaternion<T>::W() const {
    return m_w;
}

template <Numeral T>
inline constexpr T* Quaternion<T>::Data() {
    return &m_x;
}

template <Numeral T>
inline constexpr const T* Quaternion<T>::Data() const {
    return &m_x;
}

template <Numeral T>
inline constexpr std::size_t Quaternion<T>::Size() {
    return 4;
}

template <Numeral T>
template <Numeral U>
inline constexpr bool Quaternion<T>::operator==(const Quaternion<U>& other) const {
    return m_x == other[0] && m_y == other[1] && m_z == other[2] && m_w == other[3];
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T>& Quaternion<T>::operator+=(const Quaternion<U>& other) {
    m_x += static_cast<T>(other[0]);
    m_y += static_cast<T>(other[1]);
    m_z += static_cast<T>(other[2]);
    m_w += static_cast<T>(other[3]);

    return *this;
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T>& Quaternion<T>::operator-=(const Quaternion<U>& other) {
    m_x -= static_cast<T>(other[0]);
    m_y -= static_cast<T>(other[1]);
    m_z -= static_cast<T>(other[2]);
    m_w -= static_cast<T>(other[3]);

    return *this;
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T>& Quaternion<T>::operator*=(const Quaternion<U>& other) {
    const T x = m_w * static_cast<T>(other[0]) + m_x * static_cast<T>(other[3]) +
                m_y * static_cast<T>(other[2]) - m_z * static_cast<T>(other[1]);
    const T y = m_w * static_cast<T>(other[1]) - m_x * static_cast<T>(other[2]) +
                m_y * static_cast<T>(other[3]) + m_z * static_cast<T>(other[0]);
    const T z = m_w * static_cast<T>(other[2]) + m_x * static_cast<T>(other[1]) -
                m_y * static_cast<T>(other[0]) + m_z * static_cast<T>(other[3]);
    const T w = m_w * static_cast<T>(other[3]) - m_x * static_cast<T>(other[0]) -
                m_y * static_cast<T>(other[1]) - m_z * static_cast<T>(other[2]);

    m_x = x;
    m_y = y;
    m_z = z;
    m_w = w;

    return *this;
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T>& Quaternion<T>::operator*=(U scalar) {
    m_x *= static_cast<T>(scalar);
    m_y *= static_cast<T>(scalar);
    m_z *= static_cast<T>(scalar);
    m_w *= static_cast<T>(scalar);

    return *this;
}

template <Numeral T>
template <Numeral U>
inline constexpr Quaternion<T>& Quaternion<T>::operator/=(U scalar) {
    m_x /= static_cast<T>(scalar);
    m_y /= static_cast<T>(scalar);
    m_z /= static_cast<T>(scalar);
    m_w /= static_cast<T>(scalar);

    return *this;
}

template <Numeral T>
inline constexpr Quaternion<T> Quaternion<T>::operator+() const {
    return *this;
}

template <Numeral T>
inline constexpr Quaternion<T> Quaternion<T>::operator-() const {
    return Quaternion(-m_x, -m_y, -m_z, -m_w);
}

template <Numeral T>
inline constexpr T Quaternion<T>::operator[](const size_type index) const {
    switch (index) {
    case 0: return m_x;
    case 1: return m_y;
    case 2: return m_z;
    case 3: return m_w;
    default: return T{ 0 };
    }
}

template <Numeral T>
inline constexpr T Quaternion<T>::LengthSquared() const {
    return m_x * m_x + m_y * m_y + m_z * m_z + m_w * m_w;
}

template <Numeral T>
inline constexpr T Quaternion<T>::Length() const {
    return std::sqrt(LengthSquared());
}

template <Numeral T>
inline constexpr Quaternion<T> Quaternion<T>::Normalized() const {
    const T length = Length();

    if (length <= T{0}) {
        return *this;
    }

    const T invLength = T{1} / length;
    return Quaternion(m_x * invLength, m_y * invLength, m_z * invLength, m_w * invLength);
}

template <Numeral T>
inline constexpr void Quaternion<T>::Normalize() {
    *this = Normalized();
}

template <Numeral T>
inline constexpr Quaternion<T> Quaternion<T>::Conjugate() const {
    return Quaternion(-m_x, -m_y, -m_z, m_w);
}

template <Numeral T>
inline constexpr Quaternion<T> Quaternion<T>::Inverse() const {
    const T lengthSquared = LengthSquared();

    if (lengthSquared <= T{0}) {
        return Conjugate();
    }

    return Conjugate() / lengthSquared;
}

template <Numeral T>
inline constexpr Vec3<T> Quaternion<T>::RotateVector(const Vec3<T>& v) const {
    const Vec3<T> q{m_x, m_y, m_z};

    const Vec3<T> cross1{q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0]};

    const Vec3<T> cross2{q[1] * cross1[2] - q[2] * cross1[1], q[2] * cross1[0] - q[0] * cross1[2],
                            q[0] * cross1[1] - q[1] * cross1[0]};

    return v + (cross1 * (T{2} * m_w)) + (cross2 * T{2});
}

template <Numeral T>
inline constexpr Vec3<T> Quaternion<T>::ToEuler() const {
    return Vec3<T>{Pitch(), Yaw(), Roll()};
}

template <Numeral T>
inline constexpr T Quaternion<T>::Pitch() const {
    const T sinPitch = T{2} * (m_w * m_x + m_y * m_z);
    const T cosPitch = T{1} - T{2} * (m_x * m_x + m_y * m_y);

    return std::atan2(sinPitch, cosPitch);
}

template <Numeral T>
inline constexpr T Quaternion<T>::Yaw() const {
    const T sinYaw = T{2} * (m_w * m_y - m_z * m_x);

    if (std::abs(sinYaw) >= T{1}) {
        return std::copysign(std::numbers::pi_v<T> / T{2}, sinYaw);
    }

    return std::asin(sinYaw);
}

template <Numeral T>
inline constexpr T Quaternion<T>::Roll() const {
    const T sinRoll = T{2} * (m_w * m_z + m_x * m_y);
    const T cosRoll = T{1} - T{2} * (m_y * m_y + m_z * m_z);

    return std::atan2(sinRoll, cosRoll);
}

template <Numeral T>
inline constexpr void Quaternion<T>::ToAxisAngle(Vec3<T>& outAxis, T& outAngleRadians) const {
    const Quaternion normalized = Normalized();
    const T clampedW = normalized.m_w < T{-1} ? T{-1} : (normalized.m_w > T{1} ? T{1} : normalized.m_w);

    outAngleRadians = T{2} * std::acos(clampedW);

    const T s = std::sqrt(T{1} - clampedW * clampedW);

    if (s < static_cast<T>(1e-6)) {
        outAxis = Vec3<T>{T{1}, T{0}, T{0}};
    } else {
        outAxis = Vec3<T>{normalized.m_x / s, normalized.m_y / s, normalized.m_z / s};
    }
}

template <Numeral T, Numeral U>
inline constexpr auto operator+(const Quaternion<T>& a, const Quaternion<U>& b) -> QuaternionCommon<T, U> {
    using R = std::common_type_t<T, U>;

    return QuaternionCommon<T, U>(
        static_cast<R>(a[0]) + static_cast<R>(b[0]), static_cast<R>(a[1]) + static_cast<R>(b[1]),
        static_cast<R>(a[2]) + static_cast<R>(b[2]), static_cast<R>(a[3]) + static_cast<R>(b[3]));
}

template <Numeral T, Numeral U>
inline constexpr auto operator-(const Quaternion<T>& a, const Quaternion<U>& b) -> QuaternionCommon<T, U> {
    using R = std::common_type_t<T, U>;

    return QuaternionCommon<T, U>(
        static_cast<R>(a[0]) - static_cast<R>(b[0]), static_cast<R>(a[1]) - static_cast<R>(b[1]),
        static_cast<R>(a[2]) - static_cast<R>(b[2]), static_cast<R>(a[3]) - static_cast<R>(b[3]));
}

template <Numeral T, Numeral U>
inline constexpr auto operator*(const Quaternion<T>& a, const Quaternion<U>& b) -> QuaternionCommon<T, U> {
    using R = std::common_type_t<T, U>;

    QuaternionCommon<T, U> result(static_cast<R>(a[0]), static_cast<R>(a[1]), static_cast<R>(a[2]),
                                    static_cast<R>(a[3]));

    result *= QuaternionCommon<T, U>(static_cast<R>(b[0]), static_cast<R>(b[1]), static_cast<R>(b[2]),
                                        static_cast<R>(b[3]));

    return result;
}

template <Numeral T, Numeral U>
inline constexpr auto operator*(const Quaternion<T>& q, U scalar) -> QuaternionCommon<T, U> {
    using R = std::common_type_t<T, U>;

    return QuaternionCommon<T, U>(
        static_cast<R>(q[0]) * static_cast<R>(scalar), static_cast<R>(q[1]) * static_cast<R>(scalar),
        static_cast<R>(q[2]) * static_cast<R>(scalar), static_cast<R>(q[3]) * static_cast<R>(scalar));
}

template <Numeral T, Numeral U>
inline constexpr auto operator*(U scalar, const Quaternion<T>& q) -> QuaternionCommon<T, U> {
    return q * scalar;
}

template <Numeral T, Numeral U>
inline constexpr auto operator/(const Quaternion<T>& q, U scalar) -> QuaternionCommon<T, U> {
    using R = std::common_type_t<T, U>;

    return QuaternionCommon<T, U>(
        static_cast<R>(q[0]) / static_cast<R>(scalar), static_cast<R>(q[1]) / static_cast<R>(scalar),
        static_cast<R>(q[2]) / static_cast<R>(scalar), static_cast<R>(q[3]) / static_cast<R>(scalar));
}

template <Numeral T, Numeral U>
inline constexpr auto operator*(const Quaternion<T>& q, const Vec3<U>& v) -> Vec3<std::common_type_t<T, U>> {
    using R = std::common_type_t<T, U>;

    const Quaternion<R> qr(static_cast<R>(q[0]), static_cast<R>(q[1]), static_cast<R>(q[2]),
                            static_cast<R>(q[3]));
    const Vec3<R> vr{static_cast<R>(v[0]), static_cast<R>(v[1]), static_cast<R>(v[2])};

    return qr.RotateVector(vr);
}

template <Numeral T>
inline constexpr T Dot(const Quaternion<T>& a, const Quaternion<T>& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

template <Numeral T>
inline constexpr Quaternion<T> LerpUnclamped(const Quaternion<T>& a, const Quaternion<T>& b, T t) {
    return (a * (T{1} - t) + b * t).Normalized();
}

template <Numeral T>
inline constexpr Quaternion<T> Lerp(const Quaternion<T>& a, const Quaternion<T>& b, T t) {
    const T clampedT = t < T{0} ? T{0} : (t > T{1} ? T{1} : t);
    return LerpUnclamped(a, b, clampedT);
}

template <Numeral T>
inline constexpr Quaternion<T> SlerpUnclamped(const Quaternion<T>& a, const Quaternion<T>& b, T t) {
    Quaternion<T> end = b;
    T cosOmega = Dot(a, b);

    if (cosOmega < T{0}) {
        cosOmega = -cosOmega;
        end = -end;
    }

    constexpr T kEpsilon = static_cast<T>(1e-6);

    if (cosOmega > T{1} - kEpsilon) {
        return LerpUnclamped(a, end, t);
    }

    const T omega = std::acos(cosOmega);
    const T sinOmega = std::sin(omega);

    const T scaleA = std::sin((T{1} - t) * omega) / sinOmega;
    const T scaleB = std::sin(t * omega) / sinOmega;

    return (a * scaleA) + (end * scaleB);
}

template <Numeral T>
inline constexpr Quaternion<T> Slerp(const Quaternion<T>& a, const Quaternion<T>& b, T t) {
    const T clampedT = t < T{0} ? T{0} : (t > T{1} ? T{1} : t);
    return SlerpUnclamped(a, b, clampedT);
}

template <Numeral T>
inline constexpr T Angle(const Quaternion<T>& a, const Quaternion<T>& b) {
    const T cosHalfAngle = Dot(a, b);
    const T clamped = cosHalfAngle < T{-1} ? T{-1} : (cosHalfAngle > T{1} ? T{1} : cosHalfAngle);

    return T{2} * std::acos(std::abs(clamped));
}

template <Numeral T>
inline constexpr Quaternion<T> RotateTowards(const Quaternion<T>& from, const Quaternion<T>& to, T maxAngleRadians) {
    const T angle = Angle(from, to);

    if (angle <= T{0}) {
        return to;
    }

    const T t = maxAngleRadians / angle;

    if (t >= T{1}) {
        return to;
    }

    return SlerpUnclamped(from, to, t);
}

namespace std {
    template <Numeral T>
    struct formatter<Quaternion<T>> {
        formatter<T> underlying;

        constexpr auto parse(format_parse_context& ctx) {
            return underlying.parse(ctx);
        }

        auto format(const Quaternion<T>& obj, format_context& ctx) const {
            auto out = ctx.out();
            out = format_to(out, "(");
            out = underlying.format(obj[0], ctx);
            out = format_to(out, ", ");
            out = underlying.format(obj[1], ctx);
            out = format_to(out, ", ");
            out = underlying.format(obj[2], ctx);
            out = format_to(out, ", ");
            out = underlying.format(obj[3], ctx);
            return format_to(out, ")");
        }
    };
}
