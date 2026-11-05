/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the actions of a plan know about the world when they start: the places, the player and the home of the creature

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureActionHistory.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAgendaArea.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMusicReaction.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureTownTheft.h"
#include "Creature/CreatureTraining.h"
#include "Creature/CreatureTurnOrder.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
#include "CreatureMindFootball.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/FixedKinds.h"
#include "ECS/LearningMarker.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TownDesire.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Graphics/Moon.h"
#include "Gui/TextDatabase.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/MusicMood.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::mind_detail;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using creature_plan_actions::Target;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

constexpr float k_HeightOfSizeOne = 15.0f;

/// Where the altar stands of the worship site that a creature of the player takes a sacrifice to: the site in the first
/// place around the player's temple that has a site. None when the player has no temple, or the temple has no site.
std::optional<glm::vec3> SacrificeAltarOf(const ecs::Registry& registry, PlayerNames player)
{
	std::optional<glm::vec3> altar;
	registry.Each<const Temple, const CitadelWorship>([&](const Temple& temple, const CitadelWorship& worship) {
		if (temple.owner != player || altar.has_value())
		{
			return;
		}
		std::array<bool, CitadelWorship::k_Places> placeHasSite {};
		std::ranges::transform(worship.sites, placeHasSite.begin(),
		                       [&registry](entt::entity site) { return site != entt::null && registry.Valid(site); });
		const auto place = creature_sacrifice::SitePlace(placeHasSite);
		if (!place.has_value())
		{
			return;
		}
		const auto* site = registry.TryGet<const WorshipSite>(worship.sites.at(*place));
		const auto* at =
		    site != nullptr && registry.Valid(site->altar) ? registry.TryGet<const Transform>(site->altar) : nullptr;
		if (at != nullptr)
		{
			altar = at->position;
		}
	});
	return altar;
}

/// The storage pit of the nearest town of a player to a place, by the distance across the land. The towns come in the
/// order of the player's list, so of two towns at the same distance the first in the list is the nearest. None when the
/// player has no town, or when the nearest town has no storage pit.
std::optional<uint32_t> OwnTownPitOf(const ecs::Registry& registry, PlayerNames player, glm::vec3 from)
{
	std::vector<entt::entity> towns;
	std::vector<float> metres;
	for (const auto town : ecs::player_towns::InOrder(registry, player))
	{
		if (const auto* at = registry.TryGet<const Transform>(town))
		{
			towns.push_back(town);
			metres.push_back(gutils::GetDistanceInMetres(at->position, from));
		}
	}
	const auto nearest = creature_town_theft::NearestTown(metres);
	const auto pit = nearest.has_value() ? ecs::construction::StoragePitOf(towns.at(*nearest)) : std::nullopt;
	return pit.has_value() ? std::optional(entt::to_integral(*pit)) : std::nullopt;
}
} // namespace

void CreatureMindSystem::Situate(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	situation.self = entt::to_integral(creature);
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature); mind != nullptr)
	{
		situation.stage = mind->developmentPhase;
	}
	// Every agenda that faces the player can be made: also when a plan starts again after a hold, or is forced
	if (const auto camera = openblack::ecs::ReportedCreatureCamera(creature))
	{
		situation.eye = camera->origin;
	}
	// The steps that look, turn or walk to the camera need a creature with a player
	if (const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature))
	{
		situation.camera = glm::vec2(camera->origin.x, camera->origin.z);
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL;
	situation.controlledByScript = registry.AllOf<ScriptControlled>(creature);
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	situation.healthyEnoughToFight = creature_fight::HealthyEnoughToFight(needs != nullptr ? needs->needs.life : 1.0f);
	situation.radius = object_measures::TwoDRadius(registry, creature);
	situation.height = body != nullptr ? k_HeightOfSizeOne * ShownSize(*body) : 0.0f;
	const auto held = Locator::creatureObjectActionSystem::has_value()
	                      ? Locator::creatureObjectActionSystem::value().GetCarried(creature)
	                      : std::nullopt;
	situation.handFull = held.has_value();
	if (held.has_value())
	{
		situation.holdingEatable = Locator::creatureObjectActionSystem::value().CanEatHeld(creature);
		situation.heldOneShot = OneShotOf(registry, *held);
		const auto thing = Locator::creatureObjectActionSystem::value().KindOf(*held).thing;
		const auto* mobile = registry.TryGet<const MobileObject>(*held);
		situation.holdingTree = thing == creature_object_actions::Thing::Tree;
		situation.holdingLiving = creature_object_actions::IsLiving(thing);
		situation.holdingRock = creature_object_actions::IsRock(thing);
		situation.holdingMushroom = thing == creature_object_actions::Thing::MagicMushroom ||
		                            (mobile != nullptr && (mobile->type == MobileObjectInfo::MagicMushroom ||
		                                                   mobile->type == MobileObjectInfo::Toadstool));
	}
	if (const auto* at = registry.TryGet<const Transform>(creature))
	{
		situation.position = {at->position.x, at->position.z};
		// The nearest hand of the creature's own player, which it may throw to
		if (body != nullptr)
		{
			situation.playerHand = openblack::ecs::NearestHandOf(body->owner, at->position);
		}
	}
	if (const auto* transform = registry.TryGet<const Transform>(creature); transform != nullptr && situation.eye.has_value())
	{
		situation.eyeDistance = x87::Length<x87::SumOrder::Zyx>(*situation.eye - transform->position);
	}
	situation.hangAround = HangAroundPlacesOf(creature);
	situation.turnSeconds = k_TurnSeconds;
	situation.turnsPerSecond = k_TurnsPerSecond;
	SituatePlayer(creature, situation);
	SituateHome(creature, situation);
	SituateEmotes(creature, situation);
	SituateFriends(creature, situation);
	if (body != nullptr)
	{
		situation.sacrificeAltar = SacrificeAltarOf(registry, body->owner);
		if (const auto* at = registry.TryGet<const Transform>(creature))
		{
			situation.ownTownPit = OwnTownPitOf(registry, body->owner, at->position);
		}
	}
}

