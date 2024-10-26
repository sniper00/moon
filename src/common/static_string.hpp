#pragma once
#include <array>

namespace moon {
template<std::size_t N>
class static_string {
public:
    explicit static_string(std::string_view str) {
        std::copy_n(str.data(), std::min(N, str.size()), data_.begin());
    }

    char operator[](std::size_t index) const {
        return data_[index];
    }

    char& operator[](std::size_t index) {
        return data_[index];
    }

    std::size_t size() const {
        std::size_t i = 0;
        for (; i < N && data_[i] != '\0'; ++i)
            ;
        return i;
    }

    std::string_view to_string_view() const {
        return std::string_view(data_.data(), size());
    }

    const char* data() const {
        return data_.data();
    }

private:
    std::array<char, N + 1> data_ = {};
};
} // namespace moon
