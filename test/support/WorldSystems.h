/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The includer builds its own locator: it defines LOCATOR_IMPLEMENTATIONS before its includes
#include "ECS/AnimalAI.h"
#include "ECS/Systems/Implementations/AnimalAISystem.h"
#include "ECS/Systems/Implementations/FireEffectSystem.h"
#include "ECS/Systems/Implementations/FireGraphicSystem.h"
#include "ECS/Systems/Implementations/FireSoundSystem.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/LandBalanceSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "ECS/Systems/Implementations/ReactionsSystem.h"
#include "ECS/Systems/Implementations/ToBeDeletedSystem.h"
#include "ECS/Systems/Implementations/TreeSystem.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "Locator.h"
#include "Magic/Objects/MagicTree.h"
#include "Magic/Spells/SpellClasses.h"

namespace openblack::test
{
/// The game's world lists, fresh, as InitializeGame puts them in: the dead list, the object creation counter, the
/// map cells, the reactions (with the animals' and the villagers' handlers), the fires (their effects, graphics and
/// crackle), the forests, the trees' shared state (with the magic trees' listener), the land balance and the animals'
/// shared state. The flock miracles' species dying is set too: the spell classes register it once per spell system,
/// so a fresh animal state under an older spell system would miss it. A test that puts an entities registry in the
/// locator emplaces them with it, and resets them after the registry, as the game's shutdown does (entities destroyed
/// with the registry may still reach them). Not part of the listener's defaults (TestServices.cpp)
inline void EmplaceWorldSystems()
{
	Locator::toBeDeletedSystem::emplace<ecs::systems::ToBeDeletedSystem>();
	Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
	Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
	Locator::reactionsSystem::emplace<ecs::systems::ReactionsSystem>();
	ecs::animal_ai::RegisterReactionHandler();
	ecs::villager_reactions::RegisterHandlers();
	Locator::fireEffectSystem::emplace<ecs::systems::FireEffectSystem>();
	Locator::fireGraphicSystem::emplace<ecs::systems::FireGraphicSystem>();
	Locator::fireSoundSystem::emplace<ecs::systems::FireSoundSystem>();
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	Locator::treeSystem::emplace<ecs::systems::TreeSystem>();
	magic::magic_tree::RegisterTreeListener();
	Locator::landBalanceSystem::emplace<ecs::systems::LandBalanceSystem>();
	Locator::animalAISystem::emplace<ecs::systems::AnimalAISystem>();
	magic::RegisterFlockSpeciesDying();
}

inline void ResetWorldSystems()
{
	Locator::animalAISystem::reset();
	Locator::landBalanceSystem::reset();
	Locator::treeSystem::reset();
	Locator::forestSystem::reset();
	Locator::fireSoundSystem::reset();
	Locator::fireGraphicSystem::reset();
	Locator::fireEffectSystem::reset();
	Locator::reactionsSystem::reset();
	Locator::mapCellsSystem::reset();
	Locator::objectCreationIndexSystem::reset();
	Locator::toBeDeletedSystem::reset();
}

/// The world lists for one test that has no fixture: emplaced now, reset when it goes out of scope
struct ScopedWorldSystems
{
	ScopedWorldSystems() { EmplaceWorldSystems(); }
	~ScopedWorldSystems() { ResetWorldSystems(); }
	ScopedWorldSystems(const ScopedWorldSystems&) = delete;
	ScopedWorldSystems& operator=(const ScopedWorldSystems&) = delete;
};
} // namespace openblack::test
