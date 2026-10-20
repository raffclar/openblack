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
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

/// Temporary files and folders that no other test program uses.
///
/// ctest runs many test programs at the same time. Other builds and other worktrees on the same computer also run their
/// tests at the same time, and they all share one temporary folder. A test that writes a file with a fixed name could
/// thus read or remove the file of another test. Each path here has the number of the process in its name.
namespace openblack::test
{

/// A path in the temporary folder that only this process uses: the name, then the number of the process
[[nodiscard]] inline std::filesystem::path UniqueTempPath(std::string_view name)
{
#if defined(_WIN32)
	const auto process = _getpid();
#else
	const auto process = getpid();
#endif
	std::string unique(name);
	for (auto& character : unique)
	{
		if (character == '/' || character == '\\')
		{
			character = '_';
		}
	}
	unique += "_" + std::to_string(process);
	return std::filesystem::temp_directory_path() / "openblack_tests" / unique;
}

/// An empty folder for one test, which the test removes when it ends. Without a name, the folder takes the name of the
/// test that runs.
///
/// A game writes the profiles, the help record and the saves into its profile folder. Without its own folder, a game
/// uses the folder in the working directory, which all the test programs share. Thus each test that makes a game gives
/// the game a folder of this kind.
class TestTempFolder
{
public:
	explicit TestTempFolder(std::string_view name = {})
	    : _path(UniqueTempPath(name.empty() ? TestName() : std::string(name)))
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
		std::filesystem::create_directories(_path, error);
	}
	TestTempFolder(const TestTempFolder&) = delete;
	TestTempFolder& operator=(const TestTempFolder&) = delete;
	TestTempFolder(TestTempFolder&&) = delete;
	TestTempFolder& operator=(TestTempFolder&&) = delete;
	~TestTempFolder()
	{
		std::error_code error;
		std::filesystem::remove_all(_path, error);
	}

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
	/// The suite and the name of the test that runs
	[[nodiscard]] static std::string TestName()
	{
		const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
		return info != nullptr ? std::string(info->test_suite_name()) + "_" + info->name() : std::string("test");
	}

	std::filesystem::path _path;
};

} // namespace openblack::test
