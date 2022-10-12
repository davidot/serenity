/*
 * Copyright (c) 2022, David Tuin <davidot@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/StringView.h>

namespace AK {

double parse_double(StringView view);
float parse_float(StringView view);

}

using AK::parse_double;
using AK::parse_float;
