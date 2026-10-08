/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Define LOCATOR_IMPLEMENTATIONS before including this header: it makes the services the tests inject"
#endif

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Common/EventManager.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Events/TeleportEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/RestoreService.h"

/// What the creature systems' tests inject: a registry of their own, the object counter, the real resource caches, a
/// value-initialised table of the game's constants and the events, each put back as it was when the test ends. The
/// systems themselves are made by the tests, never reached through the locator.
namespace openblack::test::creature_world
{
class World
{
public:
	World()
	{
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
		Locator::resources::emplace<resources::Resources>();
		auto info = std::make_unique<InfoConstants>();
		_info = info.get();
		Locator::infoConstants::reset(info.release());
		Locator::events::emplace<EventManager>();
		Locator::events::value().AddHandler<ecs::events::Teleported>(
		    [this](const ecs::events::Teleported& event) { teleported.push_back(event.thing); });
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] InfoConstants& Info() { return *_info; }

	/// A creature as the script's creator makes one, at a point and facing along a turn about y
	static entt::entity MakeCreature(glm::vec3 position = glm::vec3(100.0f, 0.0f, 100.0f),
	                                 PlayerNames owner = PlayerNames::PLAYER_ONE, float yAngle = 0.0f, float scale = 1.0f)
	{
		return ecs::archetypes::CreatureArchetype::Create(position, owner, CreatureType::GiantApe,
		                                                  entt::hashed_string("mind").value(), yAngle, scale);
	}

	/// The Giant Ape's rig in the cache, read from a synthetic block whose spec file names the animations of `sets`
	void LoadApeRig(const creature_block::Block& block,
	                const std::vector<std::pair<std::string, std::vector<std::string>>>& sets)
	{
		_folder.emplace(std::string("openblack_test_creature_world_") +
		                ::testing::UnitTest::GetInstance()->current_test_info()->name());
		creature_block::WriteSpec(_folder->Path(), true, block.specVersion, sets);
		Locator::resources::value().GetCreatureRigs().Load(
		    creature::GetRigId(CreatureType::GiantApe), resources::CreatureRigLoader::FromBufferTag {},
		    creature_block::Build(block), _folder->Path(), _folder->Path() / "CreatureMesh");
	}

	std::vector<entt::entity> teleported;

private:
	// put back in reverse order: the events, the tables, the caches, the counter, then the registry
	const RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const RestoreService<Locator::objectCreationIndexSystem> _restoreIndex;
	const RestoreService<Locator::resources> _restoreResources;
	const RestoreService<Locator::infoConstants> _restoreInfo;
	const RestoreService<Locator::events> _restoreEvents;
	InfoConstants* _info {nullptr};
	std::optional<creature_block::TempFolder> _folder;
};
} // namespace openblack::test::creature_world
