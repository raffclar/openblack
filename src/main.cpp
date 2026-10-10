/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <typeinfo>

#include <SDL_messagebox.h>
#include <cxxopts.hpp>

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
// clang-format on
#endif

#include "Common/CrashHandler.h"
#include "EngineConfig.h"
#include "Game.h"

bool parseOptions(int argc, char** argv, openblack::Arguments& args, int& returnCode)
{
	cxxopts::Options options("openblack", "Open source reimplementation of the game Black & White (2001).");

	const std::string defaultLogFile =
#if defined(OPENBLACK_DEBUG) || defined(__EMSCRIPTEN__)
	    "stdout";
#else
	    "openblack.log";
#endif

	std::string loggingSubsystems = "all";
	for (const auto& system : openblack::k_LoggingSubsystemStrs)
	{
		loggingSubsystems += std::string(", ") + system.data();
	}

	// clang-format off
	options.add_options()
		("h,help", "Display this help message.")
		("g,game-path", "Path to the Data/ and Scripts/ directories of the original Black & White game. (Required)", cxxopts::value<std::string>())
		("W,width", "Window resolution in the x axis.", cxxopts::value<uint16_t>()->default_value("1280"))
		("H,height", "Window resolution in the y axis.", cxxopts::value<uint16_t>()->default_value("1024"))
		("u,ui-scale", "Scaling of the GUI", cxxopts::value<float>()->default_value("1.0"))
		("s,start-level", "Level that is loaded at start-up", cxxopts::value<std::string>()->default_value("Land1.txt"))
		("testbed", "Start on the flat creature testbed, a plane with a lake, instead of a level.")
		("V,vsync", "Enable Vertical Sync.")
		("detail-level", "Graphics detail level of the original game, 0 to 6 (4 by default, 5 custom, 6 the highest).", cxxopts::value<uint16_t>()->default_value("4"))
		("m,window-mode", "Which mode to run window.", cxxopts::value<std::string>()->default_value("windowed"))
		("b,backend-type", "Which backend to use for rendering.", cxxopts::value<std::string>())
		("n,num-frames-to-simulate", "Number of frames to simulate before quitting.", cxxopts::value<uint32_t>()->default_value("0"))
		("l,log-file", "Output file for logs, 'stdout'/'logcat' for terminal output.", cxxopts::value<std::string>()->default_value(defaultLogFile))
		("L,log-level", "Level (trace, debug, info, warning, error, critical, off) of logging per subsystem (" + loggingSubsystems + ").",
		    cxxopts::value<std::vector<std::string>>()->default_value("all=debug"))
		("screenshot-frame", "Request a screenshot of the backbuffer at a certain frame number.", cxxopts::value<uint32_t>())
		("screenshot-path", "Path of the request a screenshot of the backbuffer.", cxxopts::value<std::filesystem::path>()->default_value("screenshot.png"))
		("frame-stats", "Log the average and 95th percentile frame time and the profiler stages every so many frames (0 for never).", cxxopts::value<uint32_t>()->default_value("0"))
		("frame-stats-views", "With --frame-stats, also profile and log the GPU time of each render view.")
		("pre-intro", "Play the pre-intro film at start-up, as the game does on a first run.")
		("skip-logos", "Leave out the logo pictures at start-up.")
		("skip-opening", "For developers: start the new game with the start-of-game question answered, the story then going as it does for that answer: creature (straight to choosing a creature, the opening skipped), story (all of the first land's creature tutorial skipped too), old (keep the old creature), normal; or ask, to be asked it whatever the player profiles.", cxxopts::value<std::string>())
		("play-video", "Play a full-screen video once the land is loaded: intro, fall (the falling spell, which needs the player's creature) or a path such as Data/logo.bik.", cxxopts::value<std::string>()->default_value(""))
		("scenario", "Start on the testbed and run the testbed scenario of this id, such as benchmark.creatures_100.", cxxopts::value<std::string>())
		("benchmark-warmup", "With --scenario, the frames a benchmark's crowd settles for once spawned, before it is measured.", cxxopts::value<uint32_t>()->default_value("120"))
		("benchmark-frames", "With --scenario, the frames of a benchmark measured.", cxxopts::value<uint32_t>()->default_value("600"))
		("scenario-hide-window", "With --scenario, keep the testbed's window of scenarios closed, so the view is clear.")
		("benchmark-out", "With --scenario, where a benchmark writes its results (with .json and .csv after it); the game quits once they are written.", cxxopts::value<std::string>())
		("crash-dialogs", "Show the system's and C runtime's crash dialogs (Abort/Retry/Ignore) instead of writing a crash report to crashes/ and exiting.")
		("seed", "Start every random number the game draws from this seed and pin the date the game reads (1 January 2001), so that two runs of the same scenario with a fixed frame time and the same input go the same way.", cxxopts::value<uint32_t>())
	;
#if defined(OPENBLACK_INSPECTOR)
	options.add_options()
		("inspect-port", "Start the debug inspector on this port of 127.0.0.1 (0 for any free port), for agents and tools to query and control the game. The player's mouse and keyboard are kept out while a client is connected; Ctrl+Alt+Shift+F12 takes the game back.", cxxopts::value<uint16_t>())
		("inspect-lock-input", "With --inspect-port, keep the player's mouse and keyboard out from the start, as for a headless scenario run.")
		("inspect-allow-player-input", "With --inspect-port, never keep the player's mouse and keyboard out.")
		("screenshot-root", "Where the inspector keeps pictures by feature, with their catalogue (default OPENBLACK_SCREENSHOT_ROOT, or E:/openblack/screenshots when E: is there).", cxxopts::value<std::string>());
#endif
	// clang-format on

	try
	{
		auto result = options.parse(argc, argv);
		if (result["help"].as<bool>())
		{
			std::cout << options.help() << std::endl;
			returnCode = EXIT_SUCCESS;
			return false;
		}

		// pick a sane renderer based on the user os
		openblack::GraphicsBackend graphicsBackend;
#ifdef _APPLE_
		graphicsBackend = openblack::GraphicsBackend::Metal;
#else
		graphicsBackend = openblack::GraphicsBackend::Vulkan;
#endif

		// allow user to specify a renderer
		if (result.count("backend-type") != 0)
		{
			auto rendererIter = openblack::k_GraphicsBackendStringLookup.find(result["backend-type"].as<std::string>());
			if (rendererIter != openblack::k_GraphicsBackendStringLookup.cend())
			{
				graphicsBackend = rendererIter->second;
			}
			else
			{
				throw cxxopts::exceptions::no_such_option(result["backend-type"].as<std::string>());
			}
		}

		static const std::map<std::string_view, openblack::windowing::DisplayMode> displayModeLookup = {
		    std::pair {"windowed", openblack::windowing::DisplayMode::Windowed},
		    std::pair {"fullscreen", openblack::windowing::DisplayMode::Fullscreen},
		    std::pair {"borderless", openblack::windowing::DisplayMode::Borderless},
		};

		openblack::windowing::DisplayMode displayMode;
		auto displayModeIter = displayModeLookup.find(result["window-mode"].as<std::string>());
		if (displayModeIter != displayModeLookup.cend())
		{
			displayMode = displayModeIter->second;
		}
		else
		{
			throw cxxopts::exceptions::no_such_option(result["window-mode"].as<std::string>());
		}

		std::array<spdlog::level::level_enum, openblack::k_LoggingSubsystemStrs.size()> logLevels;
		{
			std::map<std::string, spdlog::level::level_enum> logLevelMap;
			logLevelMap.insert_or_assign("all", spdlog::level::debug);
			for (const auto& levelStr : result["log-level"].as<std::vector<std::string>>())
			{
				const auto delim = levelStr.find_first_of('=');
				const auto key = delim == std::string::npos ? "all" : levelStr.substr(0, delim);
				const auto value = spdlog::level::from_str(delim == std::string::npos ? levelStr : levelStr.substr(delim + 1));
				logLevelMap.insert_or_assign(key, value);
			}

			const auto all = logLevelMap["all"];
			for (auto& level : logLevels)
			{
				level = all;
			}
			// TODO (#749) use std::views::enumerate
			for (size_t i = 0; const auto& str : openblack::k_LoggingSubsystemStrs)
			{
				const auto iter = logLevelMap.find(str.data());
				if (iter != logLevelMap.cend())
				{
					logLevels.at(i) = iter->second;
				}
				++i;
			}
		}

		args.executablePath = argv[0];
		if (result.count("game-path") == 0)
		{
#ifdef _WIN32
			// if we're on windows we can find the install path
			DWORD dataLen = 0;
			LSTATUS status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
			                             RRF_RT_REG_SZ, nullptr, nullptr, &dataLen);
			if (status == ERROR_SUCCESS)
			{
				char* path = new char[dataLen];
				status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
				                     RRF_RT_REG_SZ, nullptr, path, &dataLen);

				args.gamePath = std::string(path);
			}
			else
#endif
			{
				throw cxxopts::exceptions::option_has_no_value("game-path");
			}
		}
		else
		{
			args.gamePath = result["game-path"].as<std::string>();
		}

		if (result.count("seed") != 0)
		{
			args.seed = result["seed"].as<uint32_t>();
		}

		if (result.count("screenshot-frame") != 0)
		{
			args.requestScreenshot = std::make_pair(result["screenshot-frame"].as<uint32_t>(),
			                                        result["screenshot-path"].as<std::filesystem::path>());
		}

		args.windowWidth = result["width"].as<uint16_t>();
		args.windowHeight = result["height"].as<uint16_t>();
		args.guiScale = result["ui-scale"].as<float>();
		args.vsync = result["vsync"].as<bool>();
		args.detailLevel = static_cast<uint8_t>(std::min<uint16_t>(result["detail-level"].as<uint16_t>(), 6));
		args.displayMode = displayMode;
		args.graphicsBackend = graphicsBackend;
		args.numFramesToSimulate = result["num-frames-to-simulate"].as<uint32_t>();
		args.logFile = result["log-file"].as<std::string>();
		args.logLevels = logLevels;
		args.startLevel = result["start-level"].as<std::string>();
		args.startTestbed = result.count("testbed") != 0;
		args.frameStatsInterval = result["frame-stats"].as<uint32_t>();
		args.frameStatsViews = result.count("frame-stats-views") != 0;
		args.playVideo = result["play-video"].as<std::string>();
		args.preIntro = result.count("pre-intro") != 0;
		args.skipLogos = result.count("skip-logos") != 0;
		if (result.count("skip-opening") != 0)
		{
			const auto text = result["skip-opening"].as<std::string>();
			args.newGameStart = openblack::new_game_choice::ParseNewGameStart(text);
			if (!args.newGameStart.has_value())
			{
				throw cxxopts::exceptions::incorrect_argument_type(text);
			}
		}
