/*
 * Copyright (c) 2022, David Tuin <davidot@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

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

// FIXME: Do we want to default these to double?
template<FloatingPoint T = double>
FloatingPointParseResults<T> parse_first_floating_point(char const* start, char const* end);

template<FloatingPoint T = double>
FloatingPointParseResults<T> parse_first_hexfloat(char const* start, char const* end);

// This function will return either a floating point, or an empty optional if the given StringView does not a floating point or contains more
template<FloatingPoint T = double>
Optional<T> parse_floating_point_completely(StringView view);

}

using AK::parse_first_floating_point;
using AK::parse_first_hexfloat;
using AK::parse_floating_point_completely;
