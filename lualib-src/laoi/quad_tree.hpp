#pragma once
#include <vector>
#include <unordered_map>
#include "rect.hpp"

namespace math
{
    using objectid_t = int32_t;

    using node_rect = math::rect;

    template<size_t min_radius>
    class quad_tree_node
    {
    public:
        struct quad_tree_object
        {
            float x;
            float y;
            quad_tree_node* p;
        };

        // Construct a quadtree node with the given bounds 
        explicit quad_tree_node(quad_tree_node* parent, const node_rect& bounds)
            :parent_(parent)
            , bounds_(bounds)
        {
        }

        quad_tree_node(const quad_tree_node&) = delete;
        quad_tree_node& operator=(const quad_tree_node&) = delete;

        quad_tree_node(quad_tree_node&&) = default;
        quad_tree_node& operator=(quad_tree_node&&) = default;

        bool empty() const
        {
            return (bounds_.empty() || (contents_.empty() && nodes_.empty()));
        }

        const auto& contents() const
        {
            return contents_;
        }

        const node_rect& bounds() const
        {
            return bounds_;
        }

        size_t count() const
        {
            size_t n = 0;
            for (auto& node : nodes_)
            {
                n += node.count();
            }

            n += contents_.size();
            return n;
        }

        template<typename Handler>
        void subtree_contents(const Handler& handler) const
        {
            for (auto& node : nodes_)
            {
                node.subtree_contents(handler);
            }

            for (auto& item : contents_)
            {
                handler(item.first);
            }
        }

        template<typename Handler>
        void query(const node_rect& queryArea, const Handler& handler) const
        {
            auto hw = queryArea.width / 2.0f;
            auto hh = queryArea.height / 2.0f;
            auto midx = queryArea.x + hw;
            auto midy = queryArea.y + hh;
            for (auto& item : contents_)
            {
                if (std::abs(item.second->x - midx) <= hw && std::abs(item.second->y - midy) <= hh)
                {
                    handler(item.first);
                }
            }

            for (auto& node : nodes_)
            {
                if (node.empty())
                {
                    continue;
                }

                // Case 1: search area completely contained by sub-quad
                // if a node completely contains the query area, go down that branch
                // and skip the remaining nodes (break this loop)
                if (node.bounds().contains(queryArea))
                {
                    node.query(queryArea, handler);
                    break;
                }

                // Case 2: Sub-quad completely contained by search area 
                // if the query area completely contains a sub-quad,
                // just add all the contents of that quad and it's children 
                // to the result set. You need to continue the loop to test 
                // the other quads
                if (queryArea.contains(node.bounds()))
                {
                    node.subtree_contents(handler);
                    continue;
                }

                // Case 3: search area intersects with sub-quad
                // traverse into this quad, continue the loop to search other
                // quads


                if (node.bounds().intersects(queryArea))
                {
                    node.query(queryArea, handler);
                }
            }
        }

        quad_tree_node* insert(objectid_t uid, quad_tree_object* obj)
        {
            // if the item is not contained in this quad, there's a problem
            if (!bounds_.contains(obj->x, obj->y))
            {
                assert(false && "feature is out of the bounds of this quadtree node");
                return nullptr;
            }

            // if the subnodes are null create them. may not be sucessfull: see below
            // we may be at the smallest allowed size in which case the subnodes will not be created
            if (nodes_.size() == 0)
            {
                create_sub_nodes();
            }

            // for each subnode:
            // if the node contains the item, add the item to that node and return
            // this recurses into the node that is just large enough to fit this item
            for (auto& node : nodes_)
            {
                if (node.bounds().contains(obj->x, obj->y))
                {
                    return node.insert(uid, obj);
                }
            }

            // if we make it to here, either
            // 1) none of the subnodes completely contained the item. or
            // 2) we're at the smallest subnode size allowed 
            // add the item to this node's contents.
            contents_.emplace(uid, obj);
            return this;
        }

        template<typename Action>
        void foreach(Action&& f) const
        {
            f(this);

            for (auto& node : nodes_)
            {
                node.foreach(std::forward<Action>(f));
            }
        }

        void clear()
        {
            contents_.clear();
            for (auto& node : nodes_)
            {
                node.clear();
            }
        }

        void erase(objectid_t uid)
        {
            contents_.erase(uid);
        }

        quad_tree_node* parent() const
        {
            return parent_;
        }

