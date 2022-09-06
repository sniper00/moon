//This file is modified version from https://github.com/redis/redis/blob/unstable/src/t_zset.c
#include <random>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <limits>

namespace moon
{
    class skip_list_iterator_sentinel {};

    template<typename SkipListType>
    class skip_list_iterator
    {
        using node_type = typename SkipListType::node_type;
        using score_type = typename SkipListType::score_type;
        node_type* node_ = nullptr;
    public:
        explicit skip_list_iterator(node_type* n)
            :node_(n)
        {
        }

        const score_type& operator*() const
        {
            return node_->score;
        }

        score_type* operator->() const
        {
            return &node_->score;
        }

        skip_list_iterator& operator++()
        {
            node_ = node_->level[0].forward;
            return *this;
        }

        skip_list_iterator& operator--()
        {
            node_ = node_->backward;
            return *this;
        }

        bool operator!=(const skip_list_iterator_sentinel) const {
            return node_ != nullptr;
        }

        bool operator==(const skip_list_iterator_sentinel) const {
            return node_ == nullptr;
        }
    };

    template<typename Score>
    class skip_list
    {
    private:
        static constexpr int MAXLEVEL = 32;
        static constexpr double PERCENT = 0.25;

        using score_type = Score;

        struct node_type;
        struct level_type
        {
            node_type* forward;
            size_t span;
        };

        struct node_type
        {
            score_type score;
            node_type* backward;
            level_type level[1];
        };

        static unsigned int random(unsigned int Min, unsigned int Max)
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<unsigned int> dis(Min, Max);
            return dis(gen);
        }

        static int rand_level()
        {
            static const int threshold = (int)(PERCENT * std::numeric_limits<int16_t>::max());
            int level = 1;
            while (random(1, std::numeric_limits<int16_t>::max()) < threshold)
                level += 1;
            return (level < MAXLEVEL) ? level : MAXLEVEL;
        }

        static node_type* make_node(int level, score_type score)
        {
            size_t size = sizeof(node_type) + level * sizeof(level_type);
            node_type* n = (node_type*)::malloc(size);
            if (nullptr == n)
            {
                fprintf(stderr, "malloc: Out of memory trying to allocate %zu bytes\n", size);
                fflush(stderr);
                ::abort();
            }
            n->score = score;
            return n;
        }

        static void free_node(node_type* node)
        {
            ::free(node);
        }

        void remove_node(node_type* x, node_type** update)
        {
            int i;
            for (i = 0; i < level_; i++)
            {
                if (update[i]->level[i].forward == x)
                {
                    update[i]->level[i].span += x->level[i].span - 1;
                    update[i]->level[i].forward = x->level[i].forward;
                }
                else
                {
                    update[i]->level[i].span -= 1;
                }
            }
            if (x->level[0].forward)
            {
                x->level[0].forward->backward = x->backward;
            }
            else
            {
                tail_ = x->backward;
            }
            while (level_ > 1 && header_->level[level_ - 1].forward == NULL)
                level_--;
            length_--;
        }

        void free_all()
        {
            if (header_ != nullptr)
            {
                node_type* node = header_->level[0].forward, * next;
                ::free(header_);
                while (node)
                {
                    next = node->level[0].forward;
                    free_node(node);
                    node = next;
                }
                header_ = nullptr;
            }
        }
    public:
        friend class skip_list_iterator< skip_list>;

        using const_iterator = skip_list_iterator< skip_list>;


        skip_list()
        {
            clear();
        }

        skip_list(const skip_list&) = delete;
        skip_list& operator=(const skip_list&) =delete;

        ~skip_list()
        {
            free_all();
        }

        void clear()
        {
            free_all();

            level_ = 1;
            length_ = 0;
            header_ = make_node(MAXLEVEL, score_type{});
            for (int i = 0; i < MAXLEVEL; ++i)
            {
                header_->level[i].forward = nullptr;
                header_->level[i].span = 0;
            }
            header_->backward = nullptr;
            tail_ = nullptr;
        }

