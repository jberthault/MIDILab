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


template<typename T>
struct exp_range_t {

    range_t<T> range;
    T pivot; /*!< value that will be at 50% on the scale */

    constexpr double factor() const {
        return 2. * std::log((range.max - pivot) / (pivot - range.min));
    }

    constexpr range_t<double> expRange() const {
        return {1., std::exp(factor())};
    }

    constexpr explicit operator bool() const {
        return range;
    }

    constexpr double reduce(T value) const {
        return std::log(expRange().rescale(range, value)) / factor();
    }

    constexpr T expand(double value) const {
        return range.rescale(expRange(), std::exp(factor() * value));
    }

    template<typename U>
    constexpr T rescale(const range_t<U>& src, U value) const {
        return expand(src.reduce(value));
    }


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


template<typename T, bool = std::is_enum<T>::value, bool = std::is_integral<T>::value>
struct enum_decay {};

template<typename T>
struct enum_decay<T, true, false> {
    using type = std::underlying_type_t<T>;
};

template<typename T>
struct enum_decay<T, false, true> {
    using type = T;
};

template<typename T = size_t, typename N = size_t>
class flags_t {

public:

    using storage_type = std::enable_if_t<std::is_integral<T>::value, T>;
    using value_type = N;
    using base_type = typename enum_decay<N>::type;

    // ------------
    // constructors
    // ------------

    template<typename ... Values>
    static constexpr flags_t merge(Values&& ... values) { return from_integral(merge_integral(std::forward<Values>(values)...)); }

    constexpr flags_t() : m_storage(0) { }

    // ----------
    // conversion
    // ----------

    template<typename IntegralT>
    static constexpr flags_t from_integral(IntegralT integral) { return flags_t{static_cast<storage_type>(integral)}; }

    constexpr storage_type to_integral() const { return m_storage; }

    constexpr explicit operator bool() const { return m_storage; }

    // ----------
    // observers
    // ----------

    constexpr bool contains(value_type value) const { return (m_storage >> static_cast<base_type>(value)) & 1; }

    constexpr size_t size() const {
        size_t count = 0;
        for (storage_type storage = m_storage ; storage ; count++)
            storage &= storage - 1;
        return count;
    }

    // better names : intersects, is_superset, disjoints
    constexpr bool any(flags_t flags) const { return m_storage & flags.m_storage;} /*!< true if intersection is not null */
    constexpr bool all(flags_t flags) const { return (m_storage & flags.m_storage) == flags.m_storage; } /*!< true if this contains all the mask */
    constexpr bool none(flags_t flags) const { return !any(flags);} /*!< true if intersection is null */

    // --------
    // mutators
    // --------

    constexpr void commute(flags_t flags, bool on) {
        on ? m_storage |= flags.m_storage : m_storage &= ~flags.m_storage; // m_storage ^= ((-on ^ m_storage) & flags.m_storage);
    }

    constexpr void reset(value_type value) { m_storage &= ~merge_integral(value); }
    constexpr void set(value_type value) { m_storage |= merge_integral(value); }
    constexpr void flip(value_type value) { m_storage ^= merge_integral(value); }

    constexpr void clear() { m_storage = 0; }

    // ---------------
    // const operators
    // ---------------

    constexpr flags_t operator|(flags_t rhs) const { return from_integral(m_storage | rhs.m_storage); }
    constexpr flags_t operator&(flags_t rhs) const { return from_integral(m_storage & rhs.m_storage); }
    constexpr flags_t operator^(flags_t rhs) const { return from_integral(m_storage ^ rhs.m_storage); }
    constexpr flags_t operator~() const { return from_integral(~m_storage); }

    constexpr bool operator==(flags_t rhs) const { return m_storage == rhs.m_storage; }
    constexpr bool operator!=(flags_t rhs) const { return m_storage != rhs.m_storage; }
    constexpr bool operator<(flags_t rhs) const { return m_storage < rhs.m_storage; }
    constexpr bool operator<=(flags_t rhs) const { return m_storage <= rhs.m_storage; }
    constexpr bool operator>(flags_t rhs) const { return m_storage > rhs.m_storage; }
    constexpr bool operator>=(flags_t rhs) const { return m_storage >= rhs.m_storage; }

    // ------------------
    // mutable operators
    // ------------------

    constexpr void operator|=(flags_t rhs) { m_storage |= rhs.m_storage; }
    constexpr void operator&=(flags_t rhs) { m_storage &= rhs.m_storage; }
    constexpr void operator^=(flags_t rhs) { m_storage ^= rhs.m_storage; }

    // ---------
    // iterators
    // ---------

    struct const_iterator : public std::iterator<std::forward_iterator_tag, value_type> {

        const_iterator(storage_type storage = 0) : storage(storage), base(0) { adjust(); }

        bool operator==(const const_iterator& rhs) const { return storage == rhs.storage; }
        bool operator!=(const const_iterator& rhs) const { return storage != rhs.storage; }

        const_iterator& operator++() { shift(); adjust(); return *this; }
        const_iterator operator++(int) { const_iterator it(*this); operator++(); return it; }
        value_type operator*() const { return static_cast<value_type>(base); }

        void shift() { storage >>= 1; base++; }
        void adjust() { while (storage && !(storage & 1)) shift(); }

        storage_type storage;
        base_type base;

    };

    constexpr const_iterator begin() const { return {m_storage}; }
    constexpr const_iterator end() const { return {}; }

private:

    // ---------------
    // private details
    // ---------------

    static constexpr storage_type merge_integral() { return 0; }

    template<typename ... Values>
    static constexpr storage_type merge_integral(value_type value, Values&& ... values) { return (storage_type(1) << static_cast<base_type>(value)) | merge_integral(std::forward<Values>(values)...); }

    // --------------------
    // private constructors
    // --------------------

    constexpr explicit flags_t(storage_type set) : m_storage(set) { }

    //----------------
    // private members
    // ---------------

    storage_type m_storage;

};

template<typename T, typename N>
struct marshalling_traits<flags_t<T, N>> {
    auto operator()(flags_t<T, N> flags) {
        return marshall(flags.to_integral());
    }
};

template<typename T, typename N>
struct unmarshalling_traits<flags_t<T, N>> {
     auto operator()(const std::string& string) {
        return flags_t<T, N>::from_integral(unmarshall<T>(string));
    }
};

namespace std {
template <typename T, typename N>
struct hash<flags_t<T, N>> {
    auto operator()(flags_t<T, N> flags) const {
        return hash<T>()(flags.to_integral());
    }
};
}

#endif // TOOLS_BYTES_H
