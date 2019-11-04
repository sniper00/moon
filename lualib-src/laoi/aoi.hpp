#pragma once
#include <unordered_map>
#include <forward_list>
#include <cassert>

template<typename AoiObject>
class aoi
{
public:
    class rect
    {
    public:
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        rect() = default;

        constexpr rect(float x_, float y_, float width_, float height_)
            :x(x_), y(y_), width(width_), height(height_)
        {
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
            return y + height;
        }

        float right() const
        {
            return x + width;
        }

        bool empty() const
        {
            return (width <= 0.0f || height <= 0.0f);
        }

        bool contains(float pointx, float pointy) const
        {
            return (pointx >= x && pointx <= right()
                && pointy >= y && pointy <= top());
        }
    };

    using object_handle_type = decltype(AoiObject().handle);

    using node_rect = rect;

    using object_type = AoiObject;
private:
    using node_type = std::forward_list<object_type*>;
public:
    aoi(int posx, int posy, int length_of_area, int length_of_node)
        :area_(static_cast<float>(posx), static_cast<float>(posy), static_cast<float>(length_of_area), static_cast<float>(length_of_area))
        , length_of_node_(length_of_node)
        , length_of_area_(length_of_area)
        , count_(length_of_area / length_of_node)
    {
        assert(length_of_area%length_of_node == 0);
        data_ = new node_type[count_*count_];
    }

    ~aoi()
    {
        delete[] data_;
    }

    constexpr int get_index_x(float v)
    {
        return static_cast<int>(v-area_.x - 0.0001f) / length_of_node_;
    }

    constexpr int get_index_y(float v)
    {
        return static_cast<int>(v - area_.y - 0.0001f) / length_of_node_;
    }

    void insert(object_handle_type handle, float x, float y)
    {
        assert(area_.contains(x, y));
        auto res = objects_.try_emplace(handle, object_type{x,y,handle});
        if (res.second)
        {
            int index_x = get_index_x(x);
            int index_y = get_index_y(y);

            node_type& node = data_[index_x*count_ + index_y];
            node.emplace_front(&res.first->second);
        }
    }

    template<class T>
    static constexpr const T& clamp(const T& v, const T& lo, const T& hi)
    {
        assert(!(hi < lo));
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }

    node_rect make_around(float x, float y, float halfw, float halfh) const
    {
        auto left = clamp(x - halfw, area_.left(), area_.right());
        auto right = clamp(x + halfw, area_.left(), area_.right());
        auto bottom = clamp(y - halfh, area_.bottom(), area_.top());
        auto top = clamp(y + halfh, area_.bottom(), area_.top());
        return node_rect(left, bottom, right - left, top - bottom);
    }

    template<typename Handler>
    void query(float x, float y, float w, float h, const Handler& handler)
    {
        node_rect rc = make_around(x, y, w/2.0f, h / 2.0f);
        float start_x = rc.x;
        float start_y = rc.y;
        float end_x = rc.right();
        float end_y = rc.top();

        int start_index_x = get_index_x(start_x);
        int start_index_y = get_index_y(start_y);

        int end_index_x = get_index_x(end_x);
        int end_index_y = get_index_y(end_y);

        for (int i = start_index_x; i <= end_index_x; ++i)
        {
            auto offset = i * count_;
            bool is_x_edge = (i == start_index_x) || (i == end_index_x);
            for (int j = start_index_y; j <= end_index_y; ++j)
            {
                bool is_edge = is_x_edge || (j == start_index_y) || (j == end_index_y);
                node_type& nd = data_[offset + j];
                if (is_edge)
                {
                    for (const auto& o : nd)
                    {
                        if (rc.contains(o->x, o->y))
                        {
                            handler(o->handle);
                        }
                    }
                }
                else
                {
                    for (const auto& o : nd)
                    {
                        handler(o->handle);
                    }
                }
            }
        }
    }

    template<typename Handler>
    void query(object_handle_type handle, float w, float h, const Handler& handler)
    {
        auto iter = objects_.find(handle);
        if (iter == objects_.end())
        {
            return;
        }
        query(iter->second.x, iter->second.y, w, h, handler);
    }

    void update(object_handle_type handle, float x, float y)
    {
        assert(area_.contains(x, y));
        auto iter = objects_.find(handle);
        if (iter == objects_.end())
        {
            return;
        }

        int old_index_x = get_index_x(iter->second.x);
        int old_index_y = get_index_y(iter->second.y);

        int new_index_x = get_index_x(x);
        int new_index_y = get_index_y(y);

        iter->second.x = x;
        iter->second.y = y;

        if (old_index_x != new_index_x || old_index_y != new_index_y)
        {
            node_type& old_node = data_[old_index_x*count_ + old_index_y];
            old_node.remove(&iter->second);
            node_type& new_node = data_[new_index_x*count_ + new_index_y];
            new_node.push_front(&iter->second);
        }
    }

    void clear()
    {
        for (int i = 0; i < count_; ++i)
        {
            for (int j = 0; j < count_; ++j)
            {
                node_type& nd = data_[i*count_ + j];
                nd.clear();
            }
        }
        objects_.clear();
    }

    void erase(object_handle_type handle)
    {
        auto iter = objects_.find(handle);
        if (iter != objects_.end())
        {
            int old_index_x = get_index_x(iter->second.x);
            int old_index_y = get_index_y(iter->second.y);

            node_type& old_node = data_[old_index_x*count_ + old_index_y];
            old_node.remove(&iter->second);

            objects_.erase(iter);
        }
    }
private:
    const node_rect area_;
    const int length_of_node_;
    const int length_of_area_;
    const int count_;
    node_type* data_;
   std::unordered_map<object_handle_type, object_type> objects_;
};