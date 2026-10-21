/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <string>

#include <Common/SystemLibraries.h>
#include <gtest/gtest.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace system_libraries = openblack::system_libraries;

TEST(SystemLibraries, TheRandomNumberLibraryLoadsBeforeTheDrivers)
{
	EXPECT_TRUE(std::ranges::find(system_libraries::k_LoadFirst, L"bcryptprimitives.dll") !=
	            system_libraries::k_LoadFirst.end());
}

TEST(SystemLibraries, EachLibraryStaysLoadedAfterTheCall)
{
	system_libraries::LoadBeforeDrivers();
	// A second call is safe and keeps the libraries loaded
	system_libraries::LoadBeforeDrivers();
#ifdef _WIN32
	for (const auto name : system_libraries::k_LoadFirst)
	{
		EXPECT_NE(GetModuleHandleW(std::wstring(name).c_str()), nullptr);
	}
#endif
}