    private:
        void create_sub_nodes()
        {
            // the smallest subnode has an area 
            if ((bounds_.width*bounds_.height) <= min_radius * min_radius)
                return;

            float halfw = bounds_.width / 2.0f;
            float halfh = bounds_.height / 2.0f;

            // left top node
            nodes_.emplace_back(this, node_rect(bounds_.x, bounds_.midy(), halfw, halfh));
            // left bottom node
            nodes_.emplace_back(this, node_rect(bounds_.x, bounds_.y, halfw, halfh));
            // right top node
            nodes_.emplace_back(this, node_rect(bounds_.midx(), bounds_.midy(), halfw, halfh));
            // right bottom node
            nodes_.emplace_back(this, node_rect(bounds_.midx(), bounds_.y, halfw, halfh));
        }

    private:
        quad_tree_node* parent_;
        // The area of this node
        node_rect bounds_;
        // The child nodes of the QuadTree
        std::vector<quad_tree_node> nodes_;
        // The objectss of the QuadTree
        std::unordered_map<objectid_t, quad_tree_object*> contents_;
    };

    template<size_t min_raduis>
    class quad_tree
    {
    public:
        using quad_tree_node_t = quad_tree_node<min_raduis>;
        using quad_tree_object_t = typename quad_tree_node<min_raduis>::quad_tree_object;

        explicit quad_tree(const node_rect& rc)
            :rect_(rc)
            , root_(nullptr, rc)
        {

        }

        quad_tree(const quad_tree&) = delete;
        quad_tree& operator=(const quad_tree&) = delete;

        size_t count()
        {
            return root_.count();
        }

        const math::node_rect& rect()
        {
            return rect_;
        }

        math::rect make_around(float x, float y, float halfw, float halfh) const
        {
            auto left = x - halfw;
            auto bottom = y - halfh;
            auto right = x + halfw;
            auto top = y + halfh;
            left = std::clamp(left, rect_.left(), rect_.right());
            right = std::clamp(right, rect_.left(), rect_.right());
            bottom = std::clamp(bottom, rect_.bottom(), rect_.top());
            top = std::clamp(top, rect_.bottom(), rect_.top());
            return node_rect(left, bottom, right - left, top - bottom);
        }

        void insert(objectid_t uid, float x, float y)
        {
            auto res = items_.try_emplace(uid, quad_tree_object_t{ x,y,nullptr });
            if (res.second)
            {
                auto node = root_.insert(uid, &res.first->second);
                assert(nullptr != node);
                res.first->second.p = node;
            }
        }

        void update(objectid_t uid, float x, float y)
        {
            auto iter = items_.find(uid);
            if (iter == items_.end())
            {
                return;
            }

            iter->second.x = x;
            iter->second.y = y;

            quad_tree_node_t* p = iter->second.p;
            if (!p->bounds().contains(x, y))
            {
                p->erase(uid);
                while (nullptr != (p = p->parent()))
                {
                    if (p->bounds().contains(x, y))
                    {
                        break;
                    }
                }
            }
            assert(nullptr != p);
            iter->second.p = p->insert(uid, &iter->second);;
            assert(nullptr != iter->second.p);
        }

        template<typename Handler>
        void query(objectid_t uid, float width, float height, const Handler& handler)
        {
            auto iter = items_.find(uid);
            if (iter == items_.end())
            {
                return;
            }
            node_rect rc = make_around(iter->second.x, iter->second.y, width / 2.0f, height / 2.0f);
            quad_tree_node_t* p = iter->second.p;
            do
            {
                if (p->bounds().contains(rc))
                {
                    p->query(rc, handler);
                    return;
                }
            } while (nullptr != (p = p->parent()));
            root_.query(rc, handler);
        }

        template<typename Handler>
        void query(const node_rect& rc, const Handler& handler)
        {
            root_.query(rc, handler);
        }

        template<typename Action>
        void foreach(Action&& f)
        {
            root_.foreach(std::forward<Action>(f));
        }

        const node_rect& rect() const
        {
            return rect_;
        }

        void clear()
        {
            root_.clear();
        }

        void erase(objectid_t uid)
        {
            auto iter = items_.find(uid);
            if (iter != items_.end())
            {
                iter->second.p->erase(uid);
                items_.erase(iter);
            }
        }
    private:
        const node_rect rect_;
        quad_tree_node_t root_;
        std::unordered_map<int64_t, quad_tree_object_t> items_;
    };
}

