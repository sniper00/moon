#pragma once
#include <cassert>
#include <algorithm>
#include "vec2.hpp"

namespace math
{
	class size
	{
	public:
		/**Width of the Size.*/
		float width = 0.0f;
		/**Height of the Size.*/
		float height = 0.0f;
	public:
		operator vec2() const
		{
			return vec2(width, height);
		}

		size() = default;
		size(const size& other) = default;

		constexpr size(float _width, float _height)
			:width(_width), height(_height)
		{
		}

		explicit size(const vec2& v)
			:width(v.x), height(v.y)
		{
		}

		size& operator= (const size& other)
		{
			set(other.width, other.height);
			return *this;
		}

		size& operator= (const vec2& point)
		{
			set(point.x, point.y);
			return *this;
		}

		size operator+(const size& right) const
		{
			return size(width + right.width, height + right.height);
		}

		size operator-(const size& right) const
		{
			return size(width - right.width, height - right.height);
		}

		size operator*(float a) const
		{
			return size(this->width * a, this->height * a);
		}

		size operator/(float a) const
		{
			assert(a != 0 && "size division by 0.");
			return size(width / a, height / a);
		}

		void set(float w, float h)
		{
			width = w;
			height = h;
		}

		bool equals(const size& target) const
		{
			return (fabs(this->width - target.width) < std::numeric_limits<float>::epsilon())
				&& (fabs(this->height - target.height) < std::numeric_limits<float>::epsilon());
		}
	};

	constexpr size SIZE_ZERO(0.f, 0.f);

	class rect
	{
	public:
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;

		rect() = default;

		constexpr rect(float x_, float y_, float width_, float height_)
			:x(x_),y(y_),width(width_), height(height_)
		{
		}

		rect(const vec2& pos, const math::size& dimension)
		{
			set(pos.x, pos.y, dimension.width, dimension.height);
		}

		rect(const rect& other)
		{
			set(other.x, other.y, other.width, other.height);
		}

		void set(float x_, float y_, float width_, float height_)
		{
			x = x_;
			y = y_;
			width = width_;
			height = height_;
		}

		bool equals(const rect& target) const
		{
			return (fabs(x - target.x) < std::numeric_limits<float>::epsilon())
				&& (fabs(y - target.y) < std::numeric_limits<float>::epsilon())
				&& (fabs(width - target.width) < std::numeric_limits<float>::epsilon())
				&& (fabs(height - target.height) < std::numeric_limits<float>::epsilon());
		}

		float left() const
		{
			return x;
		}

		float bottom() const
		{
			return y;
		}

		float top() const
		{
			return y+height;
		}

		float right() const
		{
			return x+width;
		}

		float midx() const
		{
			return x + width / 2.0f;
		}

		float midy() const
		{
			return y + height / 2.0f;
		}

		bool empty() const
		{
			return (width <= 0.0f || height <= 0.0f);
		}

		bool contains(const rect& rect) const
		{
			return (x<= rect.x 
				&& y <= rect.y 
				&& rect.right() <= right()
				&& rect.top() <= top());
		}

		bool contains(const vec2& point) const
		{
			return (point.x >= x && point.x <= right()
				&& point.y >= y && point.y <= top());
		}

        bool contains(float pointx, float pointy) const
        {
            return (pointx >= x && pointx <= right()
                && pointy >= y && pointy <= top());
        }

		bool intersects(const rect& rect) const
		{
			return !(right() < rect.x ||rect.right() < x ||top() < rect.y ||rect.top() < y );
		}

		bool intersects_circle(const vec2& center, float radius) const
		{
			vec2 rectangleCenter((x + width / 2),
				(y + height / 2));

			float w =width / 2;
			float h = height / 2;

			float dx = fabs(center.x - rectangleCenter.x);
			float dy = fabs(center.y - rectangleCenter.y);

			if (dx > (radius + w) || dy > (radius + h))
			{
				return false;
			}

			vec2 circleDistance(fabs(center.x - x - w),
				fabs(center.y -y - h));

			if (circleDistance.x <= (w))
			{
				return true;
			}

			if (circleDistance.y <= (h))
			{
				return true;
			}

			float cornerDistanceSq = powf(circleDistance.x - w, 2) + powf(circleDistance.y - h, 2);

			return (cornerDistanceSq <= (powf(radius, 2)));
		}

		void merge(const rect& rect)
		{
			float minX = std::min(x, rect.x);
			float minY = std::min(y, rect.y);
			float maxX = std::max(right(), rect.right());
			float maxY = std::max(top(), rect.top());
			set(minX, minY, maxX - minX, maxY - minY);
		}
	};

	constexpr rect RECT_ZERO(0.0f, 0.0f, 0.0f, 0.0f);
}