void CreatureMindSystem::SituatePlayer(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* at = registry.TryGet<const Transform>(creature);
	// Only the camera of the creature's own player looks at it here
	const auto camera = openblack::ecs::ReportedCameraOfItsPlayer(creature);
	if (at != nullptr && camera.has_value())
	{
		situation.lookedAt = creature_attention::IsLookingAt(camera->origin, camera->focus, at->position);
	}
	// The hand moves while the velocity of the hand of its player is more than 0
	if (const auto* body = registry.TryGet<const Creature>(creature); body != nullptr)
	{
		situation.handMoving = situation.playerHand.has_value() &&
		                       creature_player_attention::IsHandMoving(openblack::ecs::PlayerHandVelocityOf(body->owner));
	}
	if (at != nullptr && situation.playerHand.has_value() && camera.has_value())
	{
		situation.awayFromHand =
		    creature_player_attention::AwayFromHand(at->position, *situation.playerHand, camera->origin, camera->focus);
	}
}

void CreatureMindSystem::SituateEmotes(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (Locator::skyClockSystem::has_value())
	{
		const auto& clock = Locator::skyClockSystem::value().GetClock();
		situation.night = clock.IsVisualNight();
		// The creature looks for the moon at the place that the sky draws it at from the camera, taken as a place on
		// the land
		situation.moon = graphics::moon::Offset(clock.GetScriptTime());
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	situation.evil = body != nullptr && body->alignment < 0.0f;
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	situation.leashed = leashes != nullptr && leashes->worn.has_value();
	const auto* at = registry.TryGet<const Transform>(creature);
	if (situation.playerHand.has_value() && at != nullptr)
	{
		situation.handDistance = gutils::GetDistance(*situation.playerHand, at->position);
	}
	if (const auto* mind = registry.TryGet<const CreatureMindState>(creature))
	{
		situation.attitudeToPlayer = mind->attitudeToPlayer;
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetCarried(creature))
		{
			const auto life = magic_living::LifeOf(*held);
			situation.holdingStrokeable =
			    life.has_value() && *life < 1.0f && Locator::creatureObjectActionSystem::value().CanPickUp(creature, *held);
		}
	}
}

std::optional<glm::vec2> CreatureMindSystem::EmoteClearArea(entt::entity creature,
                                                            const creature_plan_actions::Executor& executor, glm::vec2 point,
                                                            const creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	switch (executor.build)
	{
	case creature_plan_actions::Build::Sleep:
		// Sleeping on the spot, it looks for a clear place about itself four times as wide as it is tall; one not under a
		// script's control also wants nothing living there
		return ClearAreaFor(registry, creature, situation.position, creature_emote::k_SleepOnTheSpotHeights * situation.height,
		                    !situation.controlledByScript);
	case creature_plan_actions::Build::SitDown:
		// Sitting down, it looks for a clear place about the thing four times as wide as its own radius
		return ClearAreaFor(registry, creature, point, creature_emote::k_SitDownRadii * situation.radius, false);
	default:
		return std::nullopt;
	}
}

void CreatureMindSystem::NoteDone(CreatureMindState& mind, uint32_t action)
{
	if (mind.lastDoneTurn.size() <= action)
	{
		mind.lastDoneTurn.resize(static_cast<size_t>(action) + 1, 0);
	}
	mind.lastDoneTurn.at(action) = TurnNow();
}

