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

#ifndef TOOLS_FLAGS_H
#define TOOLS_FLAGS_H

#include <cstdint>
#include <type_traits>

// =====
// flags
// =====

/**
 * enum_decay is a type trait giving the underlying type of an enum.
 * For integral types, the same type is returned.
 * It is not defined for other types
 */

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

/**
 * the integral holder maps a size given in bits to an integral type
 * capable of holding that many bits
 */

template<size_t Size> struct integral_holder;
template<> struct integral_holder<8> { using type = uint8_t; };
template<> struct integral_holder<16> { using type = uint16_t; };
template<> struct integral_holder<32> { using type = uint32_t; };
template<> struct integral_holder<64> { using type = uint64_t; };

template<size_t Size>
using integral_holder_t = typename integral_holder<Size>::type;

/**
 * flag tags may be used to differentiate the construction of flags
 */

struct flags_integral_value_tag{}; /*!< an integral value that can be casted to the actual storage type */
struct flags_homogeneous_values_tag{}; /*!< a set of pure values to set */
struct flags_heterogeneous_values_tag{}; /*!< a combination of pure values and flags to set */

/**
 * flags can be seen as iterable bitsets
 * type-safety is ensured using CRTP
 * No conversion is provided to enforce type-safety
 * enums are valid value types
 *
 */

template<typename DerivedT, typename ValueT, size_t Size>
struct flags_t {

    // -----
    // types
    // -----

    using derived_type = DerivedT;

    using value_type = ValueT;
    using value_base_type = typename enum_decay<ValueT>::type;

    static constexpr size_t capacity = Size;
    using storage_type = integral_holder_t<Size>;

    struct const_iterator : public std::iterator<std::forward_iterator_tag, value_type> {

        constexpr const_iterator(storage_type storage = {}) : storage{storage}, value_base{} { adjust(); }

        constexpr bool operator==(const const_iterator& rhs) const { return storage == rhs.storage; }
        constexpr bool operator!=(const const_iterator& rhs) const { return storage != rhs.storage; }

        constexpr auto& operator++() { shift(); adjust(); return *this; }
        constexpr auto operator++(int) { auto it{*this}; operator++(); return it; }
        constexpr auto operator*() const { return static_cast<value_type>(value_base); }

        constexpr void shift() { storage >>= 1; ++value_base; }
        constexpr void adjust() { while (storage && !(storage & 1)) shift(); }

        storage_type storage;
        value_base_type value_base;

    };

    // ---------------
    // storage helpers
    // ---------------

    static constexpr storage_type expand(value_type value) { return storage_type{1} << static_cast<value_base_type>(value); }
    static constexpr storage_type expand(flags_homogeneous_values_tag, value_type value) { return expand(value); }
    static constexpr storage_type expand(flags_heterogeneous_values_tag, value_type value) { return expand(value); }
    static constexpr storage_type expand(flags_heterogeneous_values_tag, derived_type flags) { return flags.to_integral(); }

    template<typename TagT>
    static constexpr storage_type assemble(TagT) { return {}; }

    template<typename TagT, typename T, typename ... Ts>
    static constexpr storage_type assemble(TagT tag, T&& value, Ts&& ... values) {
        return expand(tag, std::forward<T>(value)) | assemble(tag, std::forward<Ts>(values)...);
    }

    // --------------------
    // derived constructors
    // --------------------

    template<typename IntegralT>
    static constexpr auto from_integral(IntegralT storage) {
        return DerivedT{flags_integral_value_tag{}, storage};
    }

    template<typename ... Ts>
    static constexpr auto wrap(value_type value) {
        return DerivedT{flags_homogeneous_values_tag{}, value};
    }

    template<typename ... Ts>
    static constexpr auto from_values(Ts&& ... values) {
        return DerivedT{flags_homogeneous_values_tag{}, std::forward<Ts>(values)...};
    }

    template<typename ... Ts>
    static constexpr auto fuse(Ts&& ... values) {
        return DerivedT{flags_heterogeneous_values_tag{}, std::forward<Ts>(values)...};
    }

    // -----------------
    // base constructors
    // -----------------

    constexpr flags_t() : m_storage{} {}

