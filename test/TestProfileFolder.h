/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

namespace openblack::test
{

/// An empty profile folder for one test, which the test removes when it ends.
///
/// A game writes the profiles, the help record and the saves into its profile folder. Without its own folder, a game
/// uses the folder in the working directory, which all the test programs share. ctest runs many test programs at the
/// same time, so one game could read a file while another game writes it. Thus each test has its own folder.
class TestProfileFolder
{
public:
	TestProfileFolder()
	    : _path(std::filesystem::temp_directory_path() / "openblack_tests" / Name())
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
		std::filesystem::create_directories(_path, error);
	}
	TestProfileFolder(const TestProfileFolder&) = delete;
	TestProfileFolder& operator=(const TestProfileFolder&) = delete;
	~TestProfileFolder()
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
	}

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
	/// The process number and the name of the test make the name of the folder
	[[nodiscard]] static std::string Name()
	{
#if defined(_WIN32)
		const auto process = _getpid();
#else
		const auto process = getpid();
#endif
		std::string name = std::to_string(process);
		if (const auto* info = ::testing::UnitTest::GetInstance()->current_test_info(); info != nullptr)
		{
			name += std::string("_") + info->test_suite_name() + "_" + info->name();
		}
		for (auto& character : name)
		{
			if (character == '/' || character == '\\')
			{
				character = '_';
			}
		}
		return name;
	}

	std::filesystem::path _path;
};

} // namespace openblack::test
