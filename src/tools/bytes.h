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

#ifndef TOOLS_BYTES_H
#define TOOLS_BYTES_H

#include <cstdint>
#include <ostream>
#include <istream>
#include <cmath>
#include <string>
#include <type_traits>
#include <algorithm>

// ====
// byte
// ====

using byte_t = uint8_t;

template<typename N>
byte_t to_byte(N n) {
    return (byte_t)(n & 0xff);
}

template<typename N>
byte_t to_data_byte(N n) {
    return (byte_t)(n & 0x7f);
}

template<typename N> // n is a byte-like number
bool is_msb_set(N n) {
    return n > 0x7f;
}

template<typename N> // n is a byte-like number
bool is_msb_cleared(N n) {
    return n < 0x80; // return !is_msb_set(n)
}

template<typename T>
struct byte_traits {

    template<typename ByteInputIterator>
    static T read_little_endian(ByteInputIterator first, ByteInputIterator last) {
        T value{};
        for ( ; first != last ; ++first)
            value = (value << 8) | *first;
        return value;
    }

    static T read_little_endian(std::istream& stream, size_t size = sizeof(T)) {
        T value{};
        for (size_t i=0 ; i < size ; i++)
            value = (value << 8) | stream.get();
        return value;
    }

    static void write_little_endian(T value, std::ostream& stream, size_t size = sizeof(T)) {
        for (size_t i=0 ; i < size ; i++)
            stream.put(to_byte(value >> 8*(size-i-1)));
    }

    static void write_big_endian(T value, std::ostream& stream, size_t size = sizeof(T)) {
        for (size_t i=0 ; i < size ; i++) {
            stream.put(to_byte(value));
            value >>= 8;
        }
    }

};

std::string byte_string(byte_t byte);

std::ostream& print_byte(std::ostream& stream, byte_t byte);

template<typename ByteInputIterator>
std::ostream& print_bytes(std::ostream& stream, ByteInputIterator first, ByteInputIterator last) {
    if (first != last) {
        print_byte(stream, *first);
        for (++first ; first != last ; ++first) {
            stream << ' ';
            print_byte(stream, *first);
        }
    }
    return stream;
}

// ==========
// algorithms
// ==========

/**
 * return true if both range are equal (like std::equal would do),
 * but if ranges have different sizes, it will also return true
 * if all remaining element match the predicate
 */

template<typename InpuIt1, typename InpuIt2, typename UnaryPredicate>
bool equal_padding(InpuIt1 first1, InpuIt1 last1, InpuIt2 first2, InpuIt2 last2, UnaryPredicate p) {
    auto its = std::mismatch(first1, last1, first2, last2);
    if (its.first == last1) return std::all_of(its.second, last2, p);
    if (its.second == last2) return std::all_of(its.first, last1, p);
    return false;
}

template<typename T>
constexpr std::enable_if_t<std::is_integral<T>::value, T> decay_value(double value) {
    return (T)std::round(value);
}

template<typename T>
constexpr std::enable_if_t<std::is_floating_point<T>::value, T> decay_value(double value) {
    return (T)value;
}

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr auto safe_div(T a, T b) {
    return (a >= 0 ? a : (a - b + 1)) / b; // same as: (a - safe_modulo(a, b)) / b
}

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr auto safe_modulo(T a, T b) {
    return ((a % b) + b) % b; // same as: a - b*safe_div(a, b)
}

// =====
// range
// =====

namespace range_ns {

template <typename T, typename = void>
struct range_t {

    using value_type = T;

    constexpr explicit operator bool() const {
        return !(min == max);
    }

    T min;
    T max;

};

template <typename T>
struct range_t<T, typename std::enable_if_t<!std::is_same<typename std::iterator_traits<T>::value_type, void>::value>> {

    using iterator_category = typename std::iterator_traits<T>::iterator_category;
    using value_type = typename std::iterator_traits<T>::value_type;
    using difference_type = typename std::iterator_traits<T>::difference_type;
    using pointer = typename std::iterator_traits<T>::pointer;
    using reference = typename std::iterator_traits<T>::reference;

    constexpr explicit operator bool() const {
        return !(min == max);
    }

    constexpr auto begin() const { return min; }
    constexpr auto end() const { return max; }

    T min;
    T max;

};

template<typename T>
struct exp_range_t {

    using value_type = T;

    constexpr double factor() const {
        return 2. * std::log((range.max - pivot) / (pivot - range.min));
    }

    constexpr auto lifted() const {
        return range_t<double>{1., std::exp(factor())};
    }

    constexpr explicit operator bool() const {
        return range;
    }

