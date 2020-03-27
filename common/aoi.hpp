#pragma once
#include <unordered_map>
#include <forward_list>
#include <unordered_set>
#include <cassert>
#include <functional>
#include <iostream>
#include <algorithm>
#include "math.hpp"

template<class AoiObject>
class aoi
{
public:
    enum mode
    {
        watcher = 1, //Observer
        marker = 1 << 1
    };

    enum event_type
    {
        event_enter = 1,
        event_leave = 2,
    };

    using object_type = AoiObject;

    using object_handle_type = typename object_type::handle_type;

    struct aoi_event
    {
        int eventid = 0; //enum event_type
        object_handle_type watcher = object_handle_type{};
        object_handle_type marker = object_handle_type{};

        aoi_event(int eid, object_handle_type w, object_handle_type m)
            :eventid(eid)
            , watcher(w)
            , marker(m)
        {

        }
    };
private:
    struct tile
    {
        std::forward_list<object_type*> markers;
        std::unordered_set<object_type*> watchers;
    };
public:
    aoi(int posx, int posy, int map_size, int tile_size)
        :rect_(posx, posy, map_size, map_size)
        , tile_size_(tile_size)
        , map_size_(map_size)
        , count_(map_size / tile_size)
    {
        assert(map_size%tile_size == 0);
        data_ = new tile[count_*count_];
    }

    ~aoi()
    {
        delete[] data_;
    }

    constexpr int get_tile_x(int v) const
    {
        auto res = (v - rect_.x) / tile_size_;
        if (res >= count_)
        {
            res = count_ - 1;
        }
        return res;
    }

    constexpr int get_tile_y(int v) const
    {
        auto res = (v - rect_.y) / tile_size_;
        if (res >= count_)
        {
            res = count_ - 1;
        }
        return res;
    }

    template<class T>
    static constexpr const T& clamp(const T& v, const T& lo, const T& hi)
    {
        assert(!(hi < lo));
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }

    rect<int> make_rect(int x, int y, int w, int h) const
    {
        auto left = (clamp(x - w / 2, rect_.left(), rect_.right()));
        auto right = (clamp(x + w / 2, rect_.left(), rect_.right()));
        auto bottom = (clamp(y - h / 2, rect_.bottom(), rect_.top()));
        auto top = (clamp(y + h / 2, rect_.bottom(), rect_.top()));
        return rect<int>{ left, bottom, right - left, top - bottom };
    }

    rect<int> make_tile_rect(int x, int y, int w, int h) const
    {
        auto left = get_tile_x(clamp(x - w / 2, rect_.left(), rect_.right()));
        auto right = get_tile_x(clamp(x + w / 2, rect_.left(), rect_.right()));
        auto bottom = get_tile_y(clamp(y - h / 2, rect_.bottom(), rect_.top()));
        auto top = get_tile_y(clamp(y + h / 2, rect_.bottom(), rect_.top()));
        return rect<int>{ left, bottom, right - left, top - bottom };
    }

    bool insert(object_handle_type handle, int x, int y, int w, int h, int layer, int mode, bool range_marker = false)
    {
        if (!rect_.contains(x, y))
        {
            return false;
        }

        auto res = objects_.try_emplace(handle, object_type{ x, y, w, h, layer, mode, handle });
        if (res.second)
        {
            int tile_x = get_tile_x(x);
            int tile_y = get_tile_y(y);

            if (mode&marker)
            {
                if (range_marker && w > 0 && h > 0)
                {
                    assert(!(mode&watcher)&&"unsupport");
                    auto tile_rect = make_tile_rect(x, y, w, h);
                    for_each_rect(tile_rect, [this, &res](int x, int y) {
                        insert_marker(&res.first->second, x, y);
                    });
                }
                else
                {
                    insert_marker(&res.first->second, tile_x, tile_y);
                }
            }

            if (mode&watcher)
            {
                auto tile_rect = make_tile_rect(x, y, w, h);
                auto rc = make_rect(x, y, w, h);

                for_each_rect(tile_rect, [this, &res, &rc](int x, int y) {
                    tile& t = data_[y*count_ + x];
                    insert_watcher(t, &res.first->second);
                    if (debug_)
                    {
                        std::cout << res.first->first << " watch (" << x << "," << y << ")" << std::endl;
                    }
                    update_watcher(t, rect<int>{-1, -1, 0, 0}, rc, &res.first->second);
                });
            }
            return true;
        }
        return false;
    }

