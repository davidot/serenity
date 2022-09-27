/*
 * Copyright (c) 2022, David Tuin <davidot@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// I don't think this needs to stay so ignore this file

#include "AK/FloatingPointStringConversions.h"
#include "LibCore/File.h"
#include "LibCore/Stream.h"
#include "LibTest/TestRunnerUtil.h"
#include <AK/JsonParser.h>
#include <AK/LexicalPath.h>
#include <AK/QuickSort.h>
#include <LibCore/ArgsParser.h>
#include <LibMain/Main.h>

[[maybe_unused]] static double parse_via_strtod(String const& input)
{
    return strtod(input.characters(), nullptr);
};

[[maybe_unused]] static double parse_via_strof(String const& input)
{
    return strtof(input.characters(), nullptr);
};

[[maybe_unused]] static double json_parse_double(String const& input)
{
    JsonParser parser { input };
    auto error_or_result = parser.parse();
    if (error_or_result.is_error())
        return __builtin_nan("");
    return error_or_result.value().to_double(__builtin_nan(""));
};

[[maybe_unused]] static double new_parser(String const& input)
{
    return parse_double(input.view());
}

[[maybe_unused]] static float new_parserf(String const& input)
{
    return parse_float(input.view());
}

[[maybe_unused]] static double new_parser_float(String const& input)
{
    return parse_float(input.view());
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    StringView input_path {};
    int times = 1;
    bool check_bits = false;
    bool output_all_failures = false;
    char const* parser = "strtod";
    bool small = true;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("double parse speed test this probably doesn't belong in the repo");
    args_parser.add_positional_argument(input_path, "Path to test file", "path");
    args_parser.add_option(times, "How many times to parse", "times", 't', "amount");
    args_parser.add_option(check_bits, "Check against given bit patterns", "check", 'c');
    args_parser.add_option(output_all_failures, "Show all failures", "fails", 'f');
    args_parser.add_option(parser, "Parser to use strtod/json/....", "parser", 'p', "function");
    args_parser.parse(arguments);

    Vector<String> file_paths;

    if (!Core::File::is_directory(input_path)) {
        file_paths.append(input_path);
    } else {
        Test::iterate_directory_recursively(LexicalPath::canonicalized_path(input_path), [&](String const& file_path) {
            file_paths.append(file_path);
        });
        quick_sort(file_paths);
    }

    size_t total_failures = 0;
    size_t total_tests = 0;

    for (auto const& path : file_paths) {
        if (file_paths.size() > 0)
            outln("Loading file {}", path);

        Vector<String> lines;
        Vector<u64> expected_bits;
        {
            auto file = TRY(Core::Stream::File::open(path, Core::Stream::OpenMode::Read));
            auto buffer = TRY(file->read_all());
            StringView file_contents { buffer };
            auto lines_view = file_contents.lines();

            lines.ensure_capacity(lines_view.size());
            for (auto line_view : lines_view)
                lines.unchecked_append(line_view.substring_view(4 + 1 + 8 + 1 + 16 + 1));

            if (check_bits) {
                expected_bits.ensure_capacity(lines_view.size());
                for (auto line_view : lines_view) {
                    if (small) {
                        String bit_for_64_bit_double = line_view.substring_view(4 + 1, 8);
                        auto value = strtoull(bit_for_64_bit_double.characters(), nullptr, 16);
                        expected_bits.unchecked_append(value);
                    } else {
                        String bit_for_64_bit_double = line_view.substring_view(4 + 1 + 8 + 1, 16);
                        auto value = strtoull(bit_for_64_bit_double.characters(), nullptr, 16);
                        expected_bits.unchecked_append(value);
                    }
                }
            }
        }

#define string_to_double_function new_parserf

        if (check_bits) {
            // Just perform the check once
            size_t failures = 0;

            for (size_t i = 0; i < lines.size(); ++i) {
                u64 bit_value;
                if (small) {
                    float value = string_to_double_function(lines[i].characters());
                    bit_value = bit_cast<u32>(value);
                } else {
                    double value = string_to_double_function(lines[i].characters());
                    bit_value = bit_cast<u64>(value);
                }
                if (bit_value != expected_bits[i]) {
                    ++failures;
                    if (output_all_failures)
                        warnln("When parsing '{}' got [{:016x}] expected {} [{:016x}]", lines[i], bit_value, bit_cast<double>(expected_bits[i]), expected_bits[i]);
                    if (lines[i].length() < 22 && !lines[i].contains("e"sv, AK::CaseSensitivity::CaseInsensitive))
                        warnln("Short failure when parsing '{}' got [{:016x}] expected {} [{:016x}]", lines[i], bit_value, bit_cast<double>(expected_bits[i]), expected_bits[i]);
                }
            }

            outln("Checking values got {} failures {} out of {} tests", failures, failures > 0 ? ":^(" : ":^)", lines.size());
            total_failures += failures;
            total_tests += lines.size();
        }

        for (int i = 0; i < times; ++i) {
            size_t non_nan_values = 0;
            double max = -__builtin_huge_val();

            for (auto const& line : lines) {
                double value = string_to_double_function(line.characters());
                if (!__builtin_isnan(value)) {
                    ++non_nan_values;
                    if (value > max)
                        max = value;
                }
            }
            outln("From {}/{} non NaNs got max {}", non_nan_values, lines.size(), max);
        }
    }

    if (check_bits) {
        dbgln("got {} / {} total failures", total_failures, total_tests);
        outln("got {} / {} total failures", total_failures, total_tests);
        // strtod 3720645 / 5268191 -> 70% failures
        // json   4573817 / 5268191 -> 86% failures
        // new          0 / 5268191 :) 0% failures
        // total  5268191
    }

    return 0;
}