        const_iterator insert(score_type score)
        {
            node_type* update[MAXLEVEL] = { nullptr };

            size_t rank[MAXLEVEL] = { 0 };

            node_type* x = header_;
            for (int i = level_ - 1; i >= 0; --i)
            {
                /* store rank that is crossed to reach the insert position */
                rank[i] = i == (level_ - 1) ? 0 : rank[i + 1];
                while (x->level[i].forward && x->level[i].forward->score < score)
                {
                    rank[i] += x->level[i].span;
                    x = x->level[i].forward;
                }
                update[i] = x;
            }

            int level = rand_level();
            if (level > level_)
            {
                for (int i = level_; i < level; ++i)
                {
                    rank[i] = 0;
                    update[i] = header_;
                    update[i]->level[i].span = length_;
                }
                level_ = level;
            }

            x = make_node(level, score);
            for (int i = 0; i < level; ++i)
            {
                x->level[i].forward = update[i]->level[i].forward;
                update[i]->level[i].forward = x;

                /* update span covered by update[i] as x is inserted here */
                x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
                update[i]->level[i].span = (rank[0] - rank[i]) + 1;
            }

            /* increment span for untouched levels */
            for (int i = level; i < level_; i++) {
                update[i]->level[i].span++;
            }

            x->backward = (update[0] == header_) ? NULL : update[0];
            if (x->level[0].forward)
                x->level[0].forward->backward = x;
            else
                tail_ = x;
            length_++;
            return const_iterator{ x };
        }

        const_iterator update(score_type curscore, score_type newscore)
        {
            node_type* update[MAXLEVEL] = { nullptr };
            node_type* x = nullptr;
            int i;

            /* We need to seek to element to update to start: this is useful anyway,
             * we'll have to update or remove it. */
            x = header_;
            for (i = level_ - 1; i >= 0; i--)
            {
                while (x->level[i].forward && x->level[i].forward->score < curscore)
                {
                    x = x->level[i].forward;
                }
                update[i] = x;
            }

            /* Jump to our element: note that this function assumes that the
             * element with the matching score exists. */
            x = x->level[0].forward;
            assert(x && curscore == x->score);

            /* If the node, after the score update, would be still exactly
             * at the same position, we can just update the score without
             * actually removing and re-inserting the element in the skiplist. */
            if ((x->backward == NULL || x->backward->score < newscore) &&
                (x->level[0].forward == NULL || x->level[0].forward->score > newscore))
            {
                x->score = newscore;
                return const_iterator{ x };
            }

            /* No way to reuse the old node: we need to remove and insert a new
             * one at a different place. */
            remove_node(x, update);
            const_iterator newnode = insert(newscore);
            free_node(x);
            return newnode;
        }

        /* Find the rank for an element by both score and key.
     * Returns 0 when the element cannot be found, rank otherwise.
     * Note that the rank is 1-based due to the span of zsl->header to the
     * first element. */
        size_t get_rank(score_type score) const
        {
            node_type* x = header_;
            size_t rank = 0;
            int i;

            for (i = level_ - 1; i >= 0; i--)
            {
                while (x->level[i].forward && x->level[i].forward->score <= score)
                {
                    rank += x->level[i].span;
                    x = x->level[i].forward;
                }

                /* x might be equal to zsl->header, so test if obj is non-NULL */
                if (x->score == score)
                {
                    return rank;
                }
            }
            return 0;
        }

        /* Finds an element by its rank. The rank argument needs to be 1-based. */
        const_iterator find_by_rank(size_t rank) const
        {
            node_type* x = header_;
            size_t traversed = 0;
            int i;

            for (i = level_ - 1; i >= 0; i--)
            {
                while (x->level[i].forward && (traversed + x->level[i].span) <= rank)
                {
                    traversed += x->level[i].span;
                    x = x->level[i].forward;
                }
                if (traversed == rank)
                {
                    return const_iterator{ x };
                }
            }
            return const_iterator{ nullptr };
        }

        /* Delete an element with matching score/element from the skiplist.
        * The function returns 1 if the node was found and deleted, otherwise
        * 0 is returned.
        *
        * If 'node' is NULL the deleted node is freed by zslFreeNode(), otherwise
        * it is not freed (but just unlinked) and *node is set to the node pointer,
        * so that it is possible for the caller to reuse the node (including the
        * referenced SDS string at node->ele). */
        size_t erase(score_type score)
        {
            node_type* update[MAXLEVEL] = { nullptr };
            node_type* x = header_;
            int i;

            for (i = level_ - 1; i >= 0; i--)
            {
                while (x->level[i].forward && x->level[i].forward->score < score)
                {
                    x = x->level[i].forward;
                }
                update[i] = x;
            }
            /* We may have multiple elements with the same score, what we need
             * is to find the element with both the right score and object. */
            x = x->level[0].forward;
            if (x && score == x->score)
            {
                remove_node(x, update);
                free_node(x);
                return 1;
            }
            return 0; /* not found */
        }

