/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SimRecorder.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <map>
#include <mutex>
#include <numeric>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "ComponentReflection.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePhysiology.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EntityDescription.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Profiler.h"
#include "SystemProviders.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace openblack::inspector
{

/// Keeps the warnings, errors and critical messages of every log until the record takes them
class SimLogSink final: public spdlog::sinks::base_sink<std::mutex>
{
public:
	struct Message
	{
		sim_checks::LogLevel level {sim_checks::LogLevel::Other};
		std::string text;
	};

	[[nodiscard]] std::vector<Message> Take()
	{
		const std::lock_guard lock(mutex_);
		return std::exchange(_messages, {});
	}

protected:
	void sink_it_(const spdlog::details::log_msg& message) override
	{
		if (message.level < spdlog::level::warn || _messages.size() >= k_MostKept)
		{
			return;
		}
		const auto level = message.level == spdlog::level::critical ? sim_checks::LogLevel::Critical
		                   : message.level == spdlog::level::err    ? sim_checks::LogLevel::Error
		                                                            : sim_checks::LogLevel::Warning;
		_messages.push_back({.level = level, .text = std::string(message.payload.data(), message.payload.size())});
	}
	void flush_() override {}

private:
	/// The most messages that the sink keeps between two records
	static constexpr size_t k_MostKept = 256;
	std::vector<Message> _messages;
};

} // namespace openblack::inspector

namespace
{

/// Where a slap lands by default, as a share of the creature's height
constexpr double k_SlapHeight = 0.5;
/// The most desires, beliefs and opinions that a record shows
constexpr size_t k_TopItems = 5;
constexpr double k_BytesPerMegabyte = 1024.0 * 1024.0;

/// The ranges of the creature's values, from the comments on their fields. The audit checks them against the game.
constexpr sim_checks::Range k_Unit {.lowest = 0.0f, .highest = 1.0f};
constexpr sim_checks::Range k_Signed {.lowest = -1.0f, .highest = 1.0f};

const ecs::Registry* RegistryOrNone()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

/// The player's first creature, or else the first creature with a mind
std::optional<entt::entity> RecordedCreature(const ecs::Registry& registry)
{
	if (Locator::playerSystem::has_value())
	{
		if (const auto primary = Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE);
		    primary.has_value() && registry.Valid(*primary) && registry.AllOf<CreatureMindState>(*primary))
		{
			return primary;
		}
	}
	std::optional<entt::entity> first;
	registry.Each<const CreatureMindState>([&first](entt::entity entity, const CreatureMindState&) {
		if (!first.has_value() || entity < *first)
		{
			first = entity;
		}
	});
	return first;
}

/// Goes through a component's fields as JSON: each number is checked, and the sizes of the lists add up by their path,
/// the places in a list left out of the path
void Walk(const Json& value, const std::string& path, uint32_t turn, std::vector<sim_checks::Flag>& flags,
          std::map<std::string, size_t, std::less<>>& lists)
{
	if (value.is_number_float())
	{
		if (auto flag = sim_checks::CheckValue(path, value.get<double>(), std::nullopt, turn); flag.has_value())
		{
			flags.push_back(std::move(*flag));
		}
		return;
	}
	if (value.is_array())
	{
		lists[path] += value.size();
		const auto inner = path + "[]";
		for (const auto& item : value)
		{
			Walk(item, inner, turn, flags, lists);
		}
		return;
	}
	if (value.is_object())
	{
		for (const auto& [key, item] : value.items())
		{
			Walk(item, path + "." + key, turn, flags, lists);
		}
	}
}

/// FNV-1a over bytes, as ecs.hash makes its hash
constexpr uint64_t k_HashOffset = 14695981039346656037ull;
constexpr uint64_t k_HashPrime = 1099511628211ull;
void Mix(uint64_t& hash, const void* data, size_t size)
{
	const auto* bytes = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < size; ++i)
	{
		hash = (hash ^ bytes[i]) * k_HashPrime;
	}
}

/// The hash that ecs.hash gives with no parameters: the entities in the order of their ids, each with its transform
uint64_t TransformHash(const ecs::Registry& registry)
{
	std::vector<entt::entity> entities;
	registry.Each<const Transform>([&entities](entt::entity entity, const Transform&) { entities.push_back(entity); });
	std::ranges::sort(entities);
	uint64_t hash = k_HashOffset;
	for (const auto entity : entities)
	{
		const auto id = entt::to_integral(entity);
		Mix(hash, &id, sizeof(id));
		const auto& transform = registry.Get<const Transform>(entity);
		Mix(hash, &transform.position, sizeof(transform.position));
		Mix(hash, &transform.rotation, sizeof(transform.rotation));
		Mix(hash, &transform.scale, sizeof(transform.scale));
	}
	return hash;
}

Json FlagJson(const sim_checks::Flag& flag)
{
	return {
	    {"kind", sim_checks::Name(flag.kind)},
	    {"what", flag.what},
	    {"turn", flag.turn},
	    {"detail", flag.detail},
	};
}

std::string_view RestName(CreatureNeeds::Rest rest)
{
	switch (rest)
	{
	case CreatureNeeds::Rest::Awake:
		return "awake";
	case CreatureNeeds::Rest::Asleep:
		return "asleep";
	case CreatureNeeds::Rest::Resting:
		return "resting";
	case CreatureNeeds::Rest::Unconscious:
		return "unconscious";
	}
	return "unknown";
}

