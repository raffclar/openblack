/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// debug_env::Variable: an environment variable read once, when it is made, and kept as a copy.

#include <cstdlib>

#include <string>

#include <gtest/gtest.h>

#include "Debug/DebugEnv.h"

namespace
{
constexpr const char* k_Name = "OPENBLACK_TEST_DEBUG_ENV_VARIABLE";

void Set(const char* value)
{
#if defined(_WIN32)
	_putenv_s(k_Name, value);
#else
	if (*value == '\0')
	{
		unsetenv(k_Name);
	}
	else
	{
		setenv(k_Name, value, 1);
	}
#endif
}
} // namespace

TEST(DebugEnvVariable, ReadsTheValueOnceAndKeepsACopy)
{
	Set("12,34");
	const openblack::debug_env::Variable variable(k_Name);
	ASSERT_NE(variable.Get(), nullptr);
	EXPECT_EQ(std::string(variable.Get()), "12,34");
	// a later change of the environment does not reach it
	Set("56");
	EXPECT_EQ(std::string(variable.Get()), "12,34");
	Set(""); // unset
	EXPECT_EQ(std::string(variable.Get()), "12,34");
}

TEST(DebugEnvVariable, AnUnsetVariableIsNull)
{
	Set(""); // unset
	ASSERT_EQ(std::getenv(k_Name), nullptr);
	const openblack::debug_env::Variable variable(k_Name);
	EXPECT_EQ(variable.Get(), nullptr);
}
