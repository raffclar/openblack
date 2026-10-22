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
#include <string>
#include <typeinfo>
#include <vector>

#include <cxxopts.hpp>

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
#include <processenv.h>
// clang-format on
#endif

#include "Common/CrashHandler.h"
#include "Common/FatalMessage.h"
#include "Common/Platform.h"
#include "Common/SystemLibraries.h"
#include "Creature/BonusCreatures.h"
#include "EngineConfig.h"
#include "Game.h"
#include "LaunchSwitches.h"
#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#endif

namespace
{
/// The text of the whole command line, as the original game reads it
std::string CommandLineText(int argc, char** argv)
{
#ifdef _WIN32
	if (const char* line = GetCommandLineA(); line != nullptr)
	{
		return line;
	}
#endif
	std::string text;
	for (const auto* argument : std::span(argv, static_cast<size_t>(argc)))
	{
		if (!text.empty())
		{
			text += ' ';
		}
		text += argument;
	}
	return text;
}
} // namespace

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
		("s,start-level", "Level that is loaded at start-up: a script in Scripts, such as Land2.txt, or a playground such as Playgrounds/construct.txt, which starts without the story", cxxopts::value<std::string>()->default_value(std::string(openblack::k_DefaultStartLevel)))
		("testbed", "Start on the flat creature testbed, a plane with a lake, instead of a level.")
		("V,vsync", "Enable Vertical Sync.")
		("detail-level", "Graphics detail level of the original game: 0 Minimum, 1 Low, 2 Medium, 3 High, 4 Maximum (the default), 5 Custom, 6 Super.", cxxopts::value<uint16_t>()->default_value("4"))
		("m,window-mode", "Which mode to run window.", cxxopts::value<std::string>()->default_value("windowed"))
		("b,backend-type", "Which backend to use for rendering: Vulkan (VK), Direct3D12 (D3D12, DX12), Metal or Noop.", cxxopts::value<std::string>())
		("n,num-frames-to-simulate", "Number of frames to simulate before quitting.", cxxopts::value<uint32_t>()->default_value("0"))
		("l,log-file", "Output file for logs, 'stdout'/'logcat' for terminal output.", cxxopts::value<std::string>()->default_value(defaultLogFile))
		("L,log-level", "Level (trace, debug, info, warning, error, critical, off) of logging per subsystem (" + loggingSubsystems + ").",
		    cxxopts::value<std::vector<std::string>>()->default_value("all=debug"))
		("screenshot-frame", "Request a screenshot of the backbuffer at a certain frame number.", cxxopts::value<uint32_t>())
		("screenshot-path", "Path of the request a screenshot of the backbuffer.", cxxopts::value<std::filesystem::path>()->default_value("screenshot.png"))
		("frame-stats", "Log the average and 95th percentile frame time, the profiler stages and the memory in use every so many frames (0 for never), and each frame of 10 ms or more with its stages.", cxxopts::value<uint32_t>()->default_value("0"))
		("frame-stats-views", "With --frame-stats, also profile and log the GPU time of each render view.")
		("no-first-frame-warm-up", "Do not draw the first frame of a land behind the loading screen. Then the first frames of the game make the renderer's pipelines and render targets.")
		("unlock-bonus-creatures", "Debug: take the Creature Breeder's bonus creatures as unlocked, which the game only had once an unlock from outside it was installed: all, or some of leopard, horse, mandrill, gorilla and rhino joined by commas. None by default, as on a normal install.", cxxopts::value<std::string>())
		("no-draw", "Tools: open the window but draw nothing on the graphics card, so that a long run that the inspector drives does not wait on a busy graphics card. Screenshots are black.")
		("background", "Tools: keep the window out of the way of the desk: it never takes the focus, never comes to the front, never holds or moves the mouse, and it stays in a window (the display mode is ignored). Input from the inspector still works.")
		("dynamic-tree-shadows", "Trees cast their shadows as they are drawn each frame, swaying with them, rather than baked into the land as the game does.")
		("pre-intro", "Play the pre-intro film at start-up, as the game does on a first run.")
		("skip-logos", "Leave out the logo pictures at start-up.")
		("logo-effect", "For developers and tools: play the logo effect before the first game, as a plain start does. A developer's or a tool's run otherwise leaves it out.")
		("new-game", "Start a new game on the first land, as the game's own new game option does, rather than continue from the save on exit.")
		("continue", "For developers and tools: start as a plain start does, a run of a developer too. The save on exit of the profile loads, with the menu over it, or a new game starts when there is none. A developer's or a tool's run otherwise starts a new game.")
		("skip-opening", "For developers: start the new game with the start-of-game question answered, the story then going as it does for that answer: creature (straight to choosing a creature, the opening skipped), story (all of the first land's creature tutorial skipped too), old (keep the old creature; story when the profile has none), cow (debug: skip the creature choice, the Cow is the player's creature), normal; or ask, to be asked it as every new game is.", cxxopts::value<std::string>())
		("play-video", "Play a full-screen video once the land is loaded: intro, fall (the falling spell, which needs the player's creature) or a path such as Data/logo.bik.", cxxopts::value<std::string>()->default_value(""))
		("scenario", "Start on the testbed and run the testbed scenario of this id, such as benchmark.creatures_100.", cxxopts::value<std::string>())
		("benchmark-warmup", "With --scenario, the frames a benchmark's crowd settles for once spawned, before it is measured.", cxxopts::value<uint32_t>()->default_value("120"))
		("benchmark-frames", "With --scenario, the frames of a benchmark measured.", cxxopts::value<uint32_t>()->default_value("600"))
		("scenario-hide-window", "With --scenario, keep the testbed's window of scenarios closed, so the view is clear.")
		("benchmark-out", "With --scenario, where a benchmark writes its results (with .json and .csv after it); the game quits once they are written.", cxxopts::value<std::string>())
		("crash-dialogs", "Show the system's and C runtime's crash dialogs (Abort/Retry/Ignore) instead of writing a crash report to crashes/ and exiting.")
		("profile-folder", "For developers and tests: keep the players' profiles and their creatures in this folder rather than the Profiles folder in the working folder, as on a first start when it is empty. Without it, a developer's or a tool's run (a level, the testbed, a scenario, --skip-opening, --skip-logos, the inspector, a seed, a video, a number of frames or a screenshot asked for) plays as a profile of its own, kept apart, and never sees the new player's box.", cxxopts::value<std::string>())
		("seed", "Start every random number the game draws from this seed and pin the date the game reads (1 January 2001), so that two runs of the same scenario with a fixed frame time and the same input go the same way.", cxxopts::value<uint32_t>())
		("sim-fast", "Tools: the fast simulation mode. The game plays its frames one after the other as fast as the processor can and draws none of them (see --sim-draw-every). Each frame takes the fixed game time of --sim-frame-ms, 20 ms by default in this mode, so the turns are the same as at the normal speed.")
		("sim-frame-ms", "Tools: each frame takes this fixed game time in milliseconds (1 to 1000) from the first frame. Without --sim-fast, each frame also waits until it took this time in real time, so the run goes at the normal speed.", cxxopts::value<uint32_t>())
		("sim-draw-every", "Tools: with --sim-fast, draw one frame in this many (0, the default, for no frame).", cxxopts::value<uint32_t>()->default_value("0"))
		("sim-until-turn", "Tools: stop the game once it gets to this game turn.", cxxopts::value<uint32_t>())
	;