std::string ActionName(uint32_t action, const creature_mind_tables::Tables* tables)
{
	if (tables != nullptr && action < tables->actions.size())
	{
		return std::string(tables->actions[action].name);
	}
	return std::to_string(action);
}

/// The action that the creature carries out now and the number of the plan or agenda it is part of: its plan's action
/// while a plan is active, else the action that its idle mind started
std::optional<CreatureMindState::ActionUnderway> CurrentAction(const CreatureMindState& mind)
{
	if (mind.planActive && mind.planner.current.has_value())
	{
		return CreatureMindState::ActionUnderway {.serial = mind.planSerial,
		                                          .action = static_cast<uint32_t>(mind.planner.current->action)};
	}
	return mind.underway;
}

/// A stage of the profiler that takes less time than this each turn is left out of the record
constexpr float k_LeastStageMs = 0.005f;

} // namespace

std::optional<std::vector<sim_checks::StuckLimit>> openblack::inspector::ParseStuckLimits(const Json& table)
{
	const auto rows = table.find("limits");
	if (!table.is_object() || rows == table.end() || !rows->is_array())
	{
		return std::nullopt;
	}
	std::vector<sim_checks::StuckLimit> limits;
	for (const auto& row : *rows)
	{
		const auto key = StringMember(row, "key");
		if (!key.has_value())
		{
			return std::nullopt;
		}
		const auto turns = NumberMember(row, "turns");
		if (turns.has_value() && (*turns < 0.0 || std::floor(*turns) != *turns))
		{
			return std::nullopt;
		}
		limits.push_back(
		    {.key = *key, .turns = turns.has_value() ? std::optional(static_cast<uint32_t>(*turns)) : std::nullopt});
	}
	return limits;
}

SimProvider::SimProvider(const entt::meta_ctx& context)
    : _context(context)
{
}

SimProvider::~SimProvider()
{
	StopRecord();
}

