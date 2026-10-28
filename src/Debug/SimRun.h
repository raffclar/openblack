/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/DeveloperTools.h"
#include "ECS/GameEndConditions.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Debug/RowDump.h"
#endif

/// The fast simulation mode for long tool runs, such as a creature's whole life. Each frame takes a fixed game time, so a
/// frame plays the same turns at any speed. In fast mode the game plays its frames one after the other as fast as the
/// processor can, and it draws none of them or only a few. Without the mode the game runs as usual.
namespace openblack::sim_run
{

/// How much of the synced world each record of a run keeps besides its hash
enum class HashDetail : uint8_t
{
	/// Only the hash
	None,
	/// The hash of each component
	Components,
	/// The hash of each component and of each field of each component
	Fields,
};

/// Tests: at a turn, the temple of a player starts its destruction, as when its heart loses the last of its life. Each
/// computer of a network game must have the same switch, so that every world does the same at the same turn.
struct TempleLoss
{
	/// The number of the player, from 0
	uint8_t player {0};
	uint32_t turn {0};
};

/// Tools: the winning conditions that a test switch gives a network game, as the host picks them. The values are checked
/// against the limits of a land in "Online Maps\ConditionTemplate.txt".
struct ConditionsRequest
{
	/// The land whose limits the values must keep to, such as "mpm_2p_1". Empty: the land of the game.
	std::string limitsLand;
	std::vector<game_end::ConditionPick> picks;
};

/// How the game runs its frames
struct Settings
{
	/// Play the frames one after the other and draw only some of them
	bool fast {false};
	/// In fast mode, draw one frame in this many (0 for no frame)
	uint32_t drawEvery {0};
	/// Without fast mode, and with a fixed frame time: wait at the end of each frame until the frame takes its game
	/// time in real time too, so that the run goes at the normal speed
	bool paceToWallClock {false};
	/// Stop the game once it gets to this turn
	std::optional<uint32_t> untilTurn;
	/// The hand stays where it starts and does not follow the pointer: the player gives no input with the hand. The
	/// frames ease the hand to the pointer, so a hand that moves gives other places at other frame rates.
	bool stillHand {false};
	/// Tests: the loss of a temple at a turn
	std::optional<TempleLoss> templeLoss;
	/// Tests: a god with no temple stays in a network game. For tool runs of a land that makes no temple at its start,
	/// such as a story land. Each computer of a network game must have the same switch.
	bool godsNeedNoTemple {false};
	/// Tests: the time limit of a network game in minutes, in place of the default time limit. The game server sends its
	/// time limit to the clients at the start.
	std::optional<uint32_t> timeLimit;
	/// Tools: the winning conditions of a network game, in place of the host's pick. The game server sends its
	/// conditions to the clients at the start.
	std::vector<game_end::ConditionPick> conditions;
	/// Tests: at this turn the player of a network game leaves it as a player does: Escape, Exit Online Game, Leave Game
	std::optional<uint32_t> leaveAt;
};

/// What the command line asks of a simulation run
struct Request
{
	Settings settings {};
	/// Tools: the modes of the AI soak for the first land. A game that joins a game server takes them from the server.
	developer_tools::SoakTools soak;
	/// The game time of each frame, none for the wall clock
	std::optional<std::chrono::microseconds> frameTime;
	/// The file to write the run record to, as JSON lines
	std::optional<std::filesystem::path> recordPath;
	/// The file to write the draw trace to: each draw on the synced seed, with its turn and the place of its call
	std::optional<std::filesystem::path> traceDrawsPath;
	/// Write a record each time this many turns go by
	uint32_t recordEvery {k_DefaultRecordEvery};
	/// The table of the longest time that a plan or an activity can stay the same, as JSON
	std::optional<std::filesystem::path> limitsPath;
	/// What the records keep of the hash of the parts of the world
	HashDetail hashParts {HashDetail::None};
	/// Each land starts paused, before any of its frames, for a tool that steps it through the inspector
	bool startPaused {false};
	/// The file of recorded input that the first land makes from its first frame on, as input.record answers it
	std::optional<std::filesystem::path> inputPath;
	/// Tools: the input made from the first frame of the first land on goes to this file as the game ends
	std::optional<std::filesystem::path> recordInputPath;
	/// The file to write the commands of each turn to, for a dedicated server that does the same commands (command_log)
	std::optional<std::filesystem::path> commandsPath;
	/// The file to write the checksum of each turn to (checksum_log), or "-" for the standard output
	std::optional<std::filesystem::path> checksumsPath;
	/// What the checksum of each turn keeps of the hash of the parts of the synced world
	HashDetail checksumParts {HashDetail::None};
	/// The turn at which the checksum also writes each field of each row of some components, and those components
	std::optional<std::pair<uint32_t, std::vector<std::string>>> checksumDump;
	/// The file of the commands of each turn (command_log) that the run does at their turns. The run then does no
	/// command of its own player: as a computer that only receives the commands of a network game.
	std::optional<std::filesystem::path> playCommandsPath;
	/// Tools: log each frame step that changes the synced world, with the parts (the components, or with Fields their
	/// fields) that it changes. None: no guard.
	HashDetail frameGuard {HashDetail::None};
	/// The guard starts to look at this turn, because a sample at each step is slow
	uint32_t frameGuardFrom {0};
	/// The frame write guard also looks at the saved state of each system, which is outside the registry
	bool frameGuardSystems {false};
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	/// Tools: the file of the row dump (row_dump::RowDump), and what it keeps and writes
	std::optional<std::filesystem::path> rowDumpPath;
	row_dump::Settings rowDump;
#endif

