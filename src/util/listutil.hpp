#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace util {
    template<typename Iter, typename Compare>
    inline void insertion_sort(Iter first, Iter last, Compare compare) {
        for (Iter it = first; it != last; ++it) {
            std::rotate(
                std::upper_bound(first, it, *it, compare), it, std::next(it)
            );
        }
    }

    template <class T>
    inline bool contains(const std::vector<T>& vec, const T& value) {
        return std::find(vec.begin(), vec.end(), value) != vec.end();
    }

    template <class T>
    inline void concat(std::vector<T>& a, const std::vector<T>& b) {
        a.reserve(a.size() + b.size());
        a.insert(a.end(), b.begin(), b.end());
    }

    std::string to_string(const std::vector<std::string>& vec);
}
