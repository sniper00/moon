#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#undef min
#undef max

inline bool nearly_equal(float a, float b) {
    if (a == b)
        return true;
    auto diff = std::abs(a - b);
    auto norm = std::min((std::abs(a) + std::abs(b)), std::numeric_limits<float>::max());
    return diff
        < std::max(std::numeric_limits<float>::min(), std::numeric_limits<float>::epsilon() * norm);
}

constexpr float PI = 3.1415926f;
constexpr float TOW_PI = 6.2831853f;

template<typename ValueType>
class rect {
public:
    using value_type = ValueType;

    value_type x = value_type {};
    value_type y = value_type {};
    value_type width = {};
    value_type height = {};

    rect() = default;

    constexpr rect(value_type x_, value_type y_, value_type width_, value_type height_):
        x(x_),
        y(y_),
        width(width_),
        height(height_) {}

    rect(const rect& other) {
        set(other.x, other.y, other.width, other.height);
    }

    void set(value_type x_, value_type y_, value_type width_, value_type height_) {
        x = x_;
        y = y_;
        width = width_;
        height = height_;
    }

    value_type left() const {
        return x;
    }

    value_type bottom() const {
        return y;
    }

    value_type top() const {
        return y + height;
    }

    value_type right() const {
        return x + width;
    }

    friend bool operator==(const rect& l, const rect& r) {
        return l.x == r.x && l.y == r.y && l.width == r.width && l.height == r.height;
    }

    bool empty() const {
        return (
            width <= std::numeric_limits<value_type>::epsilon()
            || height <= std::numeric_limits<value_type>::epsilon()
        );
    }

    bool contains(value_type pointx, value_type pointy) const {
        return (pointx >= x && pointx <= right() && pointy >= y && pointy <= top());
    }

    bool contains(const rect& rc) const {
        return (x <= rc.x && y <= rc.y && rc.right() <= right() && rc.top() <= top());
    }

    bool intersects(const rect& rc) const {
        return !(right() < rc.x || rc.right() < x || top() < rc.y || rc.top() < y);
    }
};

struct vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    vector3() = default;

    constexpr vector3(float _x, float _y, float _z): x(_x), y(_y), z(_z) {}

    vector3(const vector3& other): x(other.x), y(other.y), z(other.z) {}

    void sub(const vector3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
    }

    void add(const vector3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
    }

    vector3& operator=(const vector3& other) {
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }

    vector3 operator+(const vector3& other) const {
        vector3 res(*this);
        res.add(other);
        return res;
    }

    vector3 operator-(const vector3& other) const {
        vector3 res(*this);
        res.sub(other);
        return res;
    }

    const vector3 operator*(float s) const {
        vector3 result(*this);
        result.scale(s);
        return result;
    }

    void normalize() {
        float n = (x * x + y * y + z * z);
        if (n == 1.0f) {
            return;
        }

        n = sqrtf(n);
        if (n < std::numeric_limits<float>::min())
            return;

        n = 1.0f / n;
        x *= n;
        y *= n;
        z *= n;
    }

    void scale(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
    }

    float distance2d(const vector3& other) const {
        const float dx = x - other.x;
        const float dz = z - other.z;
        return sqrtf(dx * dx + dz * dz);
    }
};

struct vector2 {
    float x = 0.0f;
    float y = 0.0f;

    vector2() = default;

    constexpr vector2(float _x, float _y): x(_x), y(_y) {}

    vector2(const vector2& other): x(other.x), y(other.y) {}

    void sub(const vector2& other) {
        x -= other.x;
        y -= other.y;
    }

    void add(const vector2& other) {
        x += other.x;
        y += other.y;
    }

    vector2& operator=(const vector2& other) {
        x = other.x;
        y = other.y;
        return *this;
    }

    vector2 operator+(const vector2& other) const {
        vector2 res(*this);
        res.add(other);
        return res;
    }

    vector2 operator-(const vector2& other) const {
        vector2 res(*this);
        res.sub(other);
        return res;
    }

    const vector2 operator*(float s) const {
        vector2 result(*this);
        result.scale(s);
        return result;
    }

    void normalize() {
        float n = (x * x + y * y);
        if (n == 1.0f) {
            return;
        }

        n = sqrtf(n);
        if (n < std::numeric_limits<float>::min())
            return;

        n = 1.0f / n;
        x *= n;
        y *= n;
    }

    void scale(float scalar) {
        x *= scalar;
        y *= scalar;
    }

    float distance(const vector2& other) const {
        const float dx = x - other.x;
        const float dy = y - other.y;
        return sqrtf(dx * dx + dy * dy);
    }

    bool zero() const {
        return x == 0.0f && y == 0.0f;
    }

    /**
     * Rotates this vector by angle (specified in radians) around the given point.
     *
     * @param point The point to rotate around.
     * @param angle The angle to rotate by (in radians).
     */
    void rotate(const vector2& point, float angle) {
        float sinAngle = sin(angle);
        float cosAngle = cos(angle);

        if (point.zero()) {
            float tempX = x * cosAngle - y * sinAngle;
            y = y * cosAngle + x * sinAngle;
            x = tempX;
        } else {
            float tempX = x - point.x;
            float tempY = y - point.y;

            x = tempX * cosAngle - tempY * sinAngle + point.x;
            y = tempY * cosAngle + tempX * sinAngle + point.y;
        }
    }

    static float dot(const vector2& v1, const vector2& v2) {
        return (v1.x * v2.x + v1.y * v2.y);
    }

    /**
     * Returns the angle (in radians) between the specified vectors.
     *
     * @param v1 The first vector.
     * @param v2 The second vector.
     *
     * @return The angle between the two vectors (in radians).
     */
    static float angle(const vector2& v1, const vector2& v2) {
        float dz = v1.x * v2.y - v1.y * v2.x;
        float v = atan2f(fabsf(dz) + std::numeric_limits<float>::epsilon(), dot(v1, v2));
        return v;
    }

    static float angle2(vector2 v1, vector2 v2) {
        float dz = v1.x * v2.y - v1.y * v2.x;
        float v = atan2f((dz) + std::numeric_limits<float>::epsilon(), dot(v1, v2));
        if (v < 0.0f) {
            v += TOW_PI;
        }
        return v;
    }

    /**
 * \relates    Vector2
 * \brief      Computes the determinant of a two-dimensional square matrix with
 *             rows consisting of the specified two-dimensional vectors.
 * \param      vector1         The top row of the two-dimensional square
 *                             matrix.
 * \param      vector2         The bottom row of the two-dimensional square
 *                             matrix.
 * \return     The determinant of the two-dimensional square matrix.
 */
    static float det(const vector2& v1, const vector2& v2) {
        return v1.x * v2.y - v1.y * v2.x;
    }
};

inline float radian_to_angle(float v) {
    static constexpr float f = 57.295779f;
    return v * f;
}

inline float angle_to_radian(float v) {
    static constexpr float f = 0.017453f;
    return v * f;
}
