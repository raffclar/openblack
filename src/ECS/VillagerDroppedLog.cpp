/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDroppedLog.h"

#include <numbers>

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerHeldObject.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/Body.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

entt::entity villager_dropped_log::Drop(systems::DynamicsSystemInterface& dynamics, entt::entity villager,
                                        const systems::DroppedLogStart& start)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* person = registry.TryGet<Villager>(villager);
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (person == nullptr || transform == nullptr)
	{
		return entt::null;
	}
	const auto& constants = Locator::infoConstants::value();
	const auto& info = constants.villager.at(static_cast<size_t>(GVillagerInfo::Find(person->tribe, person->number)));
	if (!villager_held::DropsLog(person->carried, person->woodHeld, info.minWoodToShowGraphic))
	{
		return entt::null;
	}
	// A pine's trunk, a quarter turn over so that it lies along the ground, drawn as what the villager held, whatever it
	// was; a tool too
	const auto model = *villager_held::MeshOf(person->carried);
	const auto lying = glm::mat3(glm::eulerAngleX(-std::numbers::pi_v<float> / 2.0f));
	const auto log = archetypes::DeadTreeArchetype::Create(transform->position, TreeInfo::Pine, lying, 1.0f, model);
	const auto& pine = constants.tree.at(static_cast<size_t>(TreeInfo::Pine));
	registry.Get<DeadTree>(log).woodMultiplier =
	    villager_held::LogWoodMultiplier(person->woodHeld, static_cast<float>(pine.woodValue));

	const auto started = dynamics.InitialisePhysics(log, {.velocity = start.velocity,
	                                                      .spin = start.spin,
	                                                      .thrower = entt::null,
	                                                      .player = std::nullopt,
	                                                      .add = true,
	                                                      .fromHand = false});
	if (started.entry != nullptr)
	{
		// Let go by a hand it turns as the hand turned the villager
		if (start.angularMomentum.has_value() && started.entry->body != nullptr)
		{
			started.entry->body->angularMomentum = *start.angularMomentum;
		}
		// It knocks nothing over and hurts nobody on its way
		started.entry->flags |= PhysicsEntry::k_NoObjectCollision;
		dynamics.SettleOnLand(*started.entry, false, true);
		dynamics.RaiseClearOfWhatIsUnder(*started.entry);
	}

	const auto dropped = villager_carry::PutDown(*person, ResourceType::Wood, 0);
	if (auto* tally = registry.Valid(person->town) ? registry.TryGet<TownResourceTally>(person->town) : nullptr;
	    tally != nullptr)
	{
		tally->woodCarried -= static_cast<float>(dropped);
	}
	return log;
}
