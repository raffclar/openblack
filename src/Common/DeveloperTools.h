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

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Common/GameLanguage.h"

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

/// A right of the account of the player of this computer, as the editor right or the debugger right
enum class AccountRight : uint8_t
{
	/// No network game is live and no account is signed in, so the right is not known
	Unknown,
	/// The join ticket or the signed-in account has the right
	Granted,
	/// The join ticket or the signed-in account does not have the right
	Refused,
};

/// The editor right of the player of this computer. The editor right also gives the debug cheats of the editor.
using EditorRight = AccountRight;

/// The debugger right of the player of this computer. Only this right shows the debug menu bar and the debug windows
/// that the bar opens.
using DebuggerRight = AccountRight;

/// A right of the player of this computer. In a network game it is the claim of the join ticket. Outside a network
/// game it is the right of the signed-in account, or Unknown when no account is signed in.
[[nodiscard]] constexpr AccountRight AccountRightOf(bool networkSessionLive, bool ticketClaim,
                                                    std::optional<bool> accountMark) noexcept
{
	if (networkSessionLive)
	{
		return ticketClaim ? AccountRight::Granted : AccountRight::Refused;
	}
	if (!accountMark.has_value())
	{
		return AccountRight::Unknown;
	}
	return *accountMark ? AccountRight::Granted : AccountRight::Refused;
}

/// The editor right of the player of this computer, from the editor claim or the editor right of the account
[[nodiscard]] constexpr EditorRight EditorRightOf(bool networkSessionLive, bool ticketEditor,
                                                  std::optional<bool> accountEditor) noexcept
{
	return AccountRightOf(networkSessionLive, ticketEditor, accountEditor);
}

/// The debugger right of the player of this computer, from the debugger claim or the debugger mark of the account
[[nodiscard]] constexpr DebuggerRight DebuggerRightOf(bool networkSessionLive, bool ticketDebugger,
                                                      std::optional<bool> accountDebugger) noexcept
{
	return AccountRightOf(networkSessionLive, ticketDebugger, accountDebugger);
}

/// The name of the right, for the log and the inspector
[[nodiscard]] constexpr std::string_view NameOf(AccountRight right) noexcept
{
	switch (right)
	{
	case AccountRight::Unknown:
		return "unknown";
	case AccountRight::Granted:
		return "granted";
	case AccountRight::Refused:
		return "refused";
	}
	return "unknown";
}

/// Why the in-game editor cannot open or change the world, or none when it can. The rules are:
/// - In a network game, only a player with the editor claim opens the editor, in every build. The game server refuses
///   the edits of every other player.
/// - In a developer build outside a network game, the editor always opens.
/// - In a build that players get outside a network game, the editor opens when the signed-in account has the editor
///   right. When no account is signed in, the editor does not open.
[[nodiscard]] constexpr std::optional<std::string_view> EditorRefusalOf(bool built, bool networkSessionLive,
                                                                        EditorRight right) noexcept
{
	if (networkSessionLive)
	{
		if (right != EditorRight::Granted)
		{
			return "only a player with the editor claim opens the editor in a network game";
		}
		return std::nullopt;
	}
	if (built || right == EditorRight::Granted)
	{
		return std::nullopt;
	}
	if (right == EditorRight::Refused)
	{
		return "the signed-in account does not have the editor right";
	}
	return RefusalOf(built, false);
}

/// The line that the player sees when the game or the game server refuses the editor or a debug cheat (deviation 269).
/// The game's text files have no such line, so the words are English.
inline constexpr std::u16string_view k_EditorRefusedLine = u"Your account cannot use the editor or the cheats in this game.";

/// The refusal line of the editor in one language of the game
struct EditorRefusedLine
{
	GameLanguage language {GameLanguage::UkEnglish};
	std::u16string_view line;
};