#if defined(OPENBLACK_INSPECTOR)
	options.add_options()
		("sim-record", "Tools: write the run record to this file, as JSON lines: the player's creature, the counts, the times and the problems that the stability checks find. Starts the inspector on a free port when --inspect-port is not given.", cxxopts::value<std::string>())
		("sim-record-every", "Tools: with --sim-record, write a record each time this many game turns go by.", cxxopts::value<uint32_t>()->default_value("100"))
		("sim-start-paused", "Tools: each land starts paused, before any of its frames, for a tool that steps the run through the inspector.")
		("sim-still-hand", "Tools: the hand stays where it starts and does not follow the pointer, so that runs at other frame rates get the same input from the hand.")
		("sim-hash-parts", "Tools: with --sim-record, also keep in each record the hash of the transforms of the entities of each component, to find what made two runs part.")
		("sim-limits", "Tools: with --sim-record, the JSON table of how long a plan or an activity can stay the same (tools/sim/stuck_limits.json).", cxxopts::value<std::string>())
		("inspect-port", "Start the debug inspector on this port of 127.0.0.1 (0 for any free port), for agents and tools to query and control the game. The player's mouse and keyboard are kept out while a client is connected; Ctrl+Alt+Shift+F12 takes the game back.", cxxopts::value<uint16_t>())
		("inspect-lock-input", "With --inspect-port, keep the player's mouse and keyboard out from the start, as for a headless scenario run.")
		("inspect-allow-player-input", "With --inspect-port, never keep the player's mouse and keyboard out.")
		("screenshot-root", "Where the inspector keeps pictures by feature, with their catalogue (default OPENBLACK_SCREENSHOT_ROOT, or E:/openblack/screenshots when E: is there).", cxxopts::value<std::string>())
		("fight-autopilot", "Start the fight autopilot, which fights for the player with the real mouse: lesson, aggressive, random or sequence. Starts the inspector on a free port when --inspect-port is not given.", cxxopts::value<std::string>())
		("fight-autopilot-seed", "The seed of the fight autopilot's random choices.", cxxopts::value<uint32_t>()->default_value("1"))
		("fight-autopilot-stage", "The stage of the fight lesson that the autopilot starts at: hit, hits, block, step or bout.", cxxopts::value<std::string>())
		("fight-autopilot-sequence", "The moves of the sequence policy, as seconds:move pairs divided by commas (\"1:high,2.5:block,4:step_back\").", cxxopts::value<std::string>())
		("fight-transcript-dir", "Write the transcript of each creature fight to this folder, as one file of JSON lines a fight. Starts the inspector on a free port when --inspect-port is not given.", cxxopts::value<std::string>());
