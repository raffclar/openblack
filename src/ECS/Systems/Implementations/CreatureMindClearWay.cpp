/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature does when it has no way to where it goes: it refuses and gives up, or it clears a thing out of its way
// and walks on. Also the end of a run away, which sees to the desire only when the creature is far enough away.

#define LOCATOR_IMPLEMENTATIONS

#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Audio/AnimEffectKeys.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureClearWay.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{
/// The actions that clear a thing, and the walk on after
constexpr std::string_view k_ThrowAroundAction = "ThrowAround";
constexpr std::string_view k_KickAction = "Kick";
constexpr std::string_view k_WalkOnAction = "MoveToPos";
/// The height of a creature of size one, in metres
constexpr float k_HeightOfSizeOne = 15.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// Whether a building is the centre of a town
bool IsTownCentre(const ecs::Registry& registry, entt::entity entity)
{
	const auto* abode = registry.TryGet<const Abode>(entity);
	if (abode == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	const auto type = static_cast<size_t>(abode->type);
	return type < infos.size() && infos.at(type).abodeType == AbodeType::TownCentre;
}
} // namespace

bool CreatureMindSystem::ClearWay(entt::entity creature, CreatureMindState& mind, glm::vec2 destination,
                                  std::span<const uint32_t> met)
{
	const auto& registry = Entities();
	const auto* tables = GetTables();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (tables == nullptr || body == nullptr || !mind.learnt.has_value() || mind.clearingWaySerial.has_value() ||
	    !Locator::creatureObjectActionSystem::has_value())
	{
		return false;
	}
	const auto throwAround = creature_mind_tables::FindAction(*tables, k_ThrowAroundAction);
	const auto kick = creature_mind_tables::FindAction(*tables, k_KickAction);
	if (!throwAround.has_value() || !kick.has_value())
	{
		return false;
	}
	const creature_clear_way::Actions actions {.throwAround = *throwAround, .kick = *kick};
	const auto& knowledge = mind.learnt->knowledge;
	std::vector<creature_clear_way::Miracle> miracles;
	miracles.reserve(tables->miracles.size());
	for (size_t type = 0; type < tables->miracles.size(); ++type)
	{
		miracles.push_back({.known = type < knowledge.miraclesKnown.size() && knowledge.miraclesKnown[type],
		                    .desire = tables->miracles[type].desire,
		                    .action = tables->miracles[type].action});
	}
	const auto& hands = Locator::creatureObjectActionSystem::value();
	const auto actionFor = [&](uint32_t thing) -> std::optional<uint32_t> {
		const auto entity = static_cast<entt::entity>(thing);
		if (!registry.Valid(entity))
		{
			return std::nullopt;
		}
		const creature_clear_way::Obstacle obstacle {
		    .templeHeart = registry.AllOf<Temple>(entity),
		    .storagePit = registry.AllOf<StoragePit>(entity),
		    .townCentre = IsTownCentre(registry, entity),
		    .canPickUp = hands.CanPickUp(creature, entity),
		    .canKick = hands.CanKick(creature, entity),
		};
		const auto action = creature_clear_way::ClearingAction(obstacle, miracles, actions);
		return action.has_value() && *action < tables->actions.size() ? action : std::nullopt;
	};
	const auto kept =
	    creature_clear_way::ThingsToClear(met, [&actionFor](uint32_t thing) { return actionFor(thing).has_value(); });
	const auto chosen = creature_clear_way::Choose(kept.size(), [this](uint32_t range) { return Random(range); });
	if (!chosen.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is shut in and can clear none of the {} things that it met",
		                    entt::to_integral(creature), met.size());
		return false;
	}
	const auto thing = static_cast<entt::entity>(kept[*chosen]);
	const auto action = actionFor(kept[*chosen]);
	if (!action.has_value() ||
	    !ForcePlan(creature, {.desire = Desire::ObeyPlayer, .action = tables->actions[*action].name, .object = thing}))
	{
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is shut in and clears {} out of its way: {}",
	                   entt::to_integral(creature), entt::to_integral(thing), tables->actions[*action].name);
	mind.clearingWaySerial = mind.planSerial;
	mind.walkOnTo = destination;
	mind.followUp = creature_follow_up::FollowUp::WalkOn;
	return true;
}

void CreatureMindSystem::WalkOn(entt::entity creature, const CreatureMindState& mind)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	const auto height = k_HeightOfSizeOne * ShownSize(*body);
	static_cast<void>(ForcePlan(creature, {.desire = Desire::ObeyPlayer,
	                                       .action = k_WalkOnAction,
	                                       .object = creature,
	                                       .agenda = creature_clear_way::WalkOn(mind.walkOnTo, height)}));
}

void CreatureMindSystem::StepClearingWay(entt::entity creature, CreatureMindState& mind)
{
	// The mark stays until the creature stops the plan that clears the way
	if (mind.clearingWaySerial.has_value() && (!mind.planActive || mind.planSerial != *mind.clearingWaySerial))
	{
		mind.clearingWaySerial.reset();
	}
	if (auto* walk = Entities().TryGet<CreatureLocomotion>(creature))
	{
		walk->clearingWay = mind.clearingWaySerial.has_value();
	}
}

void CreatureMindSystem::RefuseNoRoute(entt::entity creature)
{
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().Play(
		    creature,
		    {.kind = creature_audio::EventKind::Voice, .timeMs = 0, .action = audio::SoundAction::Refusal, .mode = 0});
	}
}

void CreatureMindSystem::EndRunAway(entt::entity creature, CreatureMindState& mind, std::optional<uint32_t> object)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (body == nullptr || at == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	if (row >= creatures.size())
	{
		return;
	}
	// From a thing, it measures to where it believes the thing is. From the player, it measures to the nearest hand
	// of its player.
	std::optional<glm::vec3> from;
	if (object.has_value())
	{
		if (const auto* belief = mind.beliefs.Find(static_cast<entt::entity>(*object)))
		{
			from = belief->place;
		}
	}
	else if (Locator::handSystem::has_value() && Locator::playerSystem::has_value() &&
	         body->owner == Locator::playerSystem::value().GetLocalPlayer())
	{
		for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
		{
			if (hand.has_value() &&
			    (!from.has_value() || glm::distance(*hand, at->position) < glm::distance(*from, at->position)))
			{
				from = hand;
			}
		}
	}
	if (from.has_value() &&
	    creature_emote::RanFarEnough(gutils::GetDistanceInMetres(at->position, *from), creatures.at(row).runAwayDistance))
	{
		mind.desireSeenTo = true;
	}
}