    void fire_event(object_handle_type handle, int eventid)
    {
        auto iter = objects_.find(handle);
        if (iter == objects_.end())
        {
            return;
        }
        object_type* obj = &iter->second;
        int tile_x = get_tile_x(obj->x);
        int tile_y = get_tile_y(obj->y);
        tile& t = data_[tile_y*count_ + tile_x];
        marker_event(t, obj, eventid);
    }

    // update pos, view width, view height, layer
    bool update(object_handle_type handle, int x, int y, int w, int h, int layer)
    {
        if (!rect_.contains(x, y) || w < 0 || h < 0)
        {
            return false;
        }

        auto iter = objects_.find(handle);
        if (iter == objects_.end())
        {
            return false;
        }

        object_type* obj = &iter->second;

        auto old_rect = make_rect(obj->x, obj->y, obj->w, obj->h);
        auto old_tile_rect = make_tile_rect(obj->x, obj->y, obj->w, obj->h);

        auto old_x = obj->x;
        auto old_y = obj->y;
        obj->x = x;
        obj->y = y;
        obj->h = h;
        obj->w = w;
        obj->layer = layer;

        if (iter->second.mode&marker)
        {
            update_marker(obj, old_x, old_y);
        }

        while (iter->second.mode&watcher)
        {
            auto new_rect = make_rect(x, y, w, h);
            auto new_tile_rect = make_tile_rect(x, y, w, h);

            if (old_rect.contains(new_rect))
            {
                for_each_rect(old_tile_rect, [this, &new_rect, &new_tile_rect, &old_rect, &obj](int x, int y) {
                    auto rc = rect<int>{ x*tile_size_, y*tile_size_, tile_size_, tile_size_ };
                    if (new_rect.contains(rc))
                    {
                        return;
                    }

                    tile& t = data_[y*count_ + x];
                    if (!new_tile_rect.contains(x, y))
                    {
                        remove_watcher(t, obj);
                        if (debug_)
                        {
                            std::cout << obj->handle << " unwatch (" << x << "," << y << ")" << std::endl;
                        }
                    }

                    update_watcher(t, old_rect, new_rect, obj);
                });
                break;
            }

            if (old_rect.contains(new_rect))
            {
                for_each_rect(new_tile_rect, [this, &new_rect, &old_tile_rect, &old_rect, &obj](int x, int y) {
                    auto rc = rect<int>{ x*tile_size_, y*tile_size_, tile_size_, tile_size_ };
                    if (old_rect.contains(rc))
                    {
                        return;
                    }

                    tile& t = data_[y*count_ + x];
                    if (!old_tile_rect.contains(x, y))
                    {
                        insert_watcher(t, obj);
                        if (debug_)
                        {
                            std::cout << obj->handle << " watch (" << x << "," << y << ")" << std::endl;
                        }
                    }

                    update_watcher(t, old_rect, new_rect, obj);
                });
                break;
            }

            for_each_rect(old_tile_rect, [this, &new_rect, &new_tile_rect, &old_rect, &obj](int x, int y) {
                auto rc = rect<int>{ x*tile_size_, y*tile_size_, tile_size_, tile_size_ };
                if (new_rect.contains(rc))
                {
                    return;
                }

                tile& t = data_[y*count_ + x];
                if (!new_tile_rect.contains(x, y))
                {
                    remove_watcher(t, obj);

                    if (debug_)
                    {
                        std::cout << obj->handle << " unwatch (" << x << "," << y << ")" << std::endl;
                    }
                }
                //std::cout << " update_watcher1 (" << x << "," << y << ")" << std::endl;
                update_watcher(t, old_rect, new_rect, obj, false, true);
            });

            for_each_rect(new_tile_rect, [this, &new_rect, &old_tile_rect, &old_rect, &obj](int x, int y) {
                auto rc = rect<int>{ x*tile_size_, y*tile_size_, tile_size_, tile_size_ };
                if (old_rect.contains(rc))
                {
                    return;
                }

                tile& t = data_[y*count_ + x];
                if (!old_tile_rect.contains(x, y))
                {
                    insert_watcher(t, obj);

                    if (debug_)
                    {
                        std::cout << obj->handle << " watch (" << x << "," << y << ")" << std::endl;
                    }
                }
                //std::cout << " update_watcher2 (" << x << "," << y << ")" << std::endl;
                update_watcher(t, old_rect, new_rect, obj, true, false);
            });
            break;
        }

        return true;
    }

