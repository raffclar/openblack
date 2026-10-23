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

#include "Common/GameRandom.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/SyncedState.h"
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
	const auto hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_reflection,
	                                       synced_state::PresentationParts(), _keepParts && _byField, visit);
	uint32_t seed = 0;
	uint64_t draws = 0;
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		seed = random.GetSeeds().synced;
		draws = random.GetSyncedDraws();
	}
	_last = TurnChecksum {
	    .turn = turn, .checksum = synced_state::Checksum(hash.hash, seed, draws), .parts = {}, .dump = std::move(dump)};
	if (_keepParts)
	{
		_last->parts = hash.parts;
	}
	if (_listener)
	{
		_listener(*_last);
	}
}