std::vector<QueryDescription> SimProvider::Describe() const
{
	return {
	    {.name = "state",
	     .description = "The fast simulation mode, the fixed frame time and the run record that the game writes",
	     .parameters = {},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	    {.name = "fast",
	     .description = "Turns the fast simulation mode on or off. In the mode the game plays its frames one after the "
	                    "other and draws none or only some. Each frame takes the fixed frame time, so that the turns are "
	                    "the same at any speed. The state after",
	     .parameters = {{.name = "on", .type = "boolean", .description = "The fast mode on or off", .required = true},
	                    {.name = "draw_every",
	                     .type = "integer",
	                     .description = "Draw one frame in this many (0 for no frame)",
	                     .required = false},
	                    {.name = "frame_ms",
	                     .type = "integer",
	                     .description = "The fixed game time of each frame, 1 to 1000 (20 by default when the mode turns "
	                                    "on with no fixed frame time; 0 for the wall clock)",
	                     .required = false},
	                    {.name = "until_turn",
	                     .type = "integer",
	                     .description = "Stop the game at this turn (0 for never)",
	                     .required = false}},
	     .kind = ResultKind::Object,
	     .needsNear = false,
	     .writes = true},
	    {.name = "record",
	     .description = "Starts the run record in a file of JSON lines, or stops it with stop. The state after",
	     .parameters = {{.name = "path", .type = "string", .description = "The file to write", .required = false},
	                    {.name = "every",
	                     .type = "integer",
	                     .description = "Write a record each time this many turns go by (100 by default)",
	                     .required = false},
	                    {.name = "limits",
	                     .type = "string",
	                     .description = "The JSON table of how long a plan or an activity can stay the same",
	                     .required = false},
	                    {.name = "parts",
	                     .type = "boolean",
	                     .description = "Also keep the hash of the transforms of the entities of each component",
	                     .required = false},
	                    {.name = "fields",
	                     .type = "boolean",
	                     .description = "As parts, and the synced hash of each field of each component too",
	                     .required = false},
	                    {.name = "stop", .type = "boolean", .description = "Stop the record", .required = false}},
	     .kind = ResultKind::Object,
	     .needsNear = false,
	     .writes = true},
	    {.name = "feedback",
	     .description = "The player's hand strokes or slaps the player's creature, as the button would: it takes hold of "
	                    "the creature by a command first. With release, or kind release, it lets go, and the creature "
	                    "learns from the sum. The right button let go, input.release or input.gesture also lets go. A "
	                    "stroke waits for the clocks of the last one, and a slap needs the hand to "
	                    "hold the creature for a second first: an answer of busy is no stroke or slap",
	     .parameters = {{.name = "kind", .type = "string", .description = "stroke, slap or release", .required = true},
	                    {.name = "id",
	                     .type = "integer",
	                     .description = "The creature (the player's creature by default)",
	                     .required = false},
	                    {.name = "part",
	                     .type = "integer",
	                     .description = "The part stroked, 0 (the head) to 8 (the left hand); 3, the belly, by default",
	                     .required = false},
	                    {.name = "height",
	                     .type = "number",
	                     .description = "Where the slap lands, as a share of the creature's height (0.5 by default)",
	                     .required = false},
	                    {.name = "gentle", .type = "boolean", .description = "A gentle slap", .required = false},
	                    {.name = "release", .type = "boolean", .description = "Let go afterwards", .required = false}},
	     .kind = ResultKind::Object,
	     .needsNear = false,
	     .writes = true},
	    {.name = "sample",
	     .description = "The record of the game now: the player's creature, the counts, the times, and the problems in "
	                    "its values. Only the records keep the times of the turns and the frames",
	     .parameters = {},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	    {.name = "flags",
	     .description = "The problems that the stability checks found since the record started",
	     .parameters = {},
	     .kind = ResultKind::List,
	     .needsNear = false},
	    {.name = "limits",
	     .description = "The limits of how long a plan or an activity can stay the same, and the longest time that each "
	                    "stayed the same in this record",
	     .parameters = {},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	};
}

Json SimProvider::State() const
{
	const auto& settings = Locator::simulation::value().GetSettings();
	std::optional<uint32_t> fixed;
	if (Locator::time::has_value())
	{
		if (const auto frame = Locator::time::value().GetFixedFrameTime(); frame.has_value())
		{
			fixed = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(*frame).count());
		}
	}
	return {
	    {"fast", settings.fast},
	    {"draw_every", settings.drawEvery},
	    {"pace_to_wall_clock", settings.paceToWallClock},
	    {"until_turn", settings.untilTurn.has_value() ? Json(*settings.untilTurn) : Json(nullptr)},
	    {"frame_ms", fixed.has_value() ? Json(*fixed) : Json(nullptr)},
	    {"turn", Locator::time::has_value() ? Locator::time::value().GetTurn() : 0},
	    {"recording", _out.has_value()},
	    {"path", _out.has_value() ? Json(_path.generic_string()) : Json(nullptr)},
	    {"every", _every},
	    {"records", _records},
	    {"flags", _flags.size()},
	};
}

QueryResult SimProvider::Run(std::string_view query, const QueryContext& context)
{
	const auto& params = context.params;
	if (query == "state")
	{
		return QueryResult::Value(State());
	}
	if (query == "fast")
	{
		const auto on = BoolMember(params, "on");
		if (!on.has_value())
		{
			return QueryResult::Error("sim.fast needs on, true or false");
		}
		auto settings = Locator::simulation::value().GetSettings();
		settings.fast = *on;
		if (const auto draw = NumberMember(params, "draw_every"); draw.has_value())
		{
			if (*draw < 0.0 || std::floor(*draw) != *draw)
			{
				return QueryResult::Error("draw_every is a whole number from 0");
			}
			settings.drawEvery = static_cast<uint32_t>(*draw);
		}
		if (const auto until = NumberMember(params, "until_turn"); until.has_value())
		{
			if (*until < 0.0 || std::floor(*until) != *until)
			{
				return QueryResult::Error("until_turn is a whole number from 0");
			}
			settings.untilTurn = *until == 0.0 ? std::nullopt : std::optional(static_cast<uint32_t>(*until));
		}
		auto& time = Locator::time::value();
		if (const auto ms = NumberMember(params, "frame_ms"); ms.has_value())
		{
			if (*ms < 0.0 || *ms > sim_run::k_LongestFrameMs || std::floor(*ms) != *ms)
			{
				return QueryResult::Error("frame_ms is a whole number of milliseconds from 0 (the wall clock) to 1000");
			}
			time.SetFixedFrameTime(*ms == 0.0 ? std::nullopt
			                                  : std::optional(std::chrono::milliseconds(static_cast<uint32_t>(*ms))));
		}
		else if (settings.fast && !time.GetFixedFrameTime().has_value())
		{
			time.SetFixedFrameTime(std::chrono::milliseconds(sim_run::k_DefaultFastFrameMs));
		}
		// The inspector steps the game by its own clock: the normal speed waits for no wall clock
		settings.paceToWallClock = false;
		Locator::simulation::value().SetSettings(settings);
		return QueryResult::Value(State());
	}
	if (query == "record")
	{
		if (BoolMember(params, "stop").value_or(false))
		{
			StopRecord();
			return QueryResult::Value(State());
		}
		const auto path = StringMember(params, "path");
		if (!path.has_value())
		{
			return QueryResult::Error("sim.record needs a path, or stop");
		}
		const auto every = NumberMember(params, "every").value_or(sim_run::Request::k_DefaultRecordEvery);
		if (every < 1.0 || std::floor(every) != every)
		{
			return QueryResult::Error("every is a whole number of turns from 1");
		}
		const auto limits = StringMember(params, "limits");
		_hashParts = BoolMember(params, "fields").value_or(false)  ? sim_run::HashDetail::Fields
		             : BoolMember(params, "parts").value_or(false) ? sim_run::HashDetail::Components
		                                                           : sim_run::HashDetail::None;
		if (const auto error = StartRecord(*path, static_cast<uint32_t>(every),
		                                   limits.has_value() ? std::optional(std::filesystem::path(*limits)) : std::nullopt);
		    error.has_value())
		{
			return QueryResult::Error(*error);
		}
		return QueryResult::Value(State());
	}
	if (query == "feedback")
	{
		return Feedback(params);
	}
	if (query == "sample")
	{
		std::vector<sim_checks::Flag> flags;
		const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
		auto record = Sample(turn, _frame, false, flags);
		Json problems = Json::array();
		for (const auto& flag : flags)
		{
			problems.push_back(FlagJson(flag));
		}
		record["flags"] = std::move(problems);
		return QueryResult::Value(std::move(record));
	}
	if (query == "flags")
	{
		Json items = Json::array();
		for (const auto& flag : _flags)
		{
			items.push_back(FlagJson(flag));
		}
		return QueryResult::Value(std::move(items));
	}
	if (query == "limits")
	{
		Json limits = Json::array();
		for (const auto& limit : _stuck.Limits())
		{
			limits.push_back({{"key", limit.key}, {"turns", limit.turns.has_value() ? Json(*limit.turns) : Json(nullptr)}});
		}
		Json longest = Json::array();
		for (const auto& run : _stuck.LongestRuns())
		{
			longest.push_back(
			    {{"key", run.key}, {"turns", run.turns}, {"limit", run.limit.has_value() ? Json(*run.limit) : Json(nullptr)}});
		}
		return QueryResult::Value({{"limits", std::move(limits)}, {"longest", std::move(longest)}});
	}
	return QueryResult::Error("no query sim." + std::string(query));
}

std::optional<std::string> SimProvider::StartRecord(const std::filesystem::path& path, uint32_t every,
                                                    const std::optional<std::filesystem::path>& limitsPath)
{
	std::vector<sim_checks::StuckLimit> limits;
	if (limitsPath.has_value())
	{
		std::ifstream file(*limitsPath);
		const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const auto table = file ? Parse(text) : std::nullopt;
		const auto parsed = table.has_value() ? ParseStuckLimits(*table) : std::nullopt;
		if (!parsed.has_value())
		{
			return "the limits file " + limitsPath->generic_string() + " is not a table of limits";
		}
		limits = *parsed;
	}
	StopRecord();
	std::error_code error;
	if (path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path(), error);
	}
	_out.emplace(path, std::ios::out | std::ios::trunc);
	if (!*_out)
	{
		_out.reset();
		return "the record cannot open " + path.generic_string();
	}
	_path = path;
	_every = every;
	_records = 0;
	_lastTurn.reset();
	_started = std::chrono::steady_clock::now();
	_stuck = sim_checks::StuckTracker(std::move(limits));
	_growth = sim_checks::GrowthTracker();
	_turnTime = sim_checks::TurnTimeTrend();
	_flags.clear();
	_flagged.clear();
	// The record sees the messages of every log
	_sink = std::make_shared<SimLogSink>();
	spdlog::apply_all([this](const std::shared_ptr<spdlog::logger>& logger) { logger->sinks().push_back(_sink); });
	// The times so far are not the record's
	[[maybe_unused]] const auto before = Locator::simulation::value().TakeTimings();
	// The record keeps the synced part of the world at the end of each turn that it records
	_synced.reset();
	Locator::simulation::value().SetTurnEndWatcher([this](uint32_t turn) { TurnEnded(turn); });
	auto start = State();
	start["type"] = "start";
	start["seed"] = Locator::rng::has_value() ? Json(Locator::rng::value().GetRunSeed()) : Json(nullptr);
	*_out << Dump(start) << '\n';
	_out->flush();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Simulation record: {} every {} turns", path.generic_string(), every);
	return std::nullopt;
}

void SimProvider::StopRecord()
{
	if (Locator::simulation::has_value())
	{
		Locator::simulation::value().SetTurnEndWatcher({});
	}
	_synced.reset();
	if (_sink != nullptr)
	{
		spdlog::apply_all([this](const std::shared_ptr<spdlog::logger>& logger) {
			auto& sinks = logger->sinks();
			sinks.erase(std::remove(sinks.begin(), sinks.end(), _sink), sinks.end());
		});
		_sink.reset();
	}
	if (!_out.has_value())
	{
		return;
	}
	std::map<std::string, size_t> counts;
	for (const auto& flag : _flags)
	{
		++counts[std::string(sim_checks::Name(flag.kind))];
	}
	Json longest = Json::array();
	for (const auto& run : _stuck.LongestRuns())
	{
		longest.push_back(
		    {{"key", run.key}, {"turns", run.turns}, {"limit", run.limit.has_value() ? Json(*run.limit) : Json(nullptr)}});
	}
	const Json end = {
	    {"type", "end"},         {"turn", Locator::time::has_value() ? Locator::time::value().GetTurn() : 0},
	    {"records", _records},   {"wall_s", std::chrono::duration<double>(std::chrono::steady_clock::now() - _started).count()},
	    {"flag_counts", counts}, {"longest", std::move(longest)},
	};
	*_out << Dump(end) << '\n';
	_out->close();
	_out.reset();
}

void SimProvider::TurnEnded(uint32_t turn)
{
	const auto* registry = RegistryOrNone();
	if (!_out.has_value() || _every == 0 || turn % _every != 0 || registry == nullptr)
	{
		return;
	}
	// The hash of the entities and of every saved component, but for the parts that only show the world on this computer
	const auto synced = synced_state::HashOf(registry->Underlying(), _context, synced_state::PresentationParts(),
	                                         _hashParts == sim_run::HashDetail::Fields);
	Json parts = Json::object();
	for (const auto& [name, part] : synced.parts)
	{
		parts[name] = fmt::format("{:016x}", part);
	}
	Json value = {{"hash", fmt::format("{:016x}", synced.hash)}};
	// A component with no registration is left out of the hash: tell of each name once, and show the names in the record
	if (!synced.unregistered.empty())
	{
		value["unregistered"] = synced.unregistered;
		if (const auto fresh = _unregisteredTold.Fresh(synced.unregistered); !fresh.empty())
		{
			if (const auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->error("{}", synced_state::UnregisteredLine(fresh));
			}
		}
	}
	// The music that the scripts start is synced too
	auto stateHash = synced.hash;
	if (Locator::scriptMusicSystem::has_value())
	{
		const auto music = Locator::scriptMusicSystem::value().GetMusic().SyncedValues();
		value["script_music"] = music;
		stateHash = synced_state::MixValues(stateHash, music);
	}
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		value["seed"] = random.GetSeeds().synced;
		value["draws"] = random.GetSyncedDraws();
		// The checksum that a network game compares each turn
		value["checksum"] =
		    fmt::format("{:016x}", synced_state::Checksum(stateHash, random.GetSeeds().synced, random.GetSyncedDraws()));
	}
	if (_hashParts != sim_run::HashDetail::None)
	{
		value["parts"] = std::move(parts);
	}
	_synced = Synced {.turn = turn, .value = std::move(value)};
}

