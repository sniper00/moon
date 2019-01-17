#pragma once
#undef min
#undef max
#include <cmath>
#include <limits>

namespace math
{
	class vec2
	{
	public:
		float x = 0.0f;
		float y = 0.0f;

		// Constructs a new vector initialized to all zeros.
		vec2() = default;

		// Constructs a new vector initialized to the specified values.
		constexpr vec2(float _x, float _y)
			:x(_x),y(_y)
		{}

		vec2(const vec2& other)
			:x(other.x),y(other.y)
		{
		}

		void add(const vec2& v)
		{
			x += v.x;
			y += v.y;
		}

		void subtract(const vec2& v)
		{
			x -= v.x;
			y -= v.y;
		}

		// Clamps this vector within the specified range.
		void clamp(const vec2& min, const vec2& max)
		{
			assert(!(min.x > max.x || min.y > max.y));

			// Clamp the x value.
			if (x < min.x)
				x = min.x;
			if (x > max.x)
				x = max.x;

			// Clamp the y value.
			if (y < min.y)
				y = min.y;
			if (y > max.y)
				y = max.y;
		}

		// Negates this vector.
		void negate()
		{
			x = -x;
			y = -y;
		}

		// Makes this vector have a magnitude of 1.
		// If the vector already has unit length or if the length of the vector is zero, this method does nothing.
		void normalize()
		{
			float n = x * x + y * y;
			// Already normalized.
			if (n == 1.0f)
				return;

			n = sqrt(n);
			// Too close to zero.
			if (n < std::numeric_limits<float>::min())
				return;

			n = 1.0f / n;
			x *= n;
			y *= n;
		}

		// Indicates whether this vector contains all zeros.
		bool iszero() const
		{
			return x == 0.0f && y == 0.0f;
		}

		// Indicates whether this vector contains all ones.
		bool isone() const
		{
			return x == 1.0f && y == 1.0f;
		}

		// Scales all elements of this vector by the specified value.
		void scale(float scalar)
		{
			x *= scalar;
			y *= scalar;
		}

		// Scales each element of this vector by the matching component of scale.
		void scale(const vec2& scale)
		{
			x *= scale.x;
			y *= scale.y;
		}

		// Rotates this vector by angle (specified in radians) around the given point.
		void rotate(const vec2& point, float angle)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			if (point.iszero())
			{
				float tempX = x * cosAngle - y * sinAngle;
				y = y * cosAngle + x * sinAngle;
				x = tempX;
			}
			else
			{
				float tempX = x - point.x;
				float tempY = y - point.y;

				x = tempX * cosAngle - tempY * sinAngle + point.x;
				y = tempY * cosAngle + tempX * sinAngle + point.y;
			}
		}


		// Dot Product of two vectors.
		float dot(const vec2& v) const
		{
			return (x * v.x + y * v.y);
		}

		float cross(const vec2& other) const {
			return x*other.y - y*other.x;
		};

		// Calculates the projection of this over other.
		vec2 project(const vec2& other) const {
			return other * (dot(other) / other.dot(other));
		};

		float distance(const vec2& other) const
		{
			return (*this - other).length();
		}

		float distance_sqrt(const vec2& v) const
		{
			float dx = v.x - x;
			float dy = v.y - y;
			return (dx * dx + dy * dy);
		}

		// Returns the length of this vector
		float length() const
		{
			return std::sqrtf(x*x + y*y);
		}

		// Returns the squared length of this vector 
		float length_sqrt() const
		{
			return (x * x + y * y);
		}
	
		// Get the normalized vector.
		vec2 normalized() const
		{
			vec2 v(*this);
			v.normalize();
			return v;
		}

		// x angle
		float angle() const
		{
			return std::atan2f(y, x);
		}

		// Returns the unsigned angle in degrees between from and to.
		float angle(const vec2& other) const
		{
			vec2 a2 = normalized();
			vec2 b2 = other.normalized();
			float angle = atan2f(a2.cross(b2), a2.dot(b2));
			if (fabs(angle) < FLT_EPSILON) return 0.f;
			return angle;
		}

		// Calculates midpoint between two points.
		vec2 mid_point(const vec2& other) const
		{
			return vec2((x + other.x) / 2.0f, (y + other.y) / 2.0f);
		}

		// Linear Interpolation between two points a and b
		// alpha == 0 ? a
		// alpha == 1 ? b
		//	otherwise a value between a..b
		inline vec2 lerp(const vec2& other, float alpha) const {
			return *this * (1.f - alpha) + other * alpha;
		};

