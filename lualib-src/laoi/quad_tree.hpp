#pragma once
#include <vector>
#include <unordered_map>
#include "rect.hpp"

namespace math
{
    using objectid_t = int32_t;

    static constexpr int32_t aoi_radius = 50;

    using node_rect = math::rect;

    class quad_tree_node
    {
    public:

        struct quad_tree_object
        {
            float x;
            float y;
            objectid_t uid;
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

        void subtree_contents(std::vector<objectid_t>& ret) const
        {
            for (auto& node : nodes_)
            {
                node.subtree_contents(ret);
            }

            for (auto& item : contents_)
            {
                ret.push_back(item.first);
            }
        }

        void query(const node_rect& queryArea, std::vector<objectid_t>& ret) const
        {
            for (auto& item : contents_)
            {
                if (queryArea.contains(item.second.x, item.second.y))
                {
                    ret.push_back(item.first);
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
                    node.query(queryArea, ret);
                    break;
                }

                // Case 2: Sub-quad completely contained by search area 
                // if the query area completely contains a sub-quad,
                // just add all the contents of that quad and it's children 
                // to the result set. You need to continue the loop to test 
                // the other quads
                if (queryArea.contains(node.bounds()))
                {
                    node.subtree_contents(ret);
                    continue;
                }

                // Case 3: search area intersects with sub-quad
                // traverse into this quad, continue the loop to search other
                // quads

                if (node.bounds().intersects(queryArea))
                {
                    node.query(queryArea, ret);
                }
            }
        }

        quad_tree_node* insert(objectid_t uid, float x, float y)
        {
            // if the item is not contained in this quad, there's a problem
            if (!bounds_.contains(x, y))
            {
                assert(false && "feature is out of the bounds of this quadtree node");
                return nullptr;
            }

            auto iter = contents_.find(uid);
            if (iter != contents_.end())
            {
                iter->second.x = x;
                iter->second.y = y;
                return this;
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
                if (node.bounds().contains(x, y))
                {
                    return node.insert(uid, x, y);
                }
            }

            // if we make it to here, either
            // 1) none of the subnodes completely contained the item. or
            // 2) we're at the smallest subnode size allowed 
            // add the item to this node's contents.
            contents_.emplace(uid, quad_tree_object{ x,y,uid });
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

        void update(objectid_t uid, float x, float y)
        {
            auto iter = contents_.find(uid);
            assert(iter != contents_.end());
            assert(bounds_.contains(x, y));
            iter->second.x = x;
            iter->second.y = y;
        }

        quad_tree_node* parent() const
        {
            return parent_;
        }

    private:
        void create_sub_nodes()
        {
            // the smallest subnode has an area 
            if ((bounds_.width*bounds_.height) <= aoi_radius * aoi_radius)
                return;

            float halfw = bounds_.width / 2;
            float halfh = bounds_.height / 2;

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
        std::unordered_map<objectid_t, quad_tree_object> contents_;
    };

    class quad_tree
    {
    public:
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

        void insert(objectid_t uid, float x, float y)
        {
            auto node = root_.insert(uid, x, y);
            assert(nullptr != node);
            items_.try_emplace(uid, node);
        }

        void update(objectid_t uid, float x, float y)
        {
            auto iter = items_.find(uid);
            if (iter != items_.end())
            {
                quad_tree_node* n = iter->second;
                quad_tree_node* p = n;
                do
                {
                    if (nullptr == p)
                    {
                        break;
                    }

                    if (!p->bounds().contains(x, y))
                    {
                        if (p == n)
                        {
                            p->erase(uid);
                            items_.erase(uid);
                        }
                        p = p->parent();
                        continue;
                    }
                    auto node = p->insert(uid, x, y);;
                    assert(nullptr != node);
                    if (node != n)
                    {
                        items_.try_emplace(uid, node);
                    }
                    return;
                } while (true);
            }
            insert(uid, x, y);
        }

        void query(objectid_t uid, float width, float height, std::vector<objectid_t>& out)
        {
            auto iter = items_.find(uid);
            if (iter != items_.end())
            {
                auto o = iter->second->contents().find(uid);
                if (o == iter->second->contents().end())
                {
                    return;
                }
                node_rect rc(o->second.x - width / 2, o->second.y - height / 2, width, height);
                quad_tree_node* p = iter->second;
                do
                {
                    if (nullptr == p)
                    {
                        break;
                    }
                    if (!p->bounds().contains(rc))
                    {
                        p = p->parent();
                        continue;
                    }
                    p->query(rc, out);
                    return;
                } while (true);
                root_.query(rc, out);
            }
        }

        void query(const node_rect& rc, std::vector<objectid_t>& out)
        {
            root_.query(rc, out);
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
                iter->second->erase(uid);
                items_.erase(iter);
            }
        }

    private:
        const node_rect rect_;
        quad_tree_node root_;
        std::unordered_map<int64_t, quad_tree_node*> items_;
    };
}