#endif
	// clang-format on

	try
	{
		// The switches of the original game, such as -NEWGAME, are read from the whole command line. They are then taken
		// out, so that only openblack's own options are left for the parse.
		args.launchSwitches = openblack::launch_switches::Scan(CommandLineText(argc, argv));
		std::vector<char*> ownArguments;
		for (auto* argument : std::span(argv, static_cast<size_t>(argc)))
		{
			if (ownArguments.empty() || !openblack::launch_switches::IsSwitchArgument(argument))
			{
				ownArguments.push_back(argument);
			}
		}
		auto result = options.parse(static_cast<int>(ownArguments.size()), ownArguments.data());
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
		if constexpr (openblack::k_Platform == openblack::Platform::Web)
		{
			// A web browser has WebGL 2 only
			graphicsBackend = openblack::GraphicsBackend::OpenGLES;
		}

		// allow user to specify a renderer
		if (result.count("backend-type") != 0)
		{
			const auto name = result["backend-type"].as<std::string>();
			if (const auto backend = openblack::ParseGraphicsBackend(name))
			{
				graphicsBackend = *backend;
			}
			else
			{
				throw cxxopts::exceptions::parsing("Unknown backend '" + name +
				                                   "': use Vulkan (VK), Direct3D12 (D3D12, DX12), Metal or Noop");
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
#if defined(__EMSCRIPTEN__)
		if (result.count("game-path") == 0)
		{
			// In a web browser the game reads the folder that the page gives
			args.gamePath = std::string(openblack::web::k_GamePath);
		}
		else
#endif
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
		if (result.count("sim-fast") != 0 || result.count("sim-frame-ms") != 0 || result.count("sim-until-turn") != 0)
		{
			openblack::sim_run::Request simulation {};
			simulation.settings.fast = result["sim-fast"].as<bool>();
			simulation.settings.drawEvery = result["sim-draw-every"].as<uint32_t>();
			if (result.count("sim-frame-ms") != 0)
			{
				const auto ms = result["sim-frame-ms"].as<uint32_t>();
				if (ms == 0 || ms > openblack::sim_run::k_LongestFrameMs)
				{
					throw cxxopts::exceptions::parsing("--sim-frame-ms is a whole number of milliseconds from 1 to 1000");
				}
				simulation.frameMs = ms;
			}
			else if (simulation.settings.fast)
			{
				simulation.frameMs = openblack::sim_run::k_DefaultFastFrameMs;
			}
			simulation.settings.paceToWallClock = !simulation.settings.fast && simulation.frameMs.has_value();
			if (result.count("sim-until-turn") != 0)
			{
				simulation.settings.untilTurn = result["sim-until-turn"].as<uint32_t>();
			}
			simulation.settings.stillHand = result["sim-still-hand"].as<bool>();
			args.simulation = simulation;
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
		args.warmUpFirstFrame = result.count("no-first-frame-warm-up") == 0;
		args.dynamicTreeShadows = result.count("dynamic-tree-shadows") != 0;
		args.noDraw = result.count("no-draw") != 0;
		args.background = result.count("background") != 0;
		if (result.count("unlock-bonus-creatures") != 0)
		{
			const auto text = result["unlock-bonus-creatures"].as<std::string>();
			const auto unlocked = openblack::creature_swap::ParseUnlocked(text);
			if (!unlocked.has_value())
			{
				throw cxxopts::exceptions::incorrect_argument_type(text);
			}
			args.unlockedBonusCreatures = *unlocked;
		}
		args.playVideo = result["play-video"].as<std::string>();
		args.preIntro = result.count("pre-intro") != 0;
		args.skipLogos = result.count("skip-logos") != 0;
		args.logoEffect = result.count("logo-effect") != 0;
		args.newGame = result.count("new-game") != 0;
		args.continueGame = result.count("continue") != 0;
		if (result.count("profile-folder") != 0)
		{
			const auto folder = result["profile-folder"].as<std::string>();
			args.profileFolder = std::filesystem::path(std::u8string(folder.begin(), folder.end()));
		}
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
		if (result.count("fight-autopilot") != 0)
		{
			args.fightAutopilot = openblack::FightAutopilotRequest {
			    .policy = result["fight-autopilot"].as<std::string>(),
			    .seed = result["fight-autopilot-seed"].as<uint32_t>(),
			    .stage = result.count("fight-autopilot-stage") != 0 ? result["fight-autopilot-stage"].as<std::string>() : "",
			    .sequence =
			        result.count("fight-autopilot-sequence") != 0 ? result["fight-autopilot-sequence"].as<std::string>() : "",
			};
		}
		if (result.count("sim-record") != 0)
		{
			if (!args.simulation.has_value())
			{
				args.simulation = openblack::sim_run::Request {};
			}
			args.simulation->recordPath = std::filesystem::path(result["sim-record"].as<std::string>());
			args.simulation->recordEvery = std::max(1u, result["sim-record-every"].as<uint32_t>());
			args.simulation->hashParts = result["sim-hash-parts"].as<bool>();
			args.simulation->startPaused = result["sim-start-paused"].as<bool>();
			if (result.count("sim-limits") != 0)
			{
				args.simulation->limitsPath = std::filesystem::path(result["sim-limits"].as<std::string>());
			}
			if (!args.inspectPort.has_value())
			{
				args.inspectPort = 0;
			}
		}
		if (result.count("fight-transcript-dir") != 0)
		{
			args.fightTranscriptFolder = std::filesystem::path(result["fight-transcript-dir"].as<std::string>());
		}
		// The fight autopilot and the transcript's files need the inspector
		if (!args.inspectPort.has_value() && (args.fightAutopilot.has_value() || args.fightTranscriptFolder.has_value()))
		{
			args.inspectPort = 0;
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
	openblack::system_libraries::LoadBeforeDrivers();

	// clang-format off
	std::cout <<
	    "==============================================================================\n"
	    "   openblack - A modern reimplementation of Lionhead's Black & White (2001)   \n"
	    "==============================================================================\n"
	    "\n";
	// clang-format on

#if defined(__EMSCRIPTEN__)
	if (!openblack::web::PrepareFileSystem())
	{
		return EXIT_FAILURE;
	}
#endif

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
#if defined(__EMSCRIPTEN__)
		// In a web browser the game thread gives control back to the browser after each frame, so that the browser shows
		// the frame. Thus the frames run after main returns, and the program stops after the last frame.
		switch (game->StartRun())
		{
		case openblack::Game::RunStart::Failed:
			return EXIT_FAILURE;
		case openblack::Game::RunStart::Done:
			return EXIT_SUCCESS;
		case openblack::Game::RunStart::Play:
			break;
		}
		std::shared_ptr<openblack::Game> running(std::move(game));
		openblack::web::RunFrames([running]() { return running->RunFrame(); },
		                          [running]() {
			                          running->EndRun();
			                          return EXIT_SUCCESS;
		                          });
#else
		if (!game->Run())
		{
			return EXIT_FAILURE;
		}
#endif
	}
	catch (std::exception& e)
	{
		if (!crashDialogs)
		{
			openblack::crash_handler::ReportFatal(openblack::crash_report::CrashKind::UncaughtException, e.what(), {}, 0,
			                                      typeid(e).name());
		}
		std::cerr << e.what() << std::endl;
		openblack::ShowFatalMessage("Fatal error", e.what());
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
