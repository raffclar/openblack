/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The main of openblack_tests, the program that holds the tests of the game's library. ctest runs it once for each
// test source with --openblack-test-source=<source, relative to the test folder>, and it runs only the tests written
// in that source. Without the option it runs every test, as a test program with Google Test's own main would.

#include <cstdio>

#include <algorithm>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

constexpr std::string_view k_SourceOption = "--openblack-test-source=";

std::string Normalise(std::string_view path)
{
	std::string normalised(path);
	std::ranges::replace(normalised, '\\', '/');
	std::ranges::transform(normalised, normalised.begin(),
	                       [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
	return normalised;
}

/// A filter of the full names of the tests written in the source, e.g. "Suite.Test:Other.Test"
std::string FilterForSource(const std::string& source)
{
	const auto suffix = "/test/" + Normalise(source);
	std::string filter;
	const auto* unit = testing::UnitTest::GetInstance();
	for (int s = 0; s < unit->total_test_suite_count(); ++s)
	{
		const auto* suite = unit->GetTestSuite(s);
		for (int t = 0; t < suite->total_test_count(); ++t)
		{
			const auto* info = suite->GetTestInfo(t);
			const auto file = Normalise(info->file() != nullptr ? info->file() : "");
			if (file.ends_with(suffix))
			{
				filter += (filter.empty() ? "" : ":") + std::string(info->test_suite_name()) + "." + info->name();
			}
		}
	}
	return filter;
}

} // namespace

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	std::string source;
	for (int i = 1; i < argc; ++i)
	{
		const std::string_view argument(argv[i]);
		if (argument.starts_with(k_SourceOption))
		{
			source = argument.substr(k_SourceOption.size());
		}
	}
	if (!source.empty())
	{
		const auto filter = FilterForSource(source);
		if (filter.empty())
		{
			std::fprintf(stderr, "openblack_tests: no tests are written in %s\n", source.c_str());
			return 1;
		}
		if (GTEST_FLAG_GET(filter) != "*")
		{
			std::fprintf(stderr, "openblack_tests: --gtest_filter is ignored with --openblack-test-source\n");
		}
		GTEST_FLAG_SET(filter, filter);
	}
	return RUN_ALL_TESTS();
}