	static constexpr uint32_t k_DefaultRecordEvery = 100;
};

/// The game time of each frame in fast mode when the command line gives none: five frames a turn, near the 60 frames a
/// second of the screen's refresh
constexpr uint32_t k_DefaultFastFrameMs = 20;

/// The longest fixed frame time, which is the longest time the inspector also accepts
constexpr uint32_t k_LongestFrameMs = 1000;

/// The fixed frame time from a number of milliseconds with a part of a millisecond, to the nearest microsecond: none
/// when it is not from 1 to the longest frame time
[[nodiscard]] std::optional<std::chrono::microseconds> FrameTimeFromMs(double milliseconds);

/// Whether the game draws this frame. In the fast mode, the game also draws a frame when a tool asks for a picture of
/// it, because a frame that is not drawn gives no picture.
[[nodiscard]] bool DrawsFrame(const Settings& settings, uint64_t frame, bool pictureWanted = false);

/// Whether the run is at its end at this turn
[[nodiscard]] bool Finished(const Settings& settings, uint32_t turn);

/// Whether the game skips a frame of a run that a tool steps through the inspector. While the tool holds the run, the
/// game is paused and the frame only gives the answers to the tool. The wall clock sets the number of these frames.
/// Thus they must not move the clock, the camera or the hand, because the turns get the camera and the hand from the
/// reports. A frame with work that the tool waits for still plays, for example a scenario to lay out or a picture.
[[nodiscard]] bool SkipsHeldFrame(bool steppedByTool, bool paused, bool workWaits);

/// The loss of a temple from the text "player:turn", such as "1:300". None for another text or a player past the seventh.
[[nodiscard]] std::optional<TempleLoss> ParseTempleLoss(std::string_view text);

/// The winning conditions from the text "[LAND:]NAME=VALUE,...", such as "mpm_2p_1:WC_HOUSES_BUILT=3,WC_GROW_TREES=5".
/// A name is the name of the condition in the template file, or its number from 0. None for another text, or for a
/// condition that the text names two times.
[[nodiscard]] std::optional<ConditionsRequest> ParseConditions(std::string_view text);

/// How long the frame waits at its end so that it takes its fixed game time in real time; no wait in fast mode, without
/// a fixed frame time, or once the frame took that long or longer
[[nodiscard]] std::chrono::microseconds PaceWait(const Settings& settings, std::optional<std::chrono::microseconds> frameTime,
                                                 std::chrono::microseconds frameTook);

/// The times of the turns and the frames since the last record
struct Timings
{
	uint32_t turns {0};
	float turnTotalMs {0.0f};
	float turnLongestMs {0.0f};
	uint32_t frames {0};
	float frameTotalMs {0.0f};
	float frameLongestMs {0.0f};
	/// The time of each stage of the profiler over the frames, by the stage's number
	std::vector<float> stageTotalMs;

	void AddTurn(float ms);
	void AddFrame(float ms);
	/// Adds the time that each stage took in a frame, by the stage's number
	void AddStages(std::span<const float> ms);
	[[nodiscard]] float TurnAverageMs() const;
	[[nodiscard]] float FrameAverageMs() const;
};

} // namespace openblack::sim_run
