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

#include <array>

#include <entt/meta/context.hpp>

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
	const auto hash =
	    synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_reflection, synced_state::PresentationParts());
	// The music that the scripts start is synced too: its piece, its lyrics and its clock
	auto stateHash = hash.hash;
	if (Locator::scriptMusicSystem::has_value())
	{
		const auto& music = Locator::scriptMusicSystem::value().GetMusic();
		const auto time = music.GetChannelTime();
		const std::array<int64_t, 5> values {static_cast<int64_t>(music.GetType()), music.IsStarted() ? 1 : 0,
		                                     music.GetLyrics().line, music.GetLyrics().word, time ? time->count() : -1};
		stateHash = synced_state::MixValues(stateHash, values);
	}
	uint32_t seed = 0;
	uint64_t draws = 0;
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		seed = random.GetSeeds().synced;
		draws = random.GetSyncedDraws();
	}
	_last = TurnChecksum {.turn = Locator::time::value().GetTurn(), .checksum = synced_state::Checksum(stateHash, seed, draws)};
	if (_listener)
	{
		_listener(*_last);
	}
}
