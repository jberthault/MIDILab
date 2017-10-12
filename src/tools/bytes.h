/*

MIDILab | A Versatile MIDI Controller
Copyright (C) 2017 Julien Berthault

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

// =======
// numeric
// =======

template<typename T>
constexpr std::enable_if_t<std::is_integral<T>::value, T> decay_value(double value) {
    return (T)std::round(value);
}

template<typename T>
constexpr std::enable_if_t<std::is_floating_point<T>::value, T> decay_value(double value) {
    return (T)value;
}

// =====
// range
// =====

template <typename T>
struct range_t {

    constexpr explicit operator bool() const {
        return min != max;
    }

    constexpr double reduce(T value) const {
        return (double)(value - min) / (max - min);
    }

    constexpr T expand(double value) const {
        return decay_value<T>(min + value * (max - min));
    }

    template<typename U>
    constexpr T rescale(const range_t<U>& src, U value) const {
        return expand(src.reduce(value));
    }

    T min;
    T max;

};

// ======
// string
// ======

std::string byte_string(byte_t byte);

template<typename ByteInputIterator>
std::ostream& print_bytes(std::ostream& stream, ByteInputIterator first, ByteInputIterator last) {
    if (first != last)
        stream << byte_string(*first++);
    for ( ; first != last ; ++first)
        stream << ' ' << byte_string(*first);
    return stream;
}

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
template<> struct unmarshalling_traits<int8_t> : unmarshalling_traits<long> {};
template<> struct unmarshalling_traits<uint8_t> : unmarshalling_traits<unsigned long> {};
template<> struct unmarshalling_traits<short> : unmarshalling_traits<long> {};
template<> struct unmarshalling_traits<unsigned short> : unmarshalling_traits<unsigned long> {};
template<> struct unmarshalling_traits<unsigned int> : unmarshalling_traits<unsigned long> {};

template<typename T> inline std::string marshall(T value) { return marshalling_traits<T>()(value); }
template<typename T> inline T unmarshall(const std::string& string) { return unmarshalling_traits<T>()(string); }

// =====
// flags
// =====

template<typename T, typename N = size_t,
         typename = std::enable_if_t<std::is_integral<T>::value>,
         typename = std::enable_if_t<std::is_integral<N>::value>
         >
struct flags_t {

    using mask_type = T;
    using bit_type = N; /*!< bit identifier */

    struct const_iterator : public std::iterator<std::forward_iterator_tag, bit_type> {

        const_iterator(mask_type mask) : mask(mask), current(0) { adjust(); }

        bool operator==(const const_iterator& rhs) const { return mask == rhs.mask; }
        bool operator!=(const const_iterator& rhs) const { return mask != rhs.mask; }

        const_iterator& operator++() { shift(); adjust(); return *this; }
        const_iterator operator++(int) { const_iterator it(*this); operator ++(); return it; }
        bit_type operator*() const { return current; }

        void shift() { mask >>= 1; current++; }
        void adjust() { while (mask && !(mask & 1)) shift(); }

        mask_type mask;
        bit_type current;

    };

    static constexpr flags_t from_bit(bit_type bit) { return mask_type(1) << bit; }

    constexpr flags_t(mask_type mask = 0) : m_mask(mask) { }

    constexpr operator mask_type() const { return m_mask; }
    operator mask_type&() { return m_mask; }

    void reset(bit_type bit) { m_mask &= ~from_bit(bit); }
    void set(bit_type bit) { m_mask |= from_bit(bit); }
    void flip(bit_type bit) { m_mask ^= from_bit(bit); }

    /**
      * Count bits set in the mask using Brian Kernighan's Algorithm
      *
      * Non-loop algorithms:
      * // 16 bits
      * n -= (n >> 1) & 0x5555;
      * n = (n & 0x3333) + ((n >> 2) & 0x3333);
      * n = (n + (n >> 4)) & 0x0f0f;
      * return (n * 0x0101) >> 8;
      * // 32 bits (_popcnt32)
      * n -= (n >> 1) & 0x55555555;
      * n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
      * n = (n + (n >> 4)) & 0x0f0f0f0f;
      * return (n * 0x01010101) >> 24;
      * // 64 bits
      * n -= (n >> 1) & 0x5555555555555555;
      * n = (n & 0x3333333333333333) + ((n >> 2) & 0x3333333333333333);
      * n = (n + (n >> 4)) & 0x0f0f0f0f0f0f0f0f;
      * return (n * 0x0101010101010101) >> 56;
      */

    size_t count() const {
        size_t count = 0;
        for (mask_type mask = m_mask ; mask ; count++)
            mask &= mask - 1;
        return count;
    }

    constexpr bool contains(bit_type bit) const { return (m_mask >> bit) & 1; }

    constexpr bool any(mask_type mask) const { return m_mask & mask;} /*!< true if intersection is not null */
    constexpr bool all(mask_type mask) const { return (m_mask & mask) == mask; } /*!< true if this contains all the mask */
    constexpr bool none(mask_type mask) const { return !any(mask);} /*!< true if intersection is null */

    constexpr flags_t alter(mask_type mask, bool on) const {
        return on ? m_mask | mask : m_mask & ~mask; // return m_mask ^ ((-on ^ m_mask) & mask);
    }

    constexpr const_iterator begin() const { return {m_mask}; }
    constexpr const_iterator end() const { return {0}; }

    mask_type m_mask;

};

template<typename T, typename N>
struct marshalling_traits<flags_t<T, N>> : public marshalling_traits<T> {};

template<typename T, typename N>
struct unmarshalling_traits<flags_t<T, N>> {
     auto operator()(const std::string& string) {
        return flags_t<T, N>{unmarshall<T>(string)};
    }
};

namespace std {
template <typename T, typename N>
struct hash<flags_t<T, N>> : public hash<T> {};
}

#endif // TOOLS_BYTES_H