bool SimProvider::Keep(const sim_checks::Flag& flag)
{
	// A value that stays wrong flags once, not at each record
	const bool once = flag.kind == sim_checks::FlagKind::NotANumber || flag.kind == sim_checks::FlagKind::OutOfRange;
	if (once)
	{
		const auto key = std::string(sim_checks::Name(flag.kind)) + ":" + flag.what;
		if (!_flagged.insert(key).second)
		{
			return false;
		}
	}
	_flags.push_back(flag);
	return true;
}

void SimProvider::Frame(uint64_t frame)
{
	_frame = frame;
	if (!_out.has_value() || !Locator::time::has_value())
	{
		return;
	}
	const auto turn = Locator::time::value().GetTurn();
	if (_lastTurn == turn)
	{
		return;
	}
	_lastTurn = turn;
	if (turn % _every != 0)
	{
		return;
	}
	std::vector<sim_checks::Flag> found;
	auto record = Sample(turn, frame, true, found);
	if (_sink != nullptr)
	{
		for (const auto& message : _sink->Take())
		{
			if (auto flag = sim_checks::CheckLogMessage(message.level, message.text, turn); flag.has_value())
			{
				found.push_back(std::move(*flag));
			}
		}
	}
	Json problems = Json::array();
	for (const auto& flag : found)
	{
		if (Keep(flag))
		{
			problems.push_back(FlagJson(flag));
			if (flag.kind != sim_checks::FlagKind::Log)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "Simulation check at turn {}: {} {}: {}", flag.turn,
				                   sim_checks::Name(flag.kind), flag.what, flag.detail);
			}
		}
	}
	record["type"] = "record";
	// The seeds and the count of the synced draws: two runs with the same draws in the same order keep the same
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		const auto seeds = random.GetSeeds();
		record["seeds"] = {seeds.synced, seeds.local};
		record["draws"] = random.GetSyncedDraws();
	}
	// The synced part of the world at the end of the turn, which the frame rate must not change
	if (_synced.has_value() && _synced->turn == turn)
	{
		record["synced"] = std::move(_synced->value);
		_synced.reset();
	}
	record["flags"] = std::move(problems);
	*_out << Dump(record) << '\n';
	_out->flush();
	++_records;
}