		void smooth(const vec2& target, float elapsedTime, float responseTime)
		{
			if (elapsedTime > 0)
			{
				*this += (target - *this) * (elapsedTime / (elapsedTime + responseTime));
			}
		}

		// Returns true if the given vector is exactly equal to this vector.
		bool equals(const vec2& target) const
		{
			return (fabs(this->x - target.x) < FLT_EPSILON)
				&& (fabs(this->y - target.y) < FLT_EPSILON);
		}



	

		// Returns the dot product of this vector and the specified vector.
		static float dot(const vec2& v1, const vec2& v2)
		{
			return (v1.x * v2.x + v1.y * v2.y);
		}

		static float angle(const vec2& v1, const vec2& v2)
		{
			float dz = v1.x * v2.y - v1.y * v2.x;
			return atan2f(fabsf(dz) + std::numeric_limits<float>::min(), dot(v1, v2));
		}
		
		inline vec2 rotate(const vec2& other) const {
			return vec2(x*other.x - y*other.y, x*other.y + y*other.x);
		};

		// Rotates a point counter clockwise by the angle around a pivot
		vec2 rotate_by_angle(const vec2& pivot, float angle) const
		{
			return pivot + (*this - pivot).rotate(vec2::forangle(angle));
		}

		static vec2 forangle(const float a)
		{
			return vec2(cosf(a), sinf(a));
		}

		void set(float xx, float yy)
		{
			x = xx;
			y = yy;
		}

		void zero()
		{
			x = y = 0.0f;
		}

		const vec2 operator+(const vec2& v) const
		{
			vec2 result(*this);
			result.add(v);
			return result;
		}

		vec2& operator+=(const vec2& v)
		{
			add(v);
			return *this;
		}

		const vec2 operator-(const vec2& v) const
		{
			vec2 result(*this);
			result.subtract(v);
			return result;
		}

		vec2& operator-=(const vec2& v)
		{
			subtract(v);
			return *this;
		}

		const vec2 operator-() const
		{
			vec2 result(*this);
			result.negate();
			return result;
		}

		const vec2 operator*(float s) const
		{
			vec2 result(*this);
			result.scale(s);
			return result;
		}

		vec2& operator*=(float s)
		{
			scale(s);
			return *this;
		}

		const vec2 operator/(const float s) const
		{
			return vec2(x / s, y / s);
		}

		bool operator<(const vec2& v) const
		{
			if (x == v.x)
			{
				return y < v.y;
			}
			return x < v.x;
		}

		bool operator>(const vec2& v) const
		{
			if (x == v.x)
			{
				return y > v.y;
			}
			return x > v.x;
		}

		bool operator==(const vec2& v) const
		{
			return x == v.x && y == v.y;
		}

		bool operator!=(const vec2& v) const
		{
			return x != v.x || y != v.y;
		}
	};

	inline static const vec2 operator*(float x, const vec2& v)
	{
		vec2 result(v);
		result.scale(x);
		return result;
	}

	constexpr vec2 VEC2_ZERO(0.0f, 0.0f);
	constexpr vec2 VEC2_ONE(1.0f, 1.0f);
	constexpr vec2 VEC2_UNIT_X(1.0f, 0.f);
	constexpr vec2 VEC2_UNIT_Y(0.0f, 1.0f);
	constexpr vec2 VEC2_NEGATIVEINFINITY(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
	constexpr vec2 VEC2_POSITIVEINFINITY(std::numeric_limits<float>::max(),std::numeric_limits<float>::max());

	// cross product of 2 vector. A->B X C->D
	inline float crossProduct2Vector(const vec2& A, const vec2& B, const vec2& C, const vec2& D)
	{
		return (D.y - C.y) * (B.x - A.x) - (D.x - C.x) * (B.y - A.y);
	}

	inline bool isLineIntersect(const vec2& A, const vec2& B,
		const vec2& C, const vec2& D,
		float *S, float *T)
	{
		// FAIL: Line undefined
		if ((A.x == B.x && A.y == B.y) || (C.x == D.x && C.y == D.y))
		{
			return false;
		}

		const float denom = crossProduct2Vector(A, B, C, D);

		if (denom == 0)
		{
			// Lines parallel or overlap
			return false;
		}

		if (S != nullptr) *S = crossProduct2Vector(C, D, C, A) / denom;
		if (T != nullptr) *T = crossProduct2Vector(A, B, C, A) / denom;

		return true;
	}
}