/// The refusal line in each language that has its own words. To give a language its line, add a row with the words
/// from a person who speaks the language. A language with no row shows the English line.
inline constexpr std::array k_EditorRefusedLines = {
    EditorRefusedLine {.language = GameLanguage::UkEnglish, .line = k_EditorRefusedLine},
    EditorRefusedLine {.language = GameLanguage::UsEnglish, .line = k_EditorRefusedLine},
};

/// The refusal line in the language of the game, or the English line when the language has no row
[[nodiscard]] constexpr std::u16string_view EditorRefusedLineOf(GameLanguage language) noexcept
{
	const auto found = std::ranges::find(k_EditorRefusedLines, language, &EditorRefusedLine::language);
	return found != k_EditorRefusedLines.end() ? found->line : k_EditorRefusedLine;
}

/// Whether a refusal of the editor or of a debug cheat shows the refusal line to the player. The line shows when the
/// join ticket or the signed-in account does not have the editor right. A developer build outside a network game never
/// refuses. With no account signed in outside a network game, a refusal shows nothing, as before.
[[nodiscard]] constexpr bool ShowsRefusalLine(bool built, bool networkSessionLive, EditorRight right) noexcept
{
	if (networkSessionLive)
	{
		return right != EditorRight::Granted;
	}
	return !built && right == EditorRight::Refused;
}

/// What the debug windows show and take
enum class GuiMode : uint8_t
{
	/// The debug menu bar and the debug windows. The editor shows only when the editor can open (see ShowsWindow).
	Full,
	/// Only the editor. In a network game the editor sends its edits and the debug cheats as commands of the turns.
	EditorOnly,
	/// No window and no menu, and the windows take no input
	Dormant,
};

/// The name of what the debug windows show, for the log and the inspector
[[nodiscard]] constexpr std::string_view NameOf(GuiMode mode) noexcept
{
	switch (mode)
	{
	case GuiMode::Full:
		return "full";
	case GuiMode::EditorOnly:
		return "editor only";
	case GuiMode::Dormant:
		return "dormant";
	}
	return "dormant";
}

/// What the debug windows show. The debugger right decides the debug menu bar and its windows. The editor right decides
/// the editor. The rules are:
/// - A player with the debugger right gets the debug menu bar, in every build and also in a network game.
/// - A player without the debugger right gets only the editor when the editor can open (see EditorRefusalOf). Else
///   the player gets nothing.
/// - A developer build outside a network game always shows the debug menu bar, also for an account without the debugger
///   right (user, 2026-11-16: "always show it"). A build that players get keeps the rules above.
[[nodiscard]] constexpr GuiMode GuiModeOf(bool built, bool networkSessionLive, EditorRight editor,
                                          DebuggerRight debugger) noexcept
{
	if (debugger == DebuggerRight::Granted)
	{
		return GuiMode::Full;
	}
	if (built && !networkSessionLive)
	{
		return GuiMode::Full;
	}
	return EditorRefusalOf(built, networkSessionLive, editor).has_value() ? GuiMode::Dormant : GuiMode::EditorOnly;
}

/// Whether a debug window shows and takes events in the mode. The editor shows only when the editor can open. In a
/// network game another debug window shows only when it shows in a network game: it changes nothing of the shared
/// world, or it sends its changes as commands of the turns.
[[nodiscard]] constexpr bool ShowsWindow(GuiMode mode, bool networkSessionLive, bool editorWindow, bool editorAllowed,
                                         bool showsInNetworkGame = false) noexcept
{
	switch (mode)
	{
	case GuiMode::Full:
		return editorWindow ? editorAllowed : (!networkSessionLive || showsInNetworkGame);
	case GuiMode::EditorOnly:
		return editorWindow && editorAllowed;
	case GuiMode::Dormant:
		return false;
	}
	return false;
}