Json SimProvider::Sample(uint32_t turn, uint64_t frame, bool track, std::vector<sim_checks::Flag>& flags)
{
	auto& simulation = Locator::simulation::value();
	const auto timings = track ? simulation.TakeTimings() : sim_run::Timings {};
	const auto memory = simulation.ProcessMemoryBytes();
	Json record = {
	    {"turn", turn},
	    {"frame", frame},
	    {"wall_s", std::chrono::duration<double>(std::chrono::steady_clock::now() - _started).count()},
	    {"turn_ms", timings.TurnAverageMs()},
	    {"turn_ms_max", timings.turnLongestMs},
	    {"turns", timings.turns},
	    {"frame_ms", timings.FrameAverageMs()},
	    {"frame_ms_max", timings.frameLongestMs},
	    {"frames", timings.frames},
	    {"memory_mb", memory.has_value() ? Json(static_cast<double>(*memory) / k_BytesPerMegabyte) : Json(nullptr)},
	};
	// The stages of the profiler that took the most time, in milliseconds for each turn
	if (timings.turns > 0)
	{
		Json stages = Json::object();
		for (size_t i = 0; i < timings.stageTotalMs.size() && i < Profiler::k_StageNames.size(); ++i)
		{
			const auto perTurn = timings.stageTotalMs[i] / static_cast<float>(timings.turns);
			if (perTurn >= k_LeastStageMs)
			{
				stages[fmt::format("{}:{}", i, Profiler::k_StageNames.at(i))] = perTurn;
			}
		}
		record["stages"] = std::move(stages);
	}
	if (track && timings.turns > 0)
	{
		if (auto flag = _turnTime.Observe(timings.TurnAverageMs(), turn); flag.has_value())
		{
			flags.push_back(std::move(*flag));
		}
	}
	if (Locator::camera::has_value())
	{
		const auto eye = Locator::camera::value().GetOrigin();
		const auto focus = Locator::camera::value().GetFocus();
		record["camera"] = {eye.x, eye.y, eye.z, focus.x, focus.y, focus.z};
	}
	const auto* registry = RegistryOrNone();
	if (registry == nullptr)
	{
		record["creature"] = nullptr;
		return record;
	}
	// The counts of the entities by their components, and the reactions now in the world
	Json counts = Json::object();
	size_t entities = 0;
	for (const auto& [id, storage] : registry->Underlying().storage())
	{
		const auto& type = reflection::StorageType(storage);
		if (type == entt::type_id<entt::entity>())
		{
			continue;
		}
		const auto name = reflection::ShortTypeName(type);
		if (type == entt::type_id<Transform>())
		{
			entities = storage.size();
		}
		if (track)
		{
			if (auto flag = _growth.Observe("count:" + std::string(name), storage.size(), turn); flag.has_value())
			{
				flags.push_back(std::move(*flag));
			}
		}
		if (!storage.empty())
		{
			counts[std::string(name)] = storage.size();
		}
	}
	record["entities"] = entities;
	// How many entities the world made and lost since it started: a count that two computers do not share shows a step
	// that made or removed an entity outside the turns
	record["entities_made"] = registry->GetEntitiesMade();
	record["entities_gone"] = registry->GetEntitiesGone();
	record["hash"] = fmt::format("{:016x}", TransformHash(*registry));
	// The hashes of the transforms of the entities of each component, to find what made two runs part
	if (_hashParts != sim_run::HashDetail::None)
	{
		Json parts = Json::object();
		for (const auto& [id, storage] : registry->Underlying().storage())
		{
			const auto& type = reflection::StorageType(storage);
			if (type == entt::type_id<entt::entity>() || storage.empty())
			{
				continue;
			}
			std::vector<entt::entity> members;
			for (const auto member : storage)
			{
				if (registry->AllOf<Transform>(member))
				{
					members.push_back(member);
				}
			}
			if (members.empty())
			{
				continue;
			}
			std::ranges::sort(members);
			uint64_t hash = k_HashOffset;
			for (const auto member : members)
			{
				const auto value = entt::to_integral(member);
				Mix(hash, &value, sizeof(value));
				const auto& transform = registry->Get<const Transform>(member);
				Mix(hash, &transform.position, sizeof(transform.position));
				Mix(hash, &transform.rotation, sizeof(transform.rotation));
				Mix(hash, &transform.scale, sizeof(transform.scale));
			}
			parts[reflection::ShortTypeName(type)] = fmt::format("{:016x}", hash);
		}
		record["hash_parts"] = std::move(parts);
	}
	record["counts"] = std::move(counts);
	const auto reactions = Locator::reactionSystem::has_value() ? Locator::reactionSystem::value().GetReactions().size() : 0;
	record["reactions"] = reactions;
	if (track)
	{
		if (auto flag = _growth.Observe("count:reactions", reactions, turn); flag.has_value())
		{
			flags.push_back(std::move(*flag));
		}
	}
	record["creature"] = CreatureRecord(turn, track, flags);
	return record;
}

