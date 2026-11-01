/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SyncChecksumSystem.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/meta/context.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Simulation/ChecksumLog.h"

using namespace openblack::ecs::systems;

SyncChecksumSystem::SyncChecksumSystem()
    : _reflection(std::make_unique<entt::meta_ctx>())
{
	reflection::RegisterComponents(*_reflection);
}

SyncChecksumSystem::~SyncChecksumSystem() = default;

openblack::WorkerPool& SyncChecksumSystem::Pool()
{
	if (_pool == nullptr)
	{
		_pool = std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
	}
	return *_pool;
}

void SyncChecksumSystem::ProcessTurn()
{
	// The start of each turn ends a hash that waits. This call is for a turn that a tool plays alone
	FinishHash();
	if (!_enabled || !Locator::entitiesRegistry::has_value() || !Locator::time::has_value())
	{
		_lastHash.reset();
		return;
	}
	const auto turn = Locator::time::value().GetTurn();
	// The synced values that the systems keep outside the entities, as the turn left them
	TurnValues values {.turn = turn};
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		values.seed = random.GetSeeds().synced;
		values.draws = random.GetSyncedDraws();
	}
	// The music that the scripts start is synced too: its piece, its lyrics and its clock
	if (Locator::scriptMusicSystem::has_value())
	{
		values.music = Locator::scriptMusicSystem::value().GetMusic().SyncedValues();
	}
	bool rowDump = false;
	bool worldStates = false;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	rowDump = _rowDump != nullptr;
	worldStates = _worldStates.has_value();
#endif
	if (synced_state::HashCanWait({.besideDraw = _besideDraw,
	                               .keepParts = _keepParts,
	                               .dumpThisTurn = _dumpTurn == turn,
	                               .rowDump = rowDump,
	                               .worldStates = worldStates}))
	{
		// The hash of the last turn stays for a tool until the hash of this turn ends
		_waiting = values;
		return;
	}
	// A tool takes the hash only in the turn that made it
	_lastHash.reset();
	std::vector<std::string> dump;
	ecs::reflection::RowVisitor visit;
	if (_dumpTurn == turn)
	{
		visit = [this, &dump](std::string_view component, entt::entity entity, std::string_view field,
		                      std::span<const uint8_t> bytes) {
			if (std::ranges::find(_dumpComponents, component) == _dumpComponents.end() &&
			    std::ranges::find(_dumpComponents, "*") == _dumpComponents.end())
			{
				return;
			}
			auto line = fmt::format("{} {} {} ", component, entt::to_integral(entity), field);
			for (const auto byte : bytes)
			{
				line += fmt::format("{:02x}", byte);
			}
			dump.push_back(std::move(line));
		};
	}
	ecs::reflection::WorldRows* rows = nullptr;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	ecs::reflection::WorldRows keptRows;
	if (_rowDump)
	{
		rows = &keptRows;
	}
#endif
	const bool byField = _keepParts && _byField;
	auto hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_reflection,
	                                 synced_state::PresentationParts(), byField, visit, &Pool(), rows, &_memo);
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// The rows go to the dump before the listener compares the checksum, so that a part at this turn finds them. The
	// checksum also mixes the synced seed and the count of the synced draws, so the rows keep them too.
	if (_rowDump)
	{
		row_dump::AddValue(keptRows, "GameRandom.seed",
		                   std::span(reinterpret_cast<const uint8_t*>(&values.seed), sizeof(values.seed)));
		row_dump::AddValue(keptRows, "GameRandom.draws",
		                   std::span(reinterpret_cast<const uint8_t*>(&values.draws), sizeof(values.draws)));
		_rowDump->Keep(turn, std::move(keptRows));
	}
#endif
	Finish(values, std::move(hash), byField, std::move(dump));
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// The world state of the tools is the world of this checksum
	if (_worldStates.has_value())
	{
		world_state_at_turn::WriteIfDue(*_worldStates, turn, *_reflection);
	}
#endif
}

void SyncChecksumSystem::StartHashBesideDraw()
{
	if (!_waiting.has_value() || _besideStarted || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	if (_side == nullptr)
	{
		_side = std::make_unique<SideThread>();
	}
	auto& pool = Pool();
	_besideStarted = true;
	// The draw only reads the world, so the threads read it at the same time. The side thread shares the hash with the
	// threads of the pool, as the main thread does when the hash does not wait.
	_side->Start([this, &pool, &registry = Locator::entitiesRegistry::value().Underlying()] {
		_besideHash =
		    synced_state::HashOf(registry, *_reflection, synced_state::PresentationParts(), false, {}, &pool, nullptr, &_memo);
	});
}

void SyncChecksumSystem::FinishHash()
{
	if (!_waiting.has_value())
	{
		return;
	}
	const auto values = *std::exchange(_waiting, std::nullopt);
	synced_state::Hash hash;
	if (std::exchange(_besideStarted, false))
	{
		_side->Wait();
		hash = std::move(*std::exchange(_besideHash, std::nullopt));
	}
	else
	{
		hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_reflection,
		                            synced_state::PresentationParts(), false, {}, &Pool(), nullptr, &_memo);
	}
	Finish(values, std::move(hash), false, {});
}

void SyncChecksumSystem::Finish(const TurnValues& values, synced_state::Hash hash, bool byField, std::vector<std::string> dump)
{
	auto stateHash = hash.hash;
	if (values.music.has_value())
	{
		stateHash = synced_state::MixValues(stateHash, *values.music);
	}
	// A component with no registration is left out of the checksum, so a change in it is never seen: tell of each name once
	if (const auto fresh = _reported.Fresh(hash.unregistered); !fresh.empty())
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			logger->error("{}", synced_state::UnregisteredLine(fresh));
		}
	}
	_last = TurnChecksum {.turn = values.turn,
	                      .checksum = synced_state::Checksum(stateHash, values.seed, values.draws),
	                      .parts = {},
	                      .dump = std::move(dump),
	                      .unregistered = hash.unregistered};
	if (_keepParts)
	{
		_last->parts = hash.parts;
	}
	if (_listener)
	{
		_listener(*_last);
	}
	_lastHash = TurnHash {.turn = values.turn, .byField = byField, .hash = std::move(hash)};
	if (_hashListener)
	{
		_hashListener(values.turn, _lastHash->hash);
	}
}

#if defined(OPENBLACK_DEVELOPER_TOOLS)
bool SyncChecksumSystem::StartRowDump(const std::filesystem::path& path, row_dump::Settings settings)
{
	_rowDump.reset();
	_rowFile.reset();
	_rowLines.reset();
	// The path "-" writes the rows to the standard output, as the web build has no file that a tool can read
	if (path == checksum_log::k_StandardOutput)
	{
		_rowLines = std::make_unique<row_dump::TaggedLines>(checksum_log::k_OutputTag);
		_rowFile = std::make_unique<std::ostream>(_rowLines.get());
	}
	else
	{
		auto file = std::make_unique<std::ofstream>(path, std::ios::binary | std::ios::trunc);
		if (!file->is_open())
		{
			return false;
		}
		_rowFile = std::move(file);
	}
	_rowDump = std::make_unique<row_dump::RowDump>(*_rowFile, settings);
	return true;
}

void SyncChecksumSystem::DumpPart(uint32_t turn, std::string_view reason, uint32_t source)
{
	if (_rowDump)
	{
		_rowDump->Part(turn, reason, source);
	}
}
#endif