    range_t<T> range;
    T pivot; /*!< value that will be at 50% on the scale */

};

template<typename IRangeT, typename IValueT, typename ORangeT>
constexpr auto rescale(const IRangeT& irange, IValueT value, const ORangeT& orange) {
    return expand(reduce(irange, value), orange);
}

template<typename T>
constexpr auto span(const range_t<T>& range) {
    return range.max - range.min;
}

template<typename T>
constexpr auto reduce(const range_t<T>& range, T value) {
    return static_cast<double>(value - range.min) / span(range);
}

template<typename T>
constexpr auto expand(double value, const range_t<T>& range) {
    /// @todo get min out of the double operation
    return decay_value<T>(range.min + value * span(range));
}

template<typename T>
constexpr auto span(const exp_range_t<T>& range) {
    return span(range.range);
}

template<typename T>
constexpr auto reduce(const exp_range_t<T>& range, T value) {
    return std::log(rescale(range.range, value, range.lifted())) / range.factor();
}

template<typename T>
constexpr auto expand(double value, const exp_range_t<T>& range) {
    return rescale(range.lifted(), std::exp(range.factor() * value), range.range);
}

template<typename T>
constexpr auto from_bounds(T min, T max) {
    return range_t<T>{min, max};
}

template<typename T, typename U>
constexpr auto from_span(T min, U span) {
    return range_t<T>{min, min + span};
}

}

using range_ns::range_t;
using range_ns::exp_range_t;

// ===========
// marshalling
// ===========

template<typename T> struct marshalling_traits { auto operator()(T value) { return std::to_string(value); } } ;

template<typename T> struct unmarshalling_traits;
template<> struct unmarshalling_traits<int> { auto operator()(const std::string& string) { return std::stoi(string); } } ;
template<> struct unmarshalling_traits<long> { auto operator()(const std::string& string) { return std::stol(string); } } ;
template<> struct unmarshalling_traits<unsigned long> { auto operator()(const std::string& string) { return std::stoul(string); } } ;
template<> struct unmarshalling_traits<long long> { auto operator()(const std::string& string) { return std::stoll(string); } } ;
template<> struct unmarshalling_traits<unsigned long long> { auto operator()(const std::string& string) { return std::stoull(string); } } ;
template<> struct unmarshalling_traits<float> { auto operator()(const std::string& string) { return std::stof(string); } } ;
template<> struct unmarshalling_traits<double> { auto operator()(const std::string& string) { return std::stod(string); } } ;
template<> struct unmarshalling_traits<long double> { auto operator()(const std::string& string) { return std::stold(string); } } ;
template<> struct unmarshalling_traits<bool> : unmarshalling_traits<long> {};
template<> struct unmarshalling_traits<int8_t> : unmarshalling_traits<long> {};
template<> struct unmarshalling_traits<uint8_t> : unmarshalling_traits<unsigned long> {};
template<> struct unmarshalling_traits<short> : unmarshalling_traits<long> {};
template<> struct unmarshalling_traits<unsigned short> : unmarshalling_traits<unsigned long> {};
template<> struct unmarshalling_traits<unsigned int> : unmarshalling_traits<unsigned long> {};

template<typename T> inline auto marshall(const T& value) { return std::string{marshalling_traits<T>()(value)}; }
template<typename T> inline auto unmarshall(const std::string& string) { return static_cast<T>(unmarshalling_traits<T>()(string)); }

// ===========
// accumulator
// ===========

template <typename ValueT>
struct accumulator_t;

template<typename T>
struct is_accumulator : std::false_type {};

template<typename ValueT>
struct is_accumulator<accumulator_t<ValueT>> : std::true_type {};

struct accumulator_type_tag {};
struct accumulator_any_tag {};

template <typename ValueT>
struct accumulator_t {

public:
    constexpr accumulator_t() = default;

    template<typename T, typename = std::enable_if_t<!is_accumulator<std::decay_t<T>>::value>>
    constexpr accumulator_t(T&& element, size_t count = 0) : m_value{std::forward<T>(element)}, m_count{count} {}

    constexpr auto value() const { return m_value; }
    constexpr auto count() const { return m_count; }

    template<typename T>
    constexpr auto as() const {
        return accumulator_t<T>{static_cast<T>(m_value), m_count};
    }

    constexpr auto average() const {
        return m_count ? static_cast<ValueT>(m_value / m_count) : m_value;
    }

    template<typename T>
    constexpr auto& operator+=(T&& element) {
        using tag_type = std::conditional_t<is_accumulator<std::decay_t<T>>::value, accumulator_type_tag, accumulator_any_tag>;
        add(tag_type{}, std::forward<T>(element));
        return *this;
    }

private:
    template<typename T>
    constexpr void add(accumulator_type_tag, T&& element) {
        m_value += element.value();
        m_count += element.count();
    }

    template<typename T>
    constexpr void add(accumulator_any_tag, T&& element) {
        m_value += std::forward<T>(element);
        ++m_count;
    }

    ValueT m_value {};
    size_t m_count {0};

};

#endif // TOOLS_BYTES_H
