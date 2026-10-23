/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dedicated server of openblack: it runs the world of one land with no window, at 10 turns a second, and logs the
// checksum of the synced world at the end of each turn. It has no network yet.

#include <csignal>
#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <typeinfo>

#include <cxxopts.hpp>

#include "Common/BuildVersion.h"
#include "Common/CrashHandler.h"
#include "Server/DedicatedServer.h"

namespace
{

/// The signal handler sets it, and the loop of the server stops at the end of its frame
std::atomic<bool> g_StopAsked {false};

extern "C" void StopOnSignal(int /*signal*/)
{
	g_StopAsked.store(true);
}

} // namespace

int main(int argc, char* argv[])
{
	// A crash writes a report to the crashes folder and ends the program. A server shows no dialog box.
	openblack::crash_handler::Install();

	cxxopts::Options options("openblack-server", "The dedicated server of openblack: the world of one land, with no window.");
	// clang-format off
	options.add_options()
		("g,game-path", "The folder of the game's data.", cxxopts::value<std::string>())
		("s,start-level", "The script of the land under the game's Scripts folder.", cxxopts::value<std::string>()->default_value(std::string(openblack::k_DefaultStartLevel)))
		("testbed", "The flat testbed in place of a land.")
		("scenario", "Tools: start on the testbed and play the testbed scenario of this id, as the game's --scenario.", cxxopts::value<std::string>())
		("skip-opening", "How the new game on the first land starts, as the game's --skip-opening.", cxxopts::value<std::string>()->default_value("cow"))
		("seed", "The seed of every random number of the world.", cxxopts::value<uint32_t>()->default_value("1"))
		("n,turns", "Stop when the world gets to this turn. With none, the server runs until a signal stops it.", cxxopts::value<uint32_t>())
		("frame-ms", "The game time of each frame in milliseconds. A turn is 100 ms.", cxxopts::value<double>()->default_value("100"))
		("fast", "Tools: play the frames one after the other with no wait for the wall clock.")
		("profile-folder", "The folder of the server's own profile. With none, the profile of a tool's run in the working folder.", cxxopts::value<std::string>())
		("record", "Tools: write the run record of each turn to this file, with the hash of each component.", cxxopts::value<std::string>())
		("record-fields", "Tools: with --record, the hash of each field of each component too.")
		("commands", "Tools: do the commands of each turn that a game wrote with --sim-commands, at the same turns.", cxxopts::value<std::string>())
		("l,log-file", "The file of the log, or stdout.", cxxopts::value<std::string>()->default_value("stdout"))
		("version", "Write the version of openblack (the semantic version and the git commit) and exit.")
		("h,help", "Show this help.");
	// clang-format on

	openblack::server::ServerSettings settings;
	try
	{
		const auto result = options.parse(argc, argv);
		if (result.count("help") != 0)
		{
			std::cout << options.help() << std::endl;
			return EXIT_SUCCESS;
		}
		if (result.count("version") != 0)
		{
			std::cout << "openblack-server " << openblack::build_version::Text() << std::endl;
			return EXIT_SUCCESS;
		}
		if (result.count("game-path") == 0)
		{
			std::cerr << "The server needs the folder of the game's data: use -g." << std::endl;
			return EXIT_FAILURE;
		}
		settings.gamePath = result["game-path"].as<std::string>();
		settings.land = result["start-level"].as<std::string>();
		settings.testbed = result.count("testbed") != 0;
		if (result.count("scenario") != 0)
		{
			settings.scenario = result["scenario"].as<std::string>();
			settings.testbed = true;
		}
		settings.opening = openblack::new_game_choice::ParseNewGameStart(result["skip-opening"].as<std::string>());
		settings.seed = result["seed"].as<uint32_t>();
		if (result.count("turns") != 0)
		{
			settings.turns = result["turns"].as<uint32_t>();
		}
		settings.frameMilliseconds = result["frame-ms"].as<double>();
		settings.fast = result.count("fast") != 0;
		if (result.count("profile-folder") != 0)
		{
			settings.profileFolder = std::filesystem::path(result["profile-folder"].as<std::string>());
		}
		settings.logFile = result["log-file"].as<std::string>();
		if (result.count("record") != 0)
		{
			settings.recordPath = std::filesystem::path(result["record"].as<std::string>());
			settings.recordFields = result.count("record-fields") != 0;
		}
		if (result.count("commands") != 0)
		{
			settings.commandsPath = std::filesystem::path(result["commands"].as<std::string>());
		}
		openblack::crash_handler::SetLogFile(settings.logFile);
	}
	catch (const cxxopts::exceptions::exception& error)
	{
		std::cerr << error.what() << std::endl << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	// Ctrl+C, the end of the console and the request of the system to end the program stop the server
	std::signal(SIGINT, StopOnSignal);
	std::signal(SIGTERM, StopOnSignal);
#if defined(SIGBREAK)
	std::signal(SIGBREAK, StopOnSignal);
#endif

	try
	{
		return openblack::server::Run(settings, g_StopAsked, {});
	}
	catch (const std::exception& error)
	{
		openblack::crash_handler::ReportFatal(openblack::crash_report::CrashKind::UncaughtException, error.what(), {}, 0,
		                                      typeid(error).name());
	}
}
