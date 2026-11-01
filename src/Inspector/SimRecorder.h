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

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <InspectorProvider.h>
#include <entt/meta/context.hpp>

#include "Debug/SimChecks.h"
#include "Debug/SimRun.h"
#include "ECS/SyncedState.h"

namespace openblack::inspector
{

class SimLogSink;

/// The fast simulation mode and the run record of a long tool run:
///   sim.state                          the mode, the fixed frame time, and the record that is written
///   sim.fast   {on, draw_every?, frame_ms?, until_turn?}   the fast mode on or off; the state after
///   sim.record {path?, every?, limits?, parts?, stop?}    starts or stops the record; the state after
///   sim.feedback {kind, id?, part?, height?, gentle?, release?}   the hand strokes or slaps the creature, or lets go
///   sim.sample                         the record of the game now, with the problems in its values
///   sim.flags                          the problems that the checks found since the record started
///   sim.limits                         the limits of how long a plan or an activity can stay the same, and the
///                                      longest time that each stayed the same
///
/// The record is a file of JSON lines: a "start" line, a "record" line each time a number of turns go by, and an "end"
/// line. A record has the player's creature (its desires, plan, action, beliefs, opinions, body, alignment, reaction,
/// leash and friends), the counts of the entities, the memory, the turn and frame times, and the new problems that the
/// stability checks found.
class SimProvider final: public ProviderInterface
{
public:
	explicit SimProvider(const entt::meta_ctx& context);
	~SimProvider() override;
	SimProvider(const SimProvider&) = delete;
	SimProvider& operator=(const SimProvider&) = delete;

	[[nodiscard]] std::string_view Name() const override { return "sim"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

	/// Once a frame, before the game's turn: writes a record when the turn is one to record
	void Frame(uint64_t frame);
	/// Starts the record in a file, with a record every so many turns and the limits from a file: an error, or none
	[[nodiscard]] std::optional<std::string> StartRecord(const std::filesystem::path& path, uint32_t every,
	                                                     const std::optional<std::filesystem::path>& limitsPath);
	void StopRecord();
	/// What the records keep of the hashes of the parts of the world
	void SetHashParts(sim_run::HashDetail parts) { _hashParts = parts; }
	/// The records read every field of the creature only on the turns that this number divides
	void SetFieldsEvery(uint32_t every) { _fieldsEvery = std::max(every, 1u); }

private:
	/// The record of the game now. With checks, the trackers see it; without, only its values are checked.
	[[nodiscard]] Json Sample(uint32_t turn, uint64_t frame, bool track, std::vector<sim_checks::Flag>& flags);
	[[nodiscard]] Json CreatureRecord(uint32_t turn, bool track, std::vector<sim_checks::Flag>& flags);
	[[nodiscard]] Json State() const;
	[[nodiscard]] QueryResult Feedback(const Json& params);
	/// Keeps a flag that is new, and gives false for a flag of the same kind and value as one kept before
	bool Keep(const sim_checks::Flag& flag);

	const entt::meta_ctx& _context;
	std::optional<std::ofstream> _out;
	std::filesystem::path _path;
	uint32_t _every {0};
	uint64_t _records {0};
	std::optional<uint32_t> _lastTurn;
	std::chrono::steady_clock::time_point _started {};
	sim_checks::StuckTracker _stuck;
	sim_checks::GrowthTracker _growth;
	sim_checks::TurnTimeTrend _turnTime;
	std::vector<sim_checks::Flag> _flags;
	std::set<std::string, std::less<>> _flagged;
	/// The unregistered components that the log already told of
	synced_state::UnregisteredReporter _unregisteredTold;
	/// The rows of the last hash of a record, so that the next hash mixes again only the rows that changed
	ecs::reflection::HashMemo _hashMemo;
	std::shared_ptr<SimLogSink> _sink;
	uint64_t _frame {0};
	/// What the records keep of the hashes of the parts of the world
	sim_run::HashDetail _hashParts {sim_run::HashDetail::None};
	uint32_t _fieldsEvery {1};
	/// The synced part of the world as each turn to record left it, before the steps of the frame
	struct Synced
	{
		uint32_t turn {0};
		/// Null while the hash of the turn waits for the draw of the frame
		Json value;
		/// The values of the systems that the checksum mixes: the music of the scripts, the synced seed and draws
		std::optional<std::array<int64_t, 5>> music;
		std::optional<std::pair<uint32_t, uint64_t>> random;
	};
	/// The turns to record that ended since the last record. A frame can end more than one turn: for example, when the
	/// turns of the next frame play beside the draw
	std::vector<Synced> _synced;
	/// At the end of a turn to record: keeps the synced part of the world
	void TurnEnded(uint32_t turn);
	/// The checksum finished the hash of a turn that waited for the draw
	void HashEnded(uint32_t turn, const synced_state::Hash& hash);
	/// Puts the hash of the synced world and the checksum into the synced part of a turn
	void AddHash(Synced& synced, const synced_state::Hash& hash);
};

/// The limits from a table as JSON: {"limits": [{"key": "plan:*", "turns": null}, ...]}; none when it is not one
[[nodiscard]] std::optional<std::vector<sim_checks::StuckLimit>> ParseStuckLimits(const Json& table);

} // namespace openblack::inspector
