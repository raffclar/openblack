/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The rules of the developer tools: the debug windows and the editor, the inspector, the testbed scenarios, the debug
/// keys, and the options of the command line that only developers and tools use. These rules keep no state, so that
/// the game and the tests use them both.
namespace openblack::developer_tools
{

/// Whether the program has the developer tools. The builds that players get do not have them (the CMake option
/// OPENBLACK_DEVELOPER_TOOLS).
#if defined(OPENBLACK_DEVELOPER_TOOLS)
inline constexpr bool k_Built = true;
#else
inline constexpr bool k_Built = false;
#endif

/// A developer action that can change the world, or show the state that the game keeps hidden from the player
enum class Action : uint8_t
{
	/// A debug window or the editor opens, shows or takes the input
	DebugWindow,
	/// The editor changes the world
	EditorWrite,
	/// An inspector query changes the game
	InspectorWrite,
	/// The testbed or a testbed scenario loads
	ScenarioLoad,
	/// A debug key does its work
	DebugKey,
};

/// The name of the action, for the log
[[nodiscard]] constexpr std::string_view NameOf(Action action) noexcept
{
	switch (action)
	{
	case Action::DebugWindow:
		return "debug window";
	case Action::EditorWrite:
		return "editor change";
	case Action::InspectorWrite:
		return "inspector change";
	case Action::ScenarioLoad:
		return "scenario load";
	case Action::DebugKey:
		return "debug key";
	}
	return "developer action";
}

/// Why a developer action cannot run, or none when it can. A build without the developer tools refuses all of them.
/// While a network session is live, every build refuses all of them, so that no developer tool changes a shared world.
[[nodiscard]] constexpr std::optional<std::string_view> RefusalOf(bool built, bool networkSessionLive) noexcept
{
	if (!built)
	{
		return "the build has no developer tools";
	}
	if (networkSessionLive)
	{
		return "a network game is live";
	}
	return std::nullopt;
}

/// Whether a developer action can run
[[nodiscard]] constexpr bool Allows(bool built, bool networkSessionLive) noexcept
{
	return !RefusalOf(built, networkSessionLive).has_value();
}

/// An option of the command line that only developers and tools use
struct DeveloperOption
{
	/// The long name, after "--"
	std::string_view name;
	/// The short name, after "-", or none
	char shortName {'\0'};
	/// The option takes a value. The value can follow in the next argument, or after a "=" in the same argument.
	bool takesValue {false};
};

/// The options of the command line that only developers and tools use. A build without the developer tools ignores
/// them. The window options (--background, -n), the logs, the lobby and the join options stay in every build.
/// --join-test-ca and --lobby-test-ca stay too: the game refuses them for all addresses but the loopback address of
/// this computer.
inline constexpr std::array k_DeveloperOptions {
    DeveloperOption {.name = "start-level", .shortName = 's', .takesValue = true},
    DeveloperOption {.name = "testbed"},
    DeveloperOption {.name = "gods-ring-map"},
    DeveloperOption {.name = "ui-scale", .shortName = 'u', .takesValue = true},
    DeveloperOption {.name = "screenshot-frame", .takesValue = true},
    DeveloperOption {.name = "screenshot-path", .takesValue = true},
    DeveloperOption {.name = "frame-stats", .takesValue = true},
    DeveloperOption {.name = "frame-stats-views"},
    DeveloperOption {.name = "no-first-frame-warm-up"},
    DeveloperOption {.name = "unlock-bonus-creatures", .takesValue = true},
    DeveloperOption {.name = "no-draw"},
    DeveloperOption {.name = "skip-logos"},
    DeveloperOption {.name = "logo-effect"},
    DeveloperOption {.name = "continue"},
    DeveloperOption {.name = "skip-opening", .takesValue = true},
    DeveloperOption {.name = "play-video", .takesValue = true},
    DeveloperOption {.name = "film-surface", .takesValue = true},
    DeveloperOption {.name = "scenario", .takesValue = true},
    DeveloperOption {.name = "benchmark-warmup", .takesValue = true},
    DeveloperOption {.name = "benchmark-frames", .takesValue = true},
    DeveloperOption {.name = "scenario-hide-window"},
    DeveloperOption {.name = "benchmark-out", .takesValue = true},
    DeveloperOption {.name = "profile-folder", .takesValue = true},
    DeveloperOption {.name = "seed", .takesValue = true},
    DeveloperOption {.name = "sim-fast"},
    DeveloperOption {.name = "sim-frame-ms", .takesValue = true},
    DeveloperOption {.name = "sim-draw-every", .takesValue = true},
    DeveloperOption {.name = "sim-until-turn", .takesValue = true},
    DeveloperOption {.name = "join-part-at", .takesValue = true},
    DeveloperOption {.name = "join-part-again"},
    DeveloperOption {.name = "join-creature", .takesValue = true},
    DeveloperOption {.name = "sim-network-game"},
    DeveloperOption {.name = "sim-still-hand"},
    DeveloperOption {.name = "sim-temple-loss", .takesValue = true},
    DeveloperOption {.name = "sim-leave-at", .takesValue = true},
    DeveloperOption {.name = "sim-checksums", .takesValue = true},
    DeveloperOption {.name = "sim-checksum-parts"},
    DeveloperOption {.name = "sim-checksum-fields"},
    DeveloperOption {.name = "sim-checksum-dump", .takesValue = true},
    DeveloperOption {.name = "sim-play-commands", .takesValue = true},
    DeveloperOption {.name = "sim-trace-draws", .takesValue = true},
    DeveloperOption {.name = "sim-row-dump", .takesValue = true},
    DeveloperOption {.name = "sim-row-dump-turn", .takesValue = true},
    DeveloperOption {.name = "sim-row-dump-keep", .takesValue = true},
    DeveloperOption {.name = "sim-record", .takesValue = true},
    DeveloperOption {.name = "sim-record-every", .takesValue = true},
    DeveloperOption {.name = "sim-start-paused"},
    DeveloperOption {.name = "sim-hash-parts"},
    DeveloperOption {.name = "sim-hash-fields"},
    DeveloperOption {.name = "sim-record-input", .takesValue = true},
    DeveloperOption {.name = "sim-input", .takesValue = true},
    DeveloperOption {.name = "sim-commands", .takesValue = true},
    DeveloperOption {.name = "sim-limits", .takesValue = true},
    DeveloperOption {.name = "sim-all-computer-gods"},
    DeveloperOption {.name = "sim-ai-seed", .takesValue = true},
    DeveloperOption {.name = "inspect-port", .takesValue = true},
    DeveloperOption {.name = "inspect-lock-input"},
    DeveloperOption {.name = "inspect-allow-player-input"},
    DeveloperOption {.name = "screenshot-root", .takesValue = true},
    DeveloperOption {.name = "fight-autopilot", .takesValue = true},
    DeveloperOption {.name = "fight-autopilot-seed", .takesValue = true},
    DeveloperOption {.name = "fight-autopilot-stage", .takesValue = true},
    DeveloperOption {.name = "fight-autopilot-sequence", .takesValue = true},
    DeveloperOption {.name = "fight-transcript-dir", .takesValue = true},
};

/// The options of the dedicated server that only developers and tools use. A build without the developer tools ignores
/// them. The server then keeps the options that a deployed game server uses: the game folder, the land, the socket and
/// its certificate, the lobby, the server id, the ticket keys, the room, the gods, the profile folder and the log.
inline constexpr std::array k_ServerDeveloperOptions {
    DeveloperOption {.name = "testbed"},
    DeveloperOption {.name = "scenario", .takesValue = true},
    DeveloperOption {.name = "skip-opening", .takesValue = true},
    DeveloperOption {.name = "seed", .takesValue = true},
    DeveloperOption {.name = "turns", .shortName = 'n', .takesValue = true},
    DeveloperOption {.name = "frame-ms", .takesValue = true},
    DeveloperOption {.name = "fast"},
    DeveloperOption {.name = "record", .takesValue = true},
    DeveloperOption {.name = "record-fields"},
    DeveloperOption {.name = "trace-draws", .takesValue = true},
    DeveloperOption {.name = "row-dump", .takesValue = true},
    DeveloperOption {.name = "row-dump-turn", .takesValue = true},
    DeveloperOption {.name = "row-dump-keep", .takesValue = true},
    DeveloperOption {.name = "commands", .takesValue = true},
    DeveloperOption {.name = "reload-at", .takesValue = true},
    DeveloperOption {.name = "log-checksums"},
    DeveloperOption {.name = "all-computer-gods"},
    DeveloperOption {.name = "ai-seed", .takesValue = true},
};

/// The tool modes of the AI soak: long games in which computer gods play every god. They exist only in builds with the
/// developer tools, in the game and in the dedicated server. A build that players get refuses a game that asks for them.
struct SoakTools
{
	/// Every god with a citadel is a computer god, also the god of the first slot, and the room has no human god
	bool allComputerGods {false};
	/// The seed of the personalities of the computer gods. None: each god keeps its built-in personality.
	std::optional<uint32_t> aiSeed;

