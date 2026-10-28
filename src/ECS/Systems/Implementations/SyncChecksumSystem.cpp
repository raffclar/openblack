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
#include <span>
#include <string>
#include <string_view>
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

void SyncChecksumSystem::ProcessTurn()
{
	if (!_enabled || !Locator::entitiesRegistry::has_value() || !Locator::time::has_value())
	{
		return;
	}
	const auto turn = Locator::time::value().GetTurn();
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
	if (_pool == nullptr)
	{
		_pool = std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
	}
	ecs::reflection::WorldRows* rows = nullptr;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	ecs::reflection::WorldRows keptRows;
	if (_rowDump)
	{
		rows = &keptRows;
	}
#endif
	const auto hash =
	    synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_reflection, synced_state::PresentationParts(),
	                         _keepParts && _byField, visit, _pool.get(), rows, &_memo);
	// The music that the scripts start is synced too: its piece, its lyrics and its clock
	auto stateHash = hash.hash;
	if (Locator::scriptMusicSystem::has_value())
	{
		stateHash = synced_state::MixValues(stateHash, Locator::scriptMusicSystem::value().GetMusic().SyncedValues());
	}
	uint32_t seed = 0;
	uint64_t draws = 0;
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		seed = random.GetSeeds().synced;
		draws = random.GetSyncedDraws();
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// The rows go to the dump before the listener compares the checksum, so that a part at this turn finds them. The
	// checksum also mixes the synced seed and the count of the synced draws, so the rows keep them too.
	if (_rowDump)
	{
		row_dump::AddValue(keptRows, "GameRandom.seed", std::span(reinterpret_cast<const uint8_t*>(&seed), sizeof(seed)));
		row_dump::AddValue(keptRows, "GameRandom.draws", std::span(reinterpret_cast<const uint8_t*>(&draws), sizeof(draws)));
		_rowDump->Keep(turn, std::move(keptRows));
	}
#endif
	// A component with no registration is left out of the checksum, so a change in it is never seen: tell of each name once
	if (const auto fresh = _reported.Fresh(hash.unregistered); !fresh.empty())
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			logger->error("{}", synced_state::UnregisteredLine(fresh));
		}
	}
	_last = TurnChecksum {.turn = turn,
	                      .checksum = synced_state::Checksum(stateHash, seed, draws),
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
