/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSight.h"

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The hand looks near to where a creature looks when it is less than a third of a half turn away from it
constexpr uint32_t k_HandSightParts = 3;
} // namespace

GameHeading creature_sight::FacingHeading(const Registry& registry, entt::entity creature)
{
	if (const auto* moving = registry.TryGet<const CreatureLocomotion>(creature); moving != nullptr && moving->started)
	{
		return moving->heading;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return GameHeading::Of(ahead.x, ahead.z);
}

GameHeading creature_sight::LookHeading(const Registry& registry, entt::entity creature)
{
	const auto& position = registry.Get<const Transform>(creature).position;
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	// The game compares the point with the position on all three axes. A point straight above the creature gives no way
	// across the land, thus the heading 0.
	if (animation != nullptr && animation->lookAt.has_value() && *animation->lookAt != position)
	{
		const auto way = *animation->lookAt - position;
		return GameHeading::Of(way.x, way.z);
	}
	return FacingHeading(registry, creature);
}

bool creature_sight::CanSeePos(const Registry& registry, entt::entity creature, glm::vec2 point)
{
	const auto from = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(creature).position));
	const auto to = map_coords::FromMetres(point);
	return creature_object_beliefs::CanSee(LookHeading(registry, creature).GameAngle(), gutils::GetAngleFromXZ(from, to),
	                                       map_coords::Cell(from) == map_coords::Cell(to));
}

CreatureMindState* creature_sight::MindSeeing(PlayerNames player, glm::vec3 point)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::leashSystem::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = Locator::leashSystem::value().PlayersCreature(player);
	auto* mind = creature.has_value() && registry.Valid(*creature) ? registry.TryGet<CreatureMindState>(*creature) : nullptr;
	if (mind == nullptr || !registry.AllOf<Transform>(*creature) || !CanSeePos(registry, *creature, glm::xz(point)))
	{
		return nullptr;
	}
	return mind;
}

void creature_sight::EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float amount, glm::vec3 point)
{
	if (auto* mind = MindSeeing(player, point))
	{
		creature_perceived_desires::Increase(mind->perceivedDesires, static_cast<size_t>(desire), amount);
	}
}

void creature_sight::SeePlayerAction(PlayerNames player, creature_desires::Desire desire)
{
	using Side = systems::HandSystemInterface::Side;
	if (!Locator::entitiesRegistry::has_value() || !Locator::leashSystem::has_value() || !Locator::handSystem::has_value() ||
	    !Locator::camera::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto creature = Locator::leashSystem::value().PlayersCreature(player);
	if (!creature.has_value())
	{
		return;
	}
	// The creature reads the hand of the same player as the camera: its own player
	const auto handPlayer = openblack::ecs::ReportedCreaturePlayer(*creature);
	const auto hand =
	    handPlayer.has_value() ? openblack::ecs::PlayerHandsOf(*handPlayer).at(static_cast<size_t>(Side::Left)) : std::nullopt;
	if (!hand.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(*creature) ? registry.TryGet<const Creature>(*creature) : nullptr;
	const auto* at = body != nullptr ? registry.TryGet<const Transform>(*creature) : nullptr;
	if (at == nullptr)
	{
		return;
	}
	// The player must look at the creature, and the creature must look towards the hand
	const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(*creature);
	if (!camera.has_value() || !creature_attention::IsLookingAt(camera->origin, camera->focus, at->position))
	{
		return;
	}
	// The creature faces the hand when the way to the hand is less than a third of a half turn from the way that it
	// looks, or the hand is in its map cell. The game does not take the difference round the circle.
	const auto from = map_coords::FromMetres(glm::xz(at->position));
	const auto to = map_coords::FromMetres(glm::xz(*hand));
	if (!creature_player_attention::IsDirectionClose(from, to, LookHeading(registry, *creature).GameAngle(), k_HandSightParts))
	{
		return;
	}
	auto* mind = registry.TryGet<CreatureMindState>(*creature);
	if (mind == nullptr || !mind->desires.has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const auto row = creature::InfoRow(body->species);
	const auto index = static_cast<size_t>(desire);
	if (row >= info.creature.size() || index >= info.creatureInitialDesire.size())
	{
		return;
	}
	const auto& species = info.creature.at(row);
	const auto speedUp = info.creatureInitialDesire.at(index).seenPlayerActionSpeedUp * species.seenPlayerActionSpeedUp;
	creature_learning::SpeedUpAfterSeeingPlayer((*mind->desires)[desire], speedUp, species.minDesireIncreaseSeconds,
	                                            species.maxDesireIncreaseSeconds);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} saw the hand: desire {} now grows in {} seconds",
	                    entt::to_integral(*creature), index, (*mind->desires)[desire].increaseSeconds);
}
