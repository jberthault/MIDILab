/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017-2018 Julien Berthault

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#ifndef TOOLS_CONTAINERS_H
#define TOOLS_CONTAINERS_H

#include <unordered_set>

template <typename ... Args>
class blacklist_t final {

public:
    blacklist_t(bool blacklist) : is_blacklist{blacklist} {

    }

    template<typename V>
    bool match(V&& key) const {
        return is_blacklist ^ (elements.count(std::forward<V>(key)) != 0);
    }

    std::unordered_set<Args...> elements;
    bool is_blacklist;

};

template<typename T, size_t N>
class vararray_t final {

public:
    auto* data() noexcept { return m_array.data(); }
    const auto* data() const noexcept { return m_array.data(); }

    constexpr auto empty() const noexcept { return m_size == 0; }
    constexpr auto size() const noexcept { return m_size; }
    constexpr auto max_size() const noexcept { return N; }

    constexpr void clear() noexcept { m_size = 0; }
    constexpr void resize(size_t size) noexcept { m_size = size; }

    const auto& operator[](size_t pos) const noexcept { return m_array[pos]; }
    auto& operator[](size_t pos) noexcept { return m_array[pos]; }

    template<typename U>
    void push_back(U&& value) {
        m_array[m_size++] = std::forward<U>(value);
    }

    auto begin() noexcept { return m_array.begin(); }
    auto end() noexcept { return m_array.begin() + m_size; }
    auto begin() const noexcept { return m_array.begin(); }
    auto end() const noexcept { return m_array.begin() + m_size; }

private:
    std::array<T, N> m_array;
    size_t m_size {0};

};

#endif // TOOLS_CONTAINERS_H

