#pragma once
#include <string_view>

class string_text_view_iterator_sentinel {};

template<typename StringTextView>
class string_text_view_iterator {
    std::string_view::iterator iter_;
    const StringTextView& stv_;

public:
    explicit string_text_view_iterator(std::string_view::iterator it, const StringTextView& stv):
        iter_(it),
        stv_(stv) {}

    std::string_view operator*() const {
        auto it = iter_;
        while (it != stv_.eof() && *it != stv_.delim()) {
            ++it;
        }
        return std::string_view { std::addressof(*iter_),
                                  static_cast<size_t>(std::distance(iter_, it)) };
    }

    string_text_view_iterator& operator++() {
        while (iter_ != stv_.eof() && *iter_ != stv_.delim()) {
            ++iter_;
        }

        if (iter_ != stv_.eof()) {
            ++iter_;
        }
        return *this;
    }

    string_text_view_iterator operator++(int) {
        auto old = iter_;
        while (iter_ != stv_.eof() && *iter_ != stv_.delim()) {
            ++iter_;
        }

        if (iter_ != stv_.eof()) {
            ++iter_;
        }
        return string_text_view_iterator { old, stv_ };
    }

    bool operator!=(const string_text_view_iterator_sentinel) const {
        return iter_ != stv_.eof();
    }
};

class string_text_view {
    std::string_view sw_;
    char delim_;

public:
    using const_iterator = string_text_view_iterator<string_text_view>;

    string_text_view(std::string_view sw, char delim): sw_(sw), delim_(delim) {}

    const_iterator begin() const {
        return const_iterator { sw_.begin(), *this };
    }

    string_text_view_iterator_sentinel end() const {
        return string_text_view_iterator_sentinel {};
    }

    std::string_view::const_iterator eof() const {
        return sw_.end();
    }

    char delim() const {
        return delim_;
    }
};