    template<typename IntegralT, typename = std::enable_if_t<std::is_integral<IntegralT>::value>>
    constexpr explicit flags_t(flags_integral_value_tag, IntegralT storage) : m_storage{static_cast<storage_type>(storage)} {}

    template<typename ... Ts>
    constexpr explicit flags_t(flags_homogeneous_values_tag tag, Ts&& ... values) : m_storage{assemble(tag, std::forward<Ts>(values)...)} {}

    template<typename ... Ts>
    constexpr explicit flags_t(flags_heterogeneous_values_tag tag, Ts&& ... values) : m_storage{assemble(tag, std::forward<Ts>(values)...)} {}

    // ----------
    // conversion
    // ----------

    constexpr auto to_integral() const { return m_storage; }

    constexpr explicit operator bool() const { return to_integral(); }

    // ----------
    // observers
    // ----------

    constexpr bool test(value_type value) const { return (to_integral() >> static_cast<value_base_type>(value)) & 1; }

    constexpr auto size() const {
        size_t count{0};
        for (auto storage = to_integral() ; storage ; count++)
            storage &= storage - 1;
        return count;
    }

    // better names : intersects, is_superset, disjoints
    constexpr bool any(derived_type flags) const { return to_integral() & flags.to_integral(); } /*!< true if intersection is not null */
    constexpr bool all(derived_type flags) const { return (to_integral() & flags.to_integral()) == flags.to_integral(); } /*!< true if this contains all the mask */
    constexpr bool none(derived_type flags) const { return !any(flags); } /*!< true if intersection is null */

    // --------
    // mutators
    // --------

    constexpr void commute(derived_type flags, bool on) {
        on ? m_storage |= flags.to_integral() : m_storage &= ~flags.to_integral(); // m_storage ^= ((-on ^ m_storage) & flags.m_storage);
    }

    constexpr void reset(value_type value) { m_storage &= ~expand(value); }
    constexpr void set(value_type value) { m_storage |= expand(value); }
    constexpr void flip(value_type value) { m_storage ^= expand(value); }

    constexpr void clear() { m_storage = {}; }

    // --------------------
    // comparison operators
    // --------------------

    friend constexpr bool operator==(derived_type lhs, derived_type rhs) { return lhs.to_integral() == rhs.to_integral(); }
    friend constexpr bool operator!=(derived_type lhs, derived_type rhs) { return lhs.to_integral() != rhs.to_integral(); }
    friend constexpr bool operator<(derived_type lhs, derived_type rhs) { return lhs.to_integral() < rhs.to_integral(); }
    friend constexpr bool operator<=(derived_type lhs, derived_type rhs) { return lhs.to_integral() <= rhs.to_integral(); }
    friend constexpr bool operator>(derived_type lhs, derived_type rhs) { return lhs.to_integral() > rhs.to_integral(); }
    friend constexpr bool operator>=(derived_type lhs, derived_type rhs) { return lhs.to_integral() >= rhs.to_integral(); }

    // -----------------
    // bitwise operators
    // -----------------

    friend constexpr auto operator|(derived_type lhs, derived_type rhs) { return from_integral(lhs.to_integral() | rhs.to_integral()); }
    friend constexpr auto operator&(derived_type lhs, derived_type rhs) { return from_integral(lhs.to_integral() & rhs.to_integral()); }
    friend constexpr auto operator^(derived_type lhs, derived_type rhs) { return from_integral(lhs.to_integral() ^ rhs.to_integral()); }
    friend constexpr auto operator~(derived_type lhs) { return from_integral(~lhs.to_integral()); }

    constexpr void operator|=(derived_type rhs) { m_storage |= rhs.to_integral(); }
    constexpr void operator&=(derived_type rhs) { m_storage &= rhs.to_integral(); }
    constexpr void operator^=(derived_type rhs) { m_storage ^= rhs.to_integral(); }

    // ---------
    // iterators
    // ---------

    constexpr auto begin() const { return const_iterator{to_integral()}; }
    constexpr auto end() const { return const_iterator{}; }

private:

    // ---------------
    // private members
    // ---------------

    storage_type m_storage;

};

#endif // TOOLS_FLAGS_H
