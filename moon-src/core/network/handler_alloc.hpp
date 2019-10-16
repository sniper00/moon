//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

// Class to manage the memory to be used for handler-based custom allocation.
// It contains a single block of memory which may be returned for allocation
// requests. If the memory is in use when an allocation request is made, the
// allocator delegates allocation to the global heap.
class handler_allocator
{
public:
    handler_allocator()
        :storage_()
        , in_use_(false)
    {
    }

    handler_allocator(const handler_allocator&) = delete;
    handler_allocator& operator=(const handler_allocator&) = delete;

    void* allocate(std::size_t size)
    {
        if (!in_use_ && size < sizeof(storage_))
        {
            in_use_ = true;
            return &storage_;
        }
        else
        {
            return ::operator new(size);
        }
    }

    void deallocate(void* pointer)
    {
        if (pointer == &storage_)
        {
            in_use_ = false;
        }
        else
        {
            ::operator delete(pointer);
        }
    }

private:
    // Storage space used for handler-based custom memory allocation.
    typename std::aligned_storage<512>::type storage_;

    // Whether the handler-based custom allocation storage has been used.
    bool in_use_;
};

// Wrapper class template for handler objects to allow handler memory
// allocation to be customised. Calls to operator() are forwarded to the
// encapsulated handler.
template <typename Allocator, typename Handler>
class custom_alloc_handler
{
public:
    using this_type = custom_alloc_handler<Allocator, Handler>;

    custom_alloc_handler(Allocator& a, Handler&& h)
        : allocator_(a),
        handler_(std::forward<Handler>(h))
    {
    }

    template <typename ...Args>
    void operator()(Args&&... args)
    {
        handler_(std::forward<Args>(args)...);
    }

    friend void* asio_handler_allocate(std::size_t size,
        this_type* this_handler)
    {
        return this_handler->allocator_.allocate(size);
    }

    friend void asio_handler_deallocate(void* pointer, std::size_t /*size*/,
        this_type* this_handler)
    {
        this_handler->allocator_.deallocate(pointer);
    }

private:
    Allocator& allocator_;
    Handler handler_;
};

// Helper function to wrap a handler object to add custom allocation.
template <typename Allocator, typename Handler>
inline custom_alloc_handler<Allocator, std::decay_t<Handler>> make_custom_alloc_handler(
    Allocator& a, Handler&& handler)
{
    using handler_type = std::decay_t<Handler>;
    return custom_alloc_handler<Allocator, handler_type>(a, std::forward<Handler>(handler));
}