        const_iterator begin() const
        {
            return const_iterator{ header_->level[0].forward };
        }

        skip_list_iterator_sentinel end() const
        {
            return skip_list_iterator_sentinel{};
        }

        const_iterator tail() const
        {
            return const_iterator{ tail_ };
        }

        size_t size() const
        {
            return length_;
        }
    private:
        size_t length_ = 0;
        int level_ = 1;
        node_type* header_ = nullptr;
        node_type* tail_ = nullptr;
    };

    class zset
    {
        struct context
        {
            int64_t key = 0;
            int64_t score = 0;
            int64_t timestamp = 0;
            uint32_t rank = 0;

            bool operator==(const context& val) const
            {
                return key == val.key;
            }

            bool operator<(const context& val) const
            {
                if (score == val.score)
                {
                    if (timestamp == val.timestamp)
                    {
                        return key < val.key;
                    }
                    return timestamp < val.timestamp;
                }
                return score > val.score;
            }

            bool operator<=(const context& val) const
            {
                if (key == val.key)
                    return true;

                if (score == val.score)
                {
                    if (timestamp == val.timestamp)
                    {
                        return key < val.key;
                    }
                    return timestamp < val.timestamp;
                }
                return score > val.score;
            }

            bool operator>(const context& val) const
            {
                if (score == val.score)
                {
                    if (timestamp == val.timestamp)
                    {
                        return key < val.key;
                    }
                    return timestamp < val.timestamp;
                }
                return score < val.score;
            }
        };
    public:
        using const_iterator = skip_list<context>::const_iterator;

        zset(size_t max_count = std::numeric_limits<size_t>::max(), bool reverse = false)
            : reverse_(reverse)
            , max_count_(max_count)
        {}

        void update(int64_t key, int64_t score, int64_t timestamp)
        {
            if (max_count_ == 0 || key == 0)
                return;

            if (reverse_)
                score = -score;

            auto iter = dict_.find(key);
            if (dict_.size() == max_count_ && iter == dict_.end() && *zsl_.tail() < context{key, score, timestamp})
            {
                return;
            }

            if (iter == dict_.end())
            {
                auto it = zsl_.insert(context{ key, score, timestamp });
                dict_.emplace(key, &(*it));
            }
            else
            {
                auto it = zsl_.update(*iter->second, context{ key, score, timestamp });
                iter->second = &(*it);
            }

            if (dict_.size() > max_count_)
            {
                erase((*zsl_.tail()).key);
            }
        }

        size_t rank(int64_t key) const
        {
            if (auto iter = dict_.find(key); iter != dict_.end())
            {
                return zsl_.get_rank(*iter->second);
            }
            return 0;
        }

        int64_t score(int64_t key) const
        {
            auto iter = dict_.find(key);
            if (iter != dict_.end())
            {
                return iter->second->score;
            }
            return 0;
        }

        bool has(int64_t key) const
        {
            return (dict_.find(key) != dict_.end());
        }

        void clear()
        {
            dict_.clear();
            zsl_.clear();
        }

        size_t erase(int64_t key)
        {
            auto iter = dict_.find(key);
            if (iter != dict_.end())
            {
                zsl_.erase(*iter->second);
                dict_.erase(iter);
                return 1;
            }
            return 0;
        }

        const_iterator find_by_rank(size_t rank) const
        {
            return zsl_.find_by_rank(rank);
        }

        const_iterator begin() const
        {
            return zsl_.begin();
        }

        const_iterator tail() const
        {
            return zsl_.tail();
        }

        skip_list_iterator_sentinel end() const
        {
            return zsl_.end();
        }

        size_t size() const
        {
            assert(dict_.size() == zsl_.size());
            return dict_.size();
        }
    private:
        bool reverse_ = false;
        const size_t max_count_;
        skip_list<context> zsl_;
        std::unordered_map<int64_t, const context*> dict_;
    };
} // namespace moon