/*
 * Copyright (c) 2022, David Tuin <davidot@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef KERNEL
#    error This file should not be included in the KERNEL as it deals with doubles \
           and there is no guraantee it won't do floating point computation.
#endif

#include <AK/StringView.h>

namespace AK {

static constexpr char floating_point_decimal_separator = '.';

enum class FloatingPointError {
    None,
    NoOrInvalidInput,
    OutOfRange,
    RoundedDownToZero
};

template<FloatingPoint T>
struct FloatingPointParseResults {
    char const* end_ptr { nullptr };
    FloatingPointError error = FloatingPointError::None;
    T value {};

    [[nodiscard]] bool parsed_value() const
    {
        // All other errors do indicate out of range but did produce a valid value.
        return error != FloatingPointError::NoOrInvalidInput;
    }
};

/// This function find the first floating point within [start, end). The accepted format is
/// intentionally as lenient as possible. If your format is stricter you must validate it
/// first. The format accepts:
/// - An optional sign, both + and - are supported
/// - 0 or more decimal digits, with leading zeros allowed [1]
/// - A decimal point '.', which can have no digits after it
/// - 0 or more decimal digits, unless the first digits [1] doesn't have any digits,
///   then this must have at least one
/// - An exponent 'e' or 'E' followed by an optional sign '+' or '-' and at least one digit
/// This function additionally detects out of range values which have been rounded to
/// [-]infinity or 0 and gives the next character to read after the floating point.
template<FloatingPoint T = double>
FloatingPointParseResults<T> parse_first_floating_point(char const* start, char const* end);

/// This function will return either a floating point, or an empty optional if the given StringView
/// does not a floating point or contains more characters beyond the floating point. For the format
/// check the comment on parse_first_floating_point.
template<FloatingPoint T = double>
Optional<T> parse_floating_point_completely(char const* start, char const* end);

/// This function find the first floating point as a hex float within [start, end).
/// The accepted format is intentionally as lenient as possible. If your format is
/// stricter you must validate it first. The format accepts:
/// - An optional sign, both + and - are supported
/// - Optionally either 0x or OX
/// - 0 or more hexadecimal digits, with leading zeros allowed [1]
/// - A decimal point '.', which can have no digits after it
/// - 0 or more hexadecimal digits, unless the first digits [1] doesn't have any digits,
///   then this must have at least one
/// - An exponent 'p' or 'P' followed by an optional sign '+' or '-' and at least one decimal digit
/// NOTE: The exponent is _not_ hexadecimal and gives powers of 2 not 16.
/// This function additionally detects out of range values which have been rounded to
/// [-]infinity or 0 and gives the next character to read after the floating point.
template<FloatingPoint T = double>
FloatingPointParseResults<T> parse_first_hexfloat(char const* start, char const* end);

}

using AK::parse_first_floating_point;
using AK::parse_first_hexfloat;
using AK::parse_floating_point_completely;