/// Whether the debug menu bar shows in this frame. The bar shows only in the Full mode, and only while the game's menu
/// is open (user, 2026-11-16: "debug bar only shows when escape main menu is showing"). When the game's menu did not
/// load, the bar shows always in the Full mode, because then the bar is the only menu.
[[nodiscard]] constexpr bool ShowsMenuBar(GuiMode mode, bool hasGameMenu, bool gameMenuOpen) noexcept
{
	return mode == GuiMode::Full && (!hasGameMenu || gameMenuOpen);
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
    DeveloperOption {.name = "device-restore-frame", .takesValue = true},
    DeveloperOption {.name = "device-loss-frame", .takesValue = true},
    DeveloperOption {.name = "frame-stats", .takesValue = true},
    DeveloperOption {.name = "frame-stats-views"},
    DeveloperOption {.name = "frame-stats-out", .takesValue = true},
    DeveloperOption {.name = "no-stage-times"},
    DeveloperOption {.name = "parallel-draw-encode"},
    DeveloperOption {.name = "ai-times", .takesValue = true},
    DeveloperOption {.name = "turn-step-times", .takesValue = true},
    DeveloperOption {.name = "no-first-frame-warm-up"},
    DeveloperOption {.name = "unlock-bonus-creatures", .takesValue = true},
    DeveloperOption {.name = "no-draw"},
    DeveloperOption {.name = "skip-logos"},
    DeveloperOption {.name = "tips-screenshot", .takesValue = true},
    DeveloperOption {.name = "logo-effect"},
    DeveloperOption {.name = "continue"},
    DeveloperOption {.name = "skip-opening", .takesValue = true},
    DeveloperOption {.name = "direct-start-creature", .takesValue = true},
    DeveloperOption {.name = "play-video", .takesValue = true},
    DeveloperOption {.name = "film-surface", .takesValue = true},
    DeveloperOption {.name = "scenario", .takesValue = true},
    DeveloperOption {.name = "benchmark-warmup", .takesValue = true},
    DeveloperOption {.name = "benchmark-frames", .takesValue = true},
    DeveloperOption {.name = "scenario-hide-window"},
    DeveloperOption {.name = "list-scenarios"},
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
    DeveloperOption {.name = "join-build-id", .takesValue = true},
    DeveloperOption {.name = "join-load-delay", .takesValue = true},
    DeveloperOption {.name = "map-piece-delay-ms", .takesValue = true},
    DeveloperOption {.name = "sim-network-game"},
    DeveloperOption {.name = "throw-lead-latency-ms", .takesValue = true},
    DeveloperOption {.name = "sim-account-editor", .takesValue = true},
    DeveloperOption {.name = "sim-account-debugger", .takesValue = true},
    DeveloperOption {.name = "sim-still-hand"},
    DeveloperOption {.name = "sim-no-hand-reports"},
    DeveloperOption {.name = "sim-temple-loss", .takesValue = true},
    DeveloperOption {.name = "sim-gods-need-no-temple"},
    DeveloperOption {.name = "sim-time-limit", .takesValue = true},
    DeveloperOption {.name = "sim-conditions", .takesValue = true},
    DeveloperOption {.name = "sim-leave-at", .takesValue = true},
    DeveloperOption {.name = "sim-info-box-back-at", .takesValue = true},
    DeveloperOption {.name = "sim-checksums", .takesValue = true},
    DeveloperOption {.name = "sim-checksum-parts"},
    DeveloperOption {.name = "sim-checksum-fields"},
    DeveloperOption {.name = "sim-checksum-dump", .takesValue = true},
    DeveloperOption {.name = "sim-play-commands", .takesValue = true},
    DeveloperOption {.name = "sim-trace-draws", .takesValue = true},
    DeveloperOption {.name = "sim-row-dump", .takesValue = true},
    DeveloperOption {.name = "sim-row-dump-turn", .takesValue = true},
    DeveloperOption {.name = "sim-row-dump-keep", .takesValue = true},
    DeveloperOption {.name = "sim-world-states", .takesValue = true},
    DeveloperOption {.name = "sim-world-state-turns", .takesValue = true},
    DeveloperOption {.name = "sentry-test-error"},
    DeveloperOption {.name = "sentry-test-trap"},
    DeveloperOption {.name = "turn-api-check", .takesValue = true},
    DeveloperOption {.name = "turn-beside-draw"},
    DeveloperOption {.name = "sim-record", .takesValue = true},
    DeveloperOption {.name = "sim-record-every", .takesValue = true},
    DeveloperOption {.name = "sim-record-fields-every", .takesValue = true},
    DeveloperOption {.name = "sim-start-paused"},
    DeveloperOption {.name = "sim-hash-parts"},
    DeveloperOption {.name = "sim-hash-fields"},
    DeveloperOption {.name = "sim-record-input", .takesValue = true},
    DeveloperOption {.name = "sim-input", .takesValue = true},
    DeveloperOption {.name = "sim-commands", .takesValue = true},
    DeveloperOption {.name = "sim-limits", .takesValue = true},
    DeveloperOption {.name = "sim-all-computer-gods"},
    DeveloperOption {.name = "sim-ai-seed", .takesValue = true},
    DeveloperOption {.name = "sim-world-seed", .takesValue = true},
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

/// The options of the game that need the inspector. A build with the developer tools but with no inspector, such as the
/// web build, cannot do them. --sim-commands is not here: it works with no inspector.
inline constexpr std::array k_InspectorOptions {
    std::string_view {"sim-record"},
    std::string_view {"sim-record-every"},
    std::string_view {"sim-record-fields-every"},
    std::string_view {"sim-start-paused"},
    std::string_view {"sim-hash-parts"},
    std::string_view {"sim-hash-fields"},
    std::string_view {"sim-record-input"},
    std::string_view {"sim-input"},
    std::string_view {"sim-limits"},
    std::string_view {"inspect-port"},
    std::string_view {"inspect-lock-input"},
    std::string_view {"inspect-allow-player-input"},
    std::string_view {"screenshot-root"},
    std::string_view {"fight-autopilot"},
    std::string_view {"fight-autopilot-seed"},
    std::string_view {"fight-autopilot-stage"},
    std::string_view {"fight-autopilot-sequence"},
    std::string_view {"fight-transcript-dir"},
};

/// The text of the error for a build with no inspector that gets options that need it. It names each option. The
/// result has no value when the list is empty.
[[nodiscard]] inline std::optional<std::string> InspectorOptionsError(std::span<const std::string> given)
{
	if (given.empty())
	{
		return std::nullopt;
	}
	std::string text = "This build has no inspector. It cannot do the option";
	text += given.size() == 1 ? " " : "s ";
	for (size_t i = 0; i < given.size(); ++i)
	{
		text += (i == 0 ? "--" : ", --") + given[i];
	}
	text += ". Use a desktop build that has the inspector.";
	return text;
}

/// The options of the dedicated server that only developers and tools use. A build without the developer tools ignores
/// them. The server then keeps the options that a deployed game server uses: the game folder, the land, the socket and
/// its certificate, the lobby, the server id, the ticket keys, the room, the gods, the winning conditions, the time
/// limit, the profile folder and the log.
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
    DeveloperOption {.name = "inspect-port", .takesValue = true},
    DeveloperOption {.name = "trace-draws", .takesValue = true},
    DeveloperOption {.name = "row-dump", .takesValue = true},
    DeveloperOption {.name = "row-dump-turn", .takesValue = true},
    DeveloperOption {.name = "row-dump-keep", .takesValue = true},
    DeveloperOption {.name = "world-states", .takesValue = true},
    DeveloperOption {.name = "world-state-turns", .takesValue = true},
    DeveloperOption {.name = "commands", .takesValue = true},
    DeveloperOption {.name = "reload-at", .takesValue = true},
    DeveloperOption {.name = "test-commands"},
    DeveloperOption {.name = "log-checksums"},
    DeveloperOption {.name = "all-computer-gods"},
    DeveloperOption {.name = "ai-seed", .takesValue = true},
    DeveloperOption {.name = "gods-need-no-temple"},
    DeveloperOption {.name = "temple-loss", .takesValue = true},
    DeveloperOption {.name = "lan-search", .takesValue = true},
    DeveloperOption {.name = "sentry-test-error"},
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