void CreatureMindSystem::EndUnderway(CreatureMindState& mind)
{
	const auto* tables = GetTables();
	if (!mind.underway.has_value() || !mind.learnt.has_value() || tables == nullptr ||
	    mind.underway->action >= tables->actions.size())
	{
		return;
	}
	const auto desire = tables->actions.at(mind.underway->action).desire;
	const auto list = desire < tables->desireActions.size() ? std::span<const uint32_t>(tables->desireActions.at(desire))
	                                                        : std::span<const uint32_t> {};
	creature_action_history::PlanEnded(mind.learnt->turnsSinceDone, mind.underway->action, list, mind.agendaTurns);
}

void CreatureMindSystem::BelieveInTempleToEnter(entt::entity creature, const creature_plan_actions::Executor& executor,
                                                const creature_plan_actions::Situation& situation)
{
	// As it sets off into its temple, the creature comes to believe in the temple's heart. As it sets off to take a
	// stolen thing to a town, it comes to believe in the storage pit of that town.
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	if (executor.build == creature_plan_actions::Build::EnterTemple && situation.templeHeart.has_value())
	{
		const auto temple = static_cast<entt::entity>(situation.templeHeart->temple);
		const auto heart = ecs::temple_heart::Of(Locator::entitiesRegistry::value(), temple);
		ComeAcross(creature, *mind, heart != entt::null ? heart : temple);
	}
	else if (executor.build == creature_plan_actions::Build::StealAndPutInTown && situation.ownTownPit.has_value())
	{
		ComeAcross(creature, *mind, static_cast<entt::entity>(*situation.ownTownPit));
	}
}

void CreatureMindSystem::SituateHome(entt::entity creature, creature_plan_actions::Situation& situation)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (body == nullptr || at == nullptr)
	{
		return;
	}
	situation.standing = at->position;
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		situation.home = glm::vec2(home->x, home->z);
		situation.homeDistance = gutils::GetDistanceInMetres(situation.position, *situation.home);
	}
	if (const auto home = ecs::creature_home::HomeOnMapOf(registry, creature))
	{
		situation.homeCellDistance = creature_plan_actions::HomeCellDistance(map_coords::FromMetres(situation.position), *home);
	}
	// The heart of its player's temple is the temple itself, on the ground
	registry.Each<const Temple, const Transform>([&](entt::entity temple, const Temple& building, const Transform& place) {
		if (building.owner != body->owner || situation.templeHeart.has_value())
		{
			return;
		}
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt({place.position.x, place.position.z})
		                        : place.position.y;
		situation.templeHeart = creature_plan_actions::Situation::TempleHeart {
		    .temple = entt::to_integral(temple),
		    .point = {place.position.x, ground, place.position.z},
		    .radius = object_measures::TwoDRadius(registry, temple),
		};
	});
	situation.hasTemple = situation.templeHeart.has_value();
	// Every other creature counts, of any player, dead or alive
	std::vector<float> others;
	others.reserve(registry.Size<Creature>());
	registry.Each<const Creature, const Transform>([&](entt::entity other, const Creature&, const Transform& place) {
		if (other != creature)
		{
			others.push_back(gutils::GetDistanceInMetres(place.position, at->position));
		}
	});
	situation.nothingFrighteningNear = creature_mind::NothingFrighteningNear(situation.height, others);
}

creature_mind::HangAroundPlaces CreatureMindSystem::HangAroundPlacesOf(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto onGround = [](glm::vec3 point) {
		const auto ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({point.x, point.z}) : point.y;
		return glm::vec3(point.x, ground, point.z);
	};
	creature_mind::HangAroundPlaces places;
	const auto* body = registry.TryGet<const Creature>(creature);
	places.height = body != nullptr ? k_HeightOfSizeOne * ShownSize(*body) : 0.0f;
	// TODO(creature-hang-around): the game keeps the place where a creature without a temple was made as its home.
	// openblack does not keep that place, so a creature without a home hangs around where it stands.
	if (const auto home = ecs::creature_home::HomeOf(registry, creature))
	{
		places.home = onGround(*home);
	}
	else if (const auto* at = registry.TryGet<const Transform>(creature))
	{
		places.home = onGround(at->position);
	}
	if (const auto* leash = registry.TryGet<const CreatureLeash>(creature))
	{
		places.confinementRadius = leash->confinementRadius;
	}
	if (body != nullptr)
	{
		registry.Each<const Temple, const Transform>([&](entt::entity, const Temple& temple, const Transform& transform) {
			if (temple.owner == body->owner && !places.temple.has_value())
			{
				places.temple = onGround(transform.position);
			}
		});
	}
	return places;
}

bool CreatureMindSystem::HasPlayed(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	return mind == nullptr || creature_script_play::Played(mind->idle);
}

void CreatureMindSystem::ClearAfterPlayed(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr; mind != nullptr)
	{
		creature_script_play::ClearAfterPlayed(mind->idle);
	}
}
