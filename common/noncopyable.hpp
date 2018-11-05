#pragma once

namespace moon
{
    class noncopyable
    {
    public:
        noncopyable() {}
        ~noncopyable() {}

        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    };
}