Json SimProvider::CreatureRecord(uint32_t turn, bool track, std::vector<sim_checks::Flag>& flags)
{
	const auto& registry = *RegistryOrNone();
	const auto entity = RecordedCreature(registry);
	if (!entity.has_value())
	{
		return nullptr;
	}
	const auto check = [&flags, turn](std::string_view what, double value, std::optional<sim_checks::Range> range) {
		if (auto flag = sim_checks::CheckValue(what, value, range, turn); flag.has_value())
		{
			flags.push_back(std::move(*flag));
		}
	};
	const auto* tables = Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	const auto& mind = registry.Get<const CreatureMindState>(*entity);
	Json creature = {{"id", ToId(*entity)}, {"stage", mind.developmentPhase}};

	// The body
	if (const auto* body = registry.TryGet<const Creature>(*entity); body != nullptr)
	{
		creature["size"] = body->size;
		creature["fatness"] = body->fatness;
		creature["strength"] = body->strength;
		creature["alignment"] = body->alignment;
		check("creature.size", body->size, std::nullopt);
		check("creature.fatness", body->fatness, k_Unit);
		check("creature.strength", body->strength, k_Unit);
		check("creature.alignment", body->alignment, k_Signed);
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(*entity); needs != nullptr)
	{
		const auto& body = needs->needs;
		const auto* shape = registry.TryGet<const Creature>(*entity);
		const auto largestEnergy = std::max(1.0f, shape != nullptr ? shape->size : 1.0f);
		creature["age"] = body.age;
		creature["lived_turns"] = body.turns;
		creature["hunger"] = creature_physiology::Hunger(body);
		creature["energy"] = body.energy;
		creature["tiredness"] = body.exhaustion;
		creature["health"] = body.life;
		creature["thirst"] = body.dehydration;
		creature["warmth"] = body.warmth;
		creature["rest"] = RestName(needs->rest);
		check("needs.energy", body.energy, sim_checks::Range {.lowest = 0.0f, .highest = largestEnergy});
		check("needs.exhaustion", body.exhaustion, k_Unit);
		check("needs.life", body.life, k_Unit);
		check("needs.dehydration", body.dehydration, k_Unit);
		check("needs.itchiness", body.itchiness, k_Unit);
		check("needs.poo", body.poo, k_Unit);
		check("needs.warmth", body.warmth, k_Signed);
	}

	// The desires
	if (mind.desires.has_value())
	{
		creature["desires"] = DesireItems(*mind.desires, k_TopItems);
		creature["desire_sum"] = mind.desires->sum;
		for (size_t i = 0; i < mind.desires->desires.size(); ++i)
		{
			const auto& desire = mind.desires->desires.at(i);
			const auto name = "desire." + std::string(creature_desires::Name(static_cast<creature_desires::Desire>(i)));
			check(name, desire.value, sim_checks::Range {.lowest = 0.0f, .highest = std::max(desire.max, 0.0f)});
			for (const auto& source : desire.sources)
			{
				check(name + ".source", source.value, std::nullopt);
			}
		}
	}

	// The plan, the activity and the action
	const auto plan = PlanJson(mind, tables);
	creature["plan"] = plan["plan"];
	creature["plan_active"] = mind.planActive;
	creature["plan_serial"] = mind.planSerial;
	creature["activity"] = plan["activity"];
	creature["agenda_steps"] = plan["agenda_steps"];
	// The action it is carrying out: its plan's while a plan is active, else the one the idle mind started
	const auto action = CurrentAction(mind);
	creature["action"] = action.has_value() ? Json(ActionName(action->action, tables)) : Json(nullptr);
	creature["idle_action"] = mind.underway.has_value() ? Json(ActionName(mind.underway->action, tables)) : Json(nullptr);
	creature["last_action_done"] =
	    mind.lastActionDone.has_value() ? Json(ActionName(*mind.lastActionDone, tables)) : Json(nullptr);
	// What the player's hand last told it
	creature["feedback_seconds"] = mind.feedbackSeconds.has_value() ? Json(*mind.feedbackSeconds) : Json(nullptr);
	creature["feedback_was_stroke"] = mind.feedbackWasStroke;
	creature["last_feedback"] = mind.lastFeedback.has_value() ? Json(mind.lastFeedback->value) : Json(nullptr);
	creature["attitude_to_player"] = mind.attitudeToPlayer;
	check("mind.attitude_to_player", mind.attitudeToPlayer, std::nullopt);
	creature["plan_forced"] = mind.planForced;
	creature["emergency"] = mind.emergencyRunning;
	if (track)
	{
		const auto planName =
		    mind.planner.current.has_value() ? ActionName(mind.planner.current->action, tables) : std::string("none");
		for (auto flag : {_stuck.Observe("plan", planName, mind.planSerial, turn),
		                  _stuck.Observe("activity", plan["activity"].get<std::string>(), 0, turn),
		                  _stuck.Observe("action", action.has_value() ? ActionName(action->action, tables) : "none",
		                                 action.has_value() ? action->serial : 0, turn)})
		{
			if (flag.has_value())
			{
				flags.push_back(std::move(*flag));
			}
		}
	}

	// What it reacts to, its leash and its friends
	if (const auto* reaction = registry.TryGet<const LivingReaction>(*entity); reaction != nullptr)
	{
		creature["reaction"] = {{"type", static_cast<uint32_t>(reaction->type)}, {"since", reaction->startTurn}};
	}
	else
	{
		creature["reaction"] = nullptr;
	}
	creature["reacting"] = mind.keptPlan.has_value();
	if (const auto* leash = registry.TryGet<const CreatureLeash>(*entity); leash != nullptr)
	{
		creature["leash"] = {
		    {"worn", leash->worn.has_value()}, {"order", leash->order.has_value()}, {"radius", leash->confinementRadius}};
	}
	else
	{
		creature["leash"] = nullptr;
	}
	if (const auto* friends = registry.TryGet<const CreatureFriends>(*entity); friends != nullptr)
	{
		creature["friends"] = friends->friends;
	}
	else
	{
		creature["friends"] = Json::array();
	}

	// The beliefs, the opinions and the lists of what it learnt
	Json beliefs = Json::array();
	for (const auto& belief : mind.beliefs.NewestFirst())
	{
		if (beliefs.size() >= k_TopItems)
		{
			break;
		}
		beliefs.push_back({{"thing", ToId(belief.thing)}, {"kind", belief.kind}});
	}
	Json byKind = Json::object();
	for (uint32_t kind = 0; kind < creature_object_beliefs::k_KindCount; ++kind)
	{
		if (const auto count = mind.beliefs.Count(kind); count > 0)
		{
			byKind[std::to_string(kind)] = count;
		}
	}
	creature["beliefs"] = {{"count", mind.beliefs.Size()}, {"by_kind", std::move(byKind)}, {"newest", std::move(beliefs)}};
	if (mind.learnt.has_value())
	{
		const auto& learnt = *mind.learnt;
		std::vector<size_t> order(learnt.opinions.size());
		std::iota(order.begin(), order.end(), size_t {0});
		std::ranges::stable_sort(order, [&learnt](size_t a, size_t b) { return learnt.opinions[a] > learnt.opinions[b]; });
		Json opinions = Json::array();
		for (size_t i = 0; i < std::min(k_TopItems, order.size()); ++i)
		{
			opinions.push_back(
			    {{"action", ActionName(static_cast<uint32_t>(order[i]), tables)}, {"value", learnt.opinions[order[i]]}});
		}
		creature["opinions"] = std::move(opinions);
		creature["attitudes"] = learnt.creatures.size();
		creature["thoughts"] = learnt.thoughts.size();
		// The newest thing it did that feedback can be credited to, as a player sees it: the action, its desire, the kind
		// of thing it acted on, and whether it can learn from it
		if (!learnt.contexts.empty())
		{
			const auto& newest = learnt.contexts.back();
			// How the game's table of desires says a plan for the desire moves the creature's alignment each turn
			std::optional<float> alignmentChange;
			if (Locator::infoConstants::has_value())
			{
				const auto& desires = Locator::infoConstants::value().creatureInitialDesire;
				if (const auto index = static_cast<size_t>(newest.desire); index < desires.size())
				{
					alignmentChange = desires.at(index).alignmentChange;
				}
			}
			creature["context"] = {
			    {"action", ActionName(newest.action, tables)},
			    {"desire", std::string(creature_desires::Name(newest.desire))},
			    {"object_kind", newest.belief.has_value() ? Json(newest.belief->type) : Json(nullptr)},
			    {"alignment_change", alignmentChange.has_value() ? Json(*alignmentChange) : Json(nullptr)},
			    {"learnable", newest.learnable},
			    {"running", newest.running},
			    {"done", newest.done},
			    {"seconds_since", newest.secondsSince},
			};
		}
		else
		{
			creature["context"] = nullptr;
		}
		check("learnt.alignment", learnt.alignment, k_Signed);
		for (size_t i = 0; i < learnt.opinions.size(); ++i)
		{
			check("learnt.opinion." + ActionName(static_cast<uint32_t>(i), tables), learnt.opinions[i], std::nullopt);
		}
	}
	if (track)
	{
		if (auto flag = _growth.Observe("list:beliefs", mind.beliefs.Size(), turn); flag.has_value())
		{
			flags.push_back(std::move(*flag));
		}
	}

	// Every field of every component of the creature that the inspector can read: its numbers and its lists
	std::map<std::string, size_t, std::less<>> lists;
	// The hash of all the fields, to compare two runs
	uint64_t stateHash = k_HashOffset;
	for (const auto& [id, storage] : registry.Underlying().storage())
	{
		if (!storage.contains(*entity) || reflection::StorageType(storage) == entt::type_id<entt::entity>())
		{
			continue;
		}
		const auto& type = reflection::StorageType(storage);
		if (!reflection::IsReflected(_context, type))
		{
			continue;
		}
		const auto fields = reflection::ComponentToJson(_context, type, storage.value(*entity));
		const auto text = Dump(fields);
		Mix(stateHash, text.data(), text.size());
		Walk(fields, std::string(reflection::ShortTypeName(type)), turn, flags, lists);
	}
	if (track)
	{
		for (const auto& [path, size] : lists)
		{
			if (auto flag = _growth.Observe("list:" + path, size, turn); flag.has_value())
			{
				flags.push_back(std::move(*flag));
			}
		}
	}
	creature["lists"] = lists.size();
	creature["state_hash"] = fmt::format("{:016x}", stateHash);
	return creature;
}