#if defined(OPENBLACK_INSPECTOR)
		if (result.count("inspect-port") != 0)
		{
			args.inspectPort = result["inspect-port"].as<uint16_t>();
		}
		if (result.count("screenshot-root") != 0)
		{
			args.screenshotRoot = std::filesystem::path(result["screenshot-root"].as<std::string>());
		}
		if (result.count("inspect-lock-input") != 0)
		{
			args.inspectInputLock = openblack::input::LockMode::Locked;
		}
		else if (result.count("inspect-allow-player-input") != 0)
		{
			args.inspectInputLock = openblack::input::LockMode::Unlocked;
		}
#endif
		if (result.count("scenario") != 0)
		{
			args.scenario = openblack::ScenarioRequest {
			    .id = result["scenario"].as<std::string>(),
			    .warmUpFrames = result["benchmark-warmup"].as<uint32_t>(),
			    .frames = std::max<uint32_t>(result["benchmark-frames"].as<uint32_t>(), 1),
			};
			args.scenario->hideWindow = result.count("scenario-hide-window") != 0;
			if (result.count("benchmark-out") != 0)
			{
				args.scenario->results = std::filesystem::path(result["benchmark-out"].as<std::string>());
			}
		}
	}
	catch (cxxopts::exceptions::parsing& err)
	{
		std::cerr << err.what() << std::endl;
		std::cerr << options.help() << std::endl;

		returnCode = EXIT_FAILURE;
		return false;
	}

	return true;
}

