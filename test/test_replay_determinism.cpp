/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// (openblack) The same game played twice must give the same state at the end of every turn (Debug/StateHash.h): a
// fixed frame clock (Debug/FixedClock.h), no window (Noop), the mock land. Each game runs in its own process (this test
// starts itself twice), so no state of a first game in the process (the CRT seed, PSys ids, statics) can leak into
// the second. Later the one-thread and two-thread runs of the engine are compared the same way.

#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <Debug/FixedClock.h>
#include <Debug/StateHash.h>
#include <EngineConfig.h>
#include <Game.h>
#include <Locator.h>
#include <gtest/gtest.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace
{
constexpr uint32_t k_FrameMs = 16;
constexpr uint32_t k_Frames = 600; // about 96 turns of 100 ms
constexpr const char* k_ChildVariable = "OPENBLACK_REPLAY_CHILD";

/// One game, its hashes written to `file` (the child process)
void PlayOnce(const std::string& file)
{
	static const auto mockGamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock";
	openblack::fixed_clock::Install(k_FrameMs);
	auto args = openblack::Arguments {
	    .graphicsBackend = openblack::GraphicsBackend::Noop,
	    .gamePath = mockGamePath.string(),
	    .numFramesToSimulate = k_Frames,
	    .logFile = "stdout",
	    .startLevel = "Land1.txt",
	};
	std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::warn);
	auto game = std::make_unique<openblack::Game>(std::move(args));
	ASSERT_TRUE(game->Initialize());
	// the state hash keeps its state in the game's services, so it is turned on once they exist
	openblack::state_hash::Enable(file);
	// Game::Update stops at once while config.running is false, and only an SDL event sets it (Game::ProcessEvents):
	// without a window none comes, so a headless run would end before its first turn
	openblack::Locator::config::value().running = true;
	ASSERT_TRUE(game->Run());
	game.reset();
	openblack::fixed_clock::Install(0);
}

std::filesystem::path ThisExecutable()
{
#if defined(_WIN32)
	std::wstring path(MAX_PATH, L'\0');
	const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
	path.resize(length);
	return path;
#else
	return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

/// This test's executable again, as the child that plays one game into `file`
int RunChild(const std::filesystem::path& file)
{
#if defined(_WIN32)
	_putenv_s(k_ChildVariable, file.string().c_str());
#else
	setenv(k_ChildVariable, file.string().c_str(), 1);
#endif
	const std::string command =
	    "\"\"" + ThisExecutable().string() + "\" --gtest_filter=ReplayDeterminism.sameGameInTwoProcesses\"";
	const int status = std::system(command.c_str());
#if defined(_WIN32)
	_putenv_s(k_ChildVariable, "");
#else
	unsetenv(k_ChildVariable);
#endif
	return status;
}

std::vector<std::string> Lines(const std::filesystem::path& file)
{
	std::vector<std::string> lines;
	std::ifstream in(file);
	for (std::string line; std::getline(in, line);)
	{
		lines.push_back(line);
	}
	return lines;
}
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReplayDeterminism, sameGameInTwoProcesses)
{
	if (const char* child = std::getenv(k_ChildVariable); child != nullptr && *child != '\0')
	{
		PlayOnce(child);
		return;
	}
	const auto dir = std::filesystem::path(TEST_BINARY_DIR) / "replay";
	std::filesystem::create_directories(dir);
	const auto first = dir / "first.txt";
	const auto second = dir / "second.txt";
	ASSERT_EQ(RunChild(first), 0);
	ASSERT_EQ(RunChild(second), 0);
	const auto a = Lines(first);
	const auto b = Lines(second);
	ASSERT_FALSE(a.empty());
	ASSERT_EQ(a.size(), b.size());
	for (size_t i = 0; i < a.size(); ++i)
	{
		// "turn <n> <total> random=.. crt=.. clock=.. pools=.. transform=..": the first line that differs names the parts
		ASSERT_EQ(a[i], b[i]) << "the two games part at line " << i;
	}
}