QueryResult SimProvider::Feedback(const Json& params)
{
	const auto* registry = RegistryOrNone();
	if (registry == nullptr || !Locator::creatureHandSystem::has_value())
	{
		return QueryResult::Error("there is no land or no hand");
	}
	auto creature = RecordedCreature(*registry);
	if (const auto id = params.find("id"); id != params.end())
	{
		creature = FromId(*id);
	}
	if (!creature.has_value() || !registry->Valid(*creature))
	{
		return QueryResult::Error("no creature");
	}
	auto& hand = Locator::creatureHandSystem::value();
	const auto kind = StringMember(params, "kind").value_or("");
	std::string did;
	if (kind == "stroke")
	{
		const auto part = NumberMember(params, "part").value_or(static_cast<double>(creature_feedback::BodyPart::Belly));
		if (part < 0.0 || part >= static_cast<double>(creature_feedback::k_BodyPartCount))
		{
			return QueryResult::Error("part is 0 to 8");
		}
		did = hand.Stroke(*creature, static_cast<creature_feedback::BodyPart>(static_cast<uint8_t>(part))) ? "stroked" : "busy";
	}
	else if (kind == "slap")
	{
		const auto height = static_cast<float>(NumberMember(params, "height").value_or(k_SlapHeight));
		did = hand.Slap(*creature, height, BoolMember(params, "gentle").value_or(false), true) ? "slapped" : "busy";
	}
	else if (kind != "release")
	{
		return QueryResult::Error("kind is stroke, slap or release");
	}
	std::optional<float> sum;
	if (kind == "release" || BoolMember(params, "release").value_or(false))
	{
		if (hand.GetCreature() == *creature)
		{
			sum = hand.GetFeedbackSum();
			hand.Release();
		}
	}
	return QueryResult::Value({
	    {"id", ToId(*creature)},
	    {"did", did.empty() ? Json(nullptr) : Json(did)},
	    {"held", hand.GetCreature().has_value()},
	    {"sum", hand.GetFeedbackSum()},
	    {"released_with", sum.has_value() ? Json(*sum) : Json(nullptr)},
	});
}
