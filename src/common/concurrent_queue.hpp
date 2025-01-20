#pragma once
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>
#include <vector>

namespace moon {
// Unbounded queue
template<
    class T,
    typename Lock = std::mutex,
    template<typename Elem, typename = std::allocator<Elem>> class Container = std::vector>
class concurrent_queue {
public:
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_move_assignable_v<T>);

    using container_type = Container<T>;
    using lock_type = Lock;

    concurrent_queue() = default;
    concurrent_queue(const concurrent_queue& t) = delete;
    concurrent_queue& operator=(const concurrent_queue& t) = delete;

    template<typename Value>
    size_t push_back(Value&& x) {
        std::lock_guard<lock_type> lk(mutex_);
        write_queue_.push_back(std::forward<Value>(x));
        return write_queue_.size();
    }

    template<typename... Args>
    size_t emplace_back(Args&&... args) {
        std::lock_guard<lock_type> lk(mutex_);
        write_queue_.emplace_back(std::forward<Args>(args)...);
        return write_queue_.size();
    }

    bool try_pop(T& t) {
        std::lock_guard<lock_type> lk(mutex_);
        if (write_queue_.empty())
            return false;
        t = std::move(write_queue_.front());
        write_queue_.pop_front();
        return true;
    }

    size_t size() const {
        std::lock_guard<lock_type> lk(mutex_);
        return write_queue_.size();
    }

    size_t capacity() const {
        std::lock_guard<lock_type> lk(mutex_);
        return write_queue_.capacity();
    }

    container_type& swap_on_read() {
        std::lock_guard<lock_type> lk(mutex_);
        write_queue_.swap(read_queue_);
        return read_queue_;
    }

private:
    mutable lock_type mutex_;
    container_type write_queue_;
    container_type read_queue_;
};

template<typename T>
class mpsc_queue final {
    struct node {
        std::optional<T> value;
        std::atomic<node*> next_;
    };

public:
    mpsc_queue():
        head_(new node { std::nullopt, nullptr }),
        tail_(head_.load(std::memory_order_relaxed)) {}

    mpsc_queue(const mpsc_queue& t) = delete;
    mpsc_queue& operator=(const mpsc_queue& t) = delete;

    ~mpsc_queue() {
        while (pop().has_value())
            ;
        node* front = head_.load(std::memory_order_relaxed);
        delete front;
    }

    template<typename Value>
    size_t push_back(Value&& v) noexcept {
        auto new_node = new node { std::forward<Value>(v), nullptr };
        auto prev = head_.exchange(new_node, std::memory_order_acq_rel);
        prev->next_.store(new_node, std::memory_order_release);
        return size_.fetch_add(1, std::memory_order_acq_rel);
    }

    std::optional<T> pop() noexcept {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto next = tail->next_.load(std::memory_order_acquire);
        if (nullptr == next)
            return std::nullopt; //should check size
        delete tail;
        tail_.store(next, std::memory_order_relaxed);
        size_.fetch_sub(1, std::memory_order_release);
        return std::optional<T> { std::move(next->value) };
    }

    size_t size() const noexcept {
        return size_.load(std::memory_order_acquire);
    }

private:
    std::atomic<size_t> size_ = { 0 };
    std::atomic<node*> head_;
    std::atomic<node*> tail_;
};
} // namespace moon