int main(int argc, char* argv[]) noexcept
{
	const bool crashDialogs = openblack::crash_handler::WantsCrashDialogs(std::span(argv, static_cast<size_t>(argc)));
	if (!crashDialogs)
	{
		openblack::crash_handler::Install();
	}

	// clang-format off
	std::cout <<
	    "==============================================================================\n"
	    "   openblack - A modern reimplementation of Lionhead's Black & White (2001)   \n"
	    "==============================================================================\n"
	    "\n";
	// clang-format on

	try
	{
		openblack::Arguments args;
		int returnCode = EXIT_FAILURE;
		if (!parseOptions(argc, argv, args, returnCode))
		{
			return returnCode;
		}
		openblack::crash_handler::SetLogFile(args.logFile);
		auto game = std::make_unique<openblack::Game>(std::move(args));
		if (!game->Initialize())
		{
			return EXIT_FAILURE;
		}
		if (!game->Run())
		{
			return EXIT_FAILURE;
		}
	}
	catch (std::exception& e)
	{
		if (!crashDialogs)
		{
			openblack::crash_handler::ReportFatal(openblack::crash_report::CrashKind::UncaughtException, e.what(), {}, 0,
			                                      typeid(e).name());
		}
		std::cerr << e.what() << std::endl;
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", e.what(), nullptr);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                   [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nShowCmd)
{
	return main(__argc, __argv);
}
#endif
