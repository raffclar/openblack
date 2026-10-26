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
	const auto hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_reflection,
	                                       synced_state::PresentationParts(), _keepParts && _byField, visit, _pool.get());
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