	[[nodiscard]] constexpr bool Any() const noexcept { return allComputerGods || aiSeed.has_value(); }
	[[nodiscard]] constexpr bool operator==(const SoakTools&) const noexcept = default;
};

/// Why a build cannot play a game that uses the soak tools, or none when it can. A build without the developer tools
/// refuses every such game, also a game that a game server asks for.
[[nodiscard]] constexpr std::optional<std::string_view> SoakRefusalOf(bool built, const SoakTools& tools) noexcept
{
	if (!built && tools.Any())
	{
		return "the build has no developer tools, so it cannot play a game of computer gods only";
	}
	return std::nullopt;
}

/// Why a build cannot play a network game in which the game server plays a testbed scenario, or none when it can. Only a
/// build with the developer tools has the testbed scenarios.
[[nodiscard]] constexpr std::optional<std::string_view> ScenarioRefusalOf(bool built) noexcept
{
	if (!built)
	{
		return "the build has no developer tools, so it cannot play a testbed scenario";
	}
	return std::nullopt;
}

/// Whether an option of the command line, by its long name, is one of the developer options
[[nodiscard]] bool IsDeveloperOption(std::string_view name, std::span<const DeveloperOption> options) noexcept;

/// The arguments of a command line, divided into the arguments that stay and the developer options that go
struct FilteredArguments
{
	/// The indices of the arguments that stay, in their order. The first argument (the program) always stays.
	std::vector<size_t> kept;
	/// The developer options that go, as the command line gave them, with their values
	std::vector<std::string> ignored;
};

/// Divides the arguments of a command line. A developer option goes with its value: the value after a "=", or the next
/// argument when the option takes a value. A short option can have its value directly after the letter ("-sLand2.txt").
/// The arguments after a "--" argument stay as they are.
[[nodiscard]] FilteredArguments FilterDeveloperOptions(std::span<const std::string_view> arguments,
                                                       std::span<const DeveloperOption> options);

} // namespace openblack::developer_tools