    template<typename... Args>
    void query(int x, int y, int w, int h, std::vector<int64_t>& out, Args&&...args)
    {
        auto rc = make_rect(x, y, w, h);
        auto tile_rc = make_tile_rect(x, y, w, h);

        int start_index_x = tile_rc.x;
        int start_index_y = tile_rc.y;

        int end_index_x = tile_rc.right();
        int end_index_y = tile_rc.top();

        for (int i = start_index_x; i <= end_index_x; ++i)
        {
            bool is_x_edge = (i == start_index_x) || (i == end_index_x);
            for (int j = start_index_y; j <= end_index_y; ++j)
            {
                bool is_edge = is_x_edge || (j == start_index_y) || (j == end_index_y);
                tile& node = data_[j*count_ + i];
                if (is_edge)
                {
                    for (auto& m : node.markers)
                    {
                        if (m->inside(rc)&&m->check(std::forward<Args>(args)...))
                        {
                            out.push_back(m->handle);
                        }
                    }
                }
                else
                {
                    for (auto& m : node.markers)
                    {
                        if (m->check(std::forward<Args>(args)...))
                        {
                            out.push_back(m->handle);
                        }
                    }
                }
            }
        }
    }

    void clear()
    {
        for (int i = 0; i < count_; ++i)
        {
            for (int j = 0; j < count_; ++j)
            {
                tile& n = data_[i*count_ + j];
                n.markers.clear();
                n.watchers.clear();
            }
        }
        objects_.clear();
    }

    void erase(object_handle_type handle)
    {
        auto iter = objects_.find(handle);
        if (iter != objects_.end())
        {
            if (iter->second.mode&marker)
            {
                if (iter->second.w > 0 && iter->second.h > 0)
                {
                    auto tile_rect = make_tile_rect(iter->second.x, iter->second.y, iter->second.w, iter->second.h);
                    for_each_rect(tile_rect, [this, &iter](int x, int y) {
                        remove_marker(&iter->second, x, y);
                    });
                }
                else
                {
                    int tile_x = get_tile_x(iter->second.x);
                    int tile_y = get_tile_y(iter->second.y);
                    remove_marker(&iter->second, tile_x, tile_y);
                }
            }

            if (iter->second.mode&watcher)
            {
                auto tile_rc = make_tile_rect(iter->second.x, iter->second.y, iter->second.w, iter->second.h);

                for_each_rect(tile_rc, [this, &iter](int x, int y) {
                    tile& node = data_[y*count_ + x];
                    if (debug_)
                    {
                        std::cout << iter->first << " unwatch (" << x << "," << y << ")" << std::endl;
                    }
                    node.watchers.erase(&iter->second);
                });
            }

            objects_.erase(iter);
        }
    }

    void enable_debug(bool v)
    {
        debug_ = v;
    }

    void enbale_leave_event(bool v)
    {
        enable_leave_event_ = v;
    }

    bool has_object(object_handle_type handle)
    {
        return objects_.find(handle) != objects_.end();
    }

    void clear_event()
    {
        event_queue_.clear();
    }

    const std::vector<aoi_event>& get_event() const
    {
        return event_queue_;
    }

    template<typename Handler>
    void for_each_all(const Handler& hander, int filter) const
    {
        for (int y = 0; y < count_; ++y)
        {
            for (int x = 0; x < count_; ++x)
            {
                tile& node = data_[y*count_ + x];
                for (auto& m : node.markers)
                {
                    if (m->mode&filter)
                    {
                        hander(m->handle, m->x, m->y, x, y);
                    }
                }

                for (auto& w : node.watchers)
                {
                    if (w->mode&filter)
                    {
                        hander(w->handle, w->x, w->y, x, y);
                    }
                }
            }
        }
    }

    object_type* find(object_handle_type handle)
    {
        auto iter = objects_.find(handle);
        if (iter != objects_.end())
        {
            return &iter->second;
        }
        return nullptr;
    }
private:
    void insert_marker(object_type* obj, int tile_x, int tile_y)
    {
        tile& node = data_[tile_y*count_ + tile_x];
        node.markers.emplace_front(obj);

        for (const auto& w : node.watchers)
        {
            if (w->handle == obj->handle) continue;

            auto rc = make_rect(w->x, w->y, w->w, w->h);
            if (!rc.contains(obj->x, obj->y))
            {
                continue;
            }

            event_queue_.emplace_back(static_cast<int>(event_enter), w->handle, obj->handle);
        }
    }

    void remove_marker(object_type* obj, int tile_x, int tile_y)
    {
        tile& node = data_[tile_y*count_ + tile_x];
        node.markers.remove(obj);

        for (const auto& w : node.watchers)
        {
            if (w->handle == obj->handle) continue;

            auto rc = make_rect(w->x, w->y, w->w, w->h);
            if (!rc.contains(obj->x, obj->y))
            {
                continue;
            }

            event_queue_.emplace_back(static_cast<int>(event_leave), w->handle, obj->handle);
        }
    }

    void update_marker(object_type* obj,  int old_x, int old_y)
    {
        int old_tile_x = get_tile_x(old_x);
        int old_tile_y = get_tile_y(old_y);

        int new_tile_x = get_tile_x(obj->x);
        int new_tile_y = get_tile_y(obj->y);

        tile& old_node = data_[old_tile_y*count_ + old_tile_x];
        tile& node = data_[new_tile_y*count_ + new_tile_x];

        if (&old_node != &node)
        {
            old_node.markers.remove(obj);
            node.markers.emplace_front(obj);
            if (debug_)
            {
                std::cout << obj->handle << " insert (" << new_tile_x << "," << new_tile_y << ")" << std::endl;
            }
        }

        if (enable_leave_event_)
        {
            for (const auto& w : old_node.watchers)
            {
                if (w->handle == obj->handle) continue;
                auto rc = make_rect(w->x, w->y, w->w, w->h);
                if (!rc.contains(old_x, old_y) || rc.contains(obj->x, obj->y))
                {
                    continue;
                }
                event_queue_.emplace_back(static_cast<int>(event_leave), w->handle, obj->handle);
            }
        }

        for (const auto& w : node.watchers)
        {
            if (w->handle == obj->handle) continue;

            auto rc = make_rect(w->x, w->y, w->w, w->h);
            if (!rc.contains(obj->x, obj->y) || rc.contains(old_x, old_y))
            {
                continue;
            }

            event_queue_.emplace_back(static_cast<int>(event_enter), w->handle, obj->handle);
        }
    }

    void marker_event(tile& node, object_type* obj, int eventid)
    {
        assert(std::find(node.markers.begin(), node.markers.end(), obj) != node.markers.end());
        for (const auto& w : node.watchers)
        {
            if (w->handle == obj->handle) continue;

            auto rc = make_rect(w->x, w->y, w->w, w->h);
            if (!rc.contains(obj->x, obj->y))
            {
                continue;
            }

            event_queue_.emplace_back(static_cast<int>(eventid), w->handle, obj->handle);
        }
    }

    void insert_watcher(tile& node, object_type* obj)
    {
        [[maybe_unused]] bool ok = node.watchers.emplace(obj).second;
        assert(ok);
    }

    void remove_watcher(tile& node, object_type* obj)
    {
        [[maybe_unused]] size_t count = node.watchers.erase(obj);
        assert(1 == count);
    }

    void update_watcher(const tile& t,
        const rect<int>& old_rect,
        const rect<int>& new_rect,
        object_type* obj,
        bool check_enter = true,
        bool check_leave = true)
    {
        for (auto& m : t.markers)
        {
            if (obj->handle == m->handle) continue;
            bool in_old_view = old_rect.contains(m->x, m->y);
            bool in_new_view = new_rect.contains(m->x, m->y);
            if (in_old_view)
            {
                if (enable_leave_event_)
                {
                    if (!in_new_view&&check_leave)
                    {
                        event_queue_.emplace_back(static_cast<int>(event_leave), obj->handle, m->handle);
                    }
                }
            }
            else
            {
                if (in_new_view&&check_enter)
                {
                    event_queue_.emplace_back(static_cast<int>(event_enter), obj->handle, m->handle);
                }
            }
        }
    }

    template<typename Handler>
    void for_each_rect(const rect<int> rc, const Handler& hander)
    {
        for (int i = rc.left(); i <= rc.right(); ++i)
        {
            for (int j = rc.bottom(); j <= rc.top(); ++j)
            {
                hander(i, j);
            }
        }
    }
private:
    bool debug_ = false;
    bool enable_leave_event_ = false;
    const rect<int> rect_;
    const int tile_size_;
    const int map_size_;
    const int count_;//map_size_ / tile_size_
    tile* data_;//count * count
    std::unordered_map<object_handle_type, object_type> objects_;
    std::vector<aoi_event> event_queue_;
};
