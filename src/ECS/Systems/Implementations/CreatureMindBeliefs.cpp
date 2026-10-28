/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature comes to believe about the things about it: each turn its eyes sweep a few of the map cells about it
// and it notes every thing in the cells it can see; going into another cell it notes the nearest temple; each new
// belief leads it on to what the thing belongs to (a tree's forest, an animal's flock, a house's town). Beliefs about
// things gone from the world are forgotten each turn.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <chrono>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/BirdRules.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureLostThing.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreatureSteal.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/Map.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/TownDesire.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace beliefs = openblack::creature_object_beliefs;
namespace kinds = openblack::creature_tree::belief_types;
using openblack::creature_desires::Desire;

namespace
{
/// The kind of belief for a thing that lies about and can move, by the kind of thing that its data row says it is: poo,
/// magic food, or any other thing
uint32_t KindOfLooseThing(MobileObjectInfo type)
{
	switch (type)
	{
	case MobileObjectInfo::LumpOfPoo:
		return kinds::k_Poo;
	case MobileObjectInfo::MagicFood:
		return kinds::k_MagicFood;
	default:
		return kinds::k_MobileObject;
	}
}

/// Where a thing is: where it stands, or a flock's own place
glm::vec3 PlaceOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* at = registry.TryGet<const Transform>(thing))
	{
		return at->position;
	}
	if (const auto* flock = registry.TryGet<const Flock>(thing))
	{
		const auto metres = map_coords::ToMetres(flock->place);
		return {metres.x, flock->place.altitude, metres.y};
	}
	return glm::vec3(0.0f);
}

/// How tall a creature is at size 1
constexpr float k_CreatureHeightOfSizeOne = 15.0f;

/// Whether a thing moved in its last game turn, as a creature sees it:
/// - A creature moves while it walks to a place or to a thing.
/// - A firefly always moves.
/// - A thing in the air moves.
/// - An animal or a villager moves when its last turn took it across the land.
/// - Nothing else moves.
bool MovedLastTurn(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Creature>(thing))
	{
		const auto* walk = registry.TryGet<const CreatureLocomotion>(thing);
		return walk != nullptr && walk->destination.has_value() && !walk->facingOnly;
	}
	if (registry.AnyOf<Firefly, InPhysics>(thing))
	{
		return true;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return glm::xz(animal->position) != glm::xz(animal->previousPosition);
	}
	if (const auto* pose = registry.TryGet<const VillagerPose>(thing))
	{
		return pose->movedLastTurn;
	}
	return false;
}

/// How tall a thing is: a creature by its size, anything else by its model as the thing is scaled, and 0 without a model.
/// A temple's heart has the model of its temple.
float HeightOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* heart = registry.TryGet<const ecs::components::TempleHeart>(thing))
	{
		return registry.Valid(heart->temple) ? HeightOf(registry, heart->temple) : 0.0f;
	}
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		return k_CreatureHeightOfSizeOne * ShownSize(*creature);
	}
	const auto* transform = registry.TryGet<const Transform>(thing);
	// An altar has its model's size from when its site is made, before the model shows
	const auto meshId = ecs::thing_model::ModelOf(registry, thing);
	if (transform == nullptr || !meshId.has_value() || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(*meshId))
	{
		return 0.0f;
	}
	return meshes.Handle(*meshId)->GetBoundingBox().Size().y * transform->scale.y;
}

/// The game turns in one second
constexpr uint32_t k_TurnsPerSecond =
    static_cast<uint32_t>(std::chrono::milliseconds(std::chrono::seconds(1)) / TimeSystemInterface::k_TurnDuration);

/// Whether a building is the centre of a town
bool IsTownCentre(const Abode& abode)
{
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	const auto row = static_cast<size_t>(abode.info);
	return row < infos.size() && infos.at(row).abodeType == AbodeType::TownCentre;
}

/// How much a thing catches a creature's eye, by what it is. The temple, fields, features and the other things that
/// stand on the land are fixed things.
creature_look::Interest LookInterestOf(const ecs::Registry& registry, entt::entity thing)
{
	using Interest = creature_look::Interest;
	if (registry.AllOf<Creature>(thing))
	{
		return Interest::Creature;
	}
	if (registry.AllOf<Villager>(thing))
	{
		return Interest::Villager;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return animals::birds::InterestsACreatureAsADove(animal->type) ? Interest::Dove : Interest::Animal;
	}
	if (registry.AllOf<Abode>(thing))
	{
		return Interest::Abode;
	}
	if (registry.AllOf<Tree>(thing))
	{
		return Interest::Tree;
	}
	if (registry.AllOf<MobileStatic>(thing))
	{
		return Locator::infoConstants::has_value() &&
		               ecs::physics_classes::IsToy(registry, thing, Locator::infoConstants::value())
		           ? Interest::Toy
		           : Interest::Static;
	}
	if (registry.AllOf<MobileObject>(thing))
	{
		return Interest::None;
	}
	return Interest::Fixed;
}

/// The temple nearest a point, of every player's in turn and then the neutral one's; the first of equals
entt::entity NearestTemple(const ecs::Registry& registry, const map_coords::MapCoords& from)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return entt::null;
	}
	entt::entity nearest = entt::null;
	auto within = std::numeric_limits<float>::max();
	for (uint8_t player = 0; player < static_cast<uint8_t>(PlayerNames::_COUNT); ++player)
	{
		const auto temple = Locator::worshipSiteSystem::value().TempleOf(static_cast<PlayerNames>(player));
		const auto* at = temple != entt::null ? registry.TryGet<const Transform>(temple) : nullptr;
		if (at == nullptr)
		{
			continue;
		}
		const auto distance = gutils::GetDistanceInMetres(from, map_coords::FromMetres(glm::xz(at->position)));
		if (distance < within)
		{
			nearest = temple;
			within = distance;
		}
	}
	return nearest;
}
} // namespace

uint16_t mind_detail::LookingAngle(const Transform& transform, const std::optional<glm::vec3>& lookAt)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	const auto direction = lookAt.has_value() && *lookAt != transform.position ? *lookAt - transform.position : ahead;
	// Turned as the game keeps a creature's turn, a quarter turn on from the map's, and nothing for no direction across
	// the land
	const float scawen = x87::RoundToFloat(x87::GameHeadingOf(direction.x, direction.z));
	return static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(scawen));
}

std::optional<uint32_t> mind_detail::ObjectSortOf(const ecs::Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing) || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const GObjectInfo* info = nullptr;
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		const auto& animals = Locator::infoConstants::value().animal;
		const auto row = static_cast<size_t>(animal->type);
		info = row < animals.size() ? &animals.at(row) : nullptr;
	}
	else if (registry.AllOf<OneOffSpellSeed>(thing))
	{
		// A one-shot globe has the row of the mobile objects for globes
		info = &Locator::infoConstants::value().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed));
	}
	else
	{
		info = ecs::world_objects::InfoOf(thing);
	}
	return info != nullptr ? std::optional(static_cast<uint32_t>(info->type)) : std::nullopt;
}

std::optional<uint32_t> mind_detail::BeliefKindOf(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Temple>(thing))
	{
		// The temple here is one thing: the game's temple and the building that stands for it
		return kinds::k_Citadel;
	}
	if (registry.AllOf<ForestTurns>(thing))
	{
		return kinds::k_Forest;
	}
	if (registry.AllOf<Flock>(thing))
	{
		return kinds::k_Flock;
	}
	if (registry.AllOf<Town>(thing))
	{
		return kinds::k_Town;
	}
	if (registry.AllOf<Creature>(thing))
	{
		return kinds::k_Creature;
	}
	if (registry.AllOf<Villager>(thing))
	{
		return kinds::k_Villager;
	}
	if (registry.AllOf<Animal>(thing))
	{
		return kinds::k_Animal;
	}
	if (registry.AllOf<Tree>(thing))
	{
		return kinds::k_Tree;
	}
	// A temple's heart, a worship site and the altar at its head are parts of their player's temple
	if (registry.AnyOf<TempleHeart, WorshipSite, WorshipAltar>(thing))
	{
		return kinds::k_CitadelPart;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		// A football pitch is a feature
		return abode->type == AbodeNumber::FootballPitch ? kinds::k_Feature : kinds::k_Abode;
	}
	if (registry.AnyOf<Field, FishFarm>(thing))
	{
		return kinds::k_Field;
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return kinds::k_TotemStatue;
	}
	if (registry.AnyOf<Feature, AnimatedStatic, Flowers>(thing))
	{
		return kinds::k_Feature;
	}
	if (registry.AllOf<Pot>(thing))
	{
		return kinds::k_Pot;
	}
	if (registry.AllOf<Rock>(thing))
	{
		return kinds::k_Rock;
	}
	if (registry.AnyOf<MobileStatic, DeadTree>(thing))
	{
		return kinds::k_MobileObject;
	}
	if (const auto* loose = registry.TryGet<const MobileObject>(thing))
	{
		return KindOfLooseThing(loose->type);
	}
	if (registry.AllOf<Ball>(thing) && Locator::infoConstants::has_value())
	{
		return KindOfLooseThing(Locator::infoConstants::value().ball.mobileType);
	}
	// A miracle's seed and a puff of mist are of one kind
	if (registry.AnyOf<SpellSeed, Mist>(thing))
	{
		return kinds::k_SpellSeed;
	}
	// A one-shot globe is a loose thing of the kind of its row of the loose things
	if (registry.AllOf<OneOffSpellSeed>(thing) && Locator::infoConstants::has_value())
	{
		return KindOfLooseThing(
		    Locator::infoConstants::value().mobileObject.at(static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed)).mobileType);
	}
	// A stone hand on a village centre is the icon of a miracle
	if (registry.AllOf<TownCentreIcon>(thing))
	{
		return kinds::k_SpellIcon;
	}
	if (registry.AllOf<ecs::components::Spell>(thing))
	{
		return kinds::k_Spell;
	}
	return std::nullopt;
}

const GWorshipSiteInfo* mind_detail::AltarInfoOf(const ecs::Registry& registry, entt::entity altar)
{
	const auto* part = registry.Valid(altar) ? registry.TryGet<const WorshipAltar>(altar) : nullptr;
	const auto* site = part != nullptr && registry.Valid(part->site) ? registry.TryGet<const WorshipSite>(part->site) : nullptr;
	if (site == nullptr || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& sites = Locator::infoConstants::value().worshipSite;
	const auto row = static_cast<size_t>(site->tribe);
	return row < sites.size() ? &sites.at(row) : nullptr;
}

void CreatureMindSystem::ComeAcross(entt::entity creature, components::CreatureMindState& mind, entt::entity thing)
{
	++_comeAcrossCount;
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	// The last moving thing that it comes across is the one that it noticed last, whatever the thing is
	if (creature_look::Notices(thing == creature, MovedLastTurn(registry, thing)))
	{
		// An animal is drawn between its last two turns: the creature notices where the animal is now
		const auto* animal = registry.TryGet<const Animal>(thing);
		const auto at = animal != nullptr ? animal->position : PlaceOf(registry, thing);
		mind.noticedMover = {.at = glm::xz(at), .height = HeightOf(registry, thing), .turn = mind.turn};
	}
	const auto kind = mind_detail::BeliefKindOf(registry, thing);
	if (!kind.has_value())
	{
		return;
	}
	const auto added = mind.beliefs.Add(thing, *kind, ecs::thing_map_place::Of(registry, thing));
	// The belief takes in the attributes of the thing now
	if (added.belief != nullptr)
	{
		if (const auto now = mind_detail::BeliefNowOf(registry, thing, creature))
		{
			beliefs::TakeIn(*added.belief, added.isNew, *now);
		}
	}
	if (!added.isNew && added.belief != nullptr)
	{
		SeeAgain(creature, mind, *added.belief);
	}
	// A new belief leads it on to what the thing belongs to, but for another creature, which is its own
	if (!added.isNew || *kind == kinds::k_Creature)
	{
		return;
	}
	// The belief about the thing is then joined to the belief about the whole thing, when the creature has one
	if (const auto owner = BelongsTo(thing); owner.has_value() && *owner != thing)
	{
		ComeAcross(creature, mind, *owner);
		if (mind.beliefs.Find(*owner) != nullptr)
		{
			mind.beliefs.JoinTo(thing, *owner);
		}
	}
}

void CreatureMindSystem::ComeAcrossWithWhole(entt::entity creature, components::CreatureMindState& mind, entt::entity thing)
{
	ComeAcross(creature, mind, thing);
	if (const auto whole = BelongsTo(thing); whole.has_value() && *whole != thing)
	{
		ComeAcross(creature, mind, *whole);
	}
}

void CreatureMindSystem::SeeAgain(entt::entity creature, components::CreatureMindState& mind, beliefs::Belief& belief)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// A building seen again does not stir the desires. A seen village centre leads the creature on to its town.
	if (!beliefs::TakesInAgain(belief.kind))
	{
		if (const auto* abode = registry.TryGet<const Abode>(belief.thing); abode != nullptr && IsTownCentre(*abode))
		{
			if (const auto town = BelongsTo(belief.thing); town.has_value() && *town != belief.thing)
			{
				ComeAcross(creature, mind, *town);
			}
		}
		return;
	}
	// What it does next can come across other things, so it keeps what it needs of the belief first
	const auto kind = belief.kind;
	const auto thing = belief.thing;
	StirSourcesOnSighting(creature, mind, belief);
	// Seen again, a villager may show the creature a skill
	if (kind == kinds::k_Villager)
	{
		WatchVillager(creature, mind, thing);
	}
	// A town that wants something stirs compassion
	if (kind == kinds::k_Town && mind.desires.has_value())
	{
		const auto* wants = registry.TryGet<const TownDesire>(thing);
		const bool wantsSomething = wants != nullptr && ecs::town_desire::GetMostSignificantRawDesire(*wants, 0.0f) != -1;
		if (creature_desire_sources::TownSightingStirs(registry.AllOf<ScriptControlled>(creature), wantsSomething))
		{
			creature_desires::ChangeSource(*mind.desires, creature_desire_sources::k_TownSightingSource,
			                               creature_desire_sources::TownSightingAmount(k_TurnsPerSecond));
		}
	}
}

void CreatureMindSystem::StirSourcesOnSighting(entt::entity creature, components::CreatureMindState& mind,
                                               beliefs::Belief& belief)
{
	if (!mind.desires.has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	auto& desires = *mind.desires;
	// A whole thing it may act on, other than itself, stirs the sources whose desires it is useful for
	if (beliefs::InListOfWholeThings(belief.kind) && belief.thing != creature &&
	    creature_desire_sources::SightingStirsSources(mind.turn, belief.sourcesTurn))
	{
		belief.sourcesTurn = mind.turn;
		for (const auto source : creature_desire_sources::k_SightingSources)
		{
			const auto desire = creature_desires::DesireOfSource(desires, source);
			if (desire.has_value() &&
			    ActivityUsefulness(creature, mind, *desire, belief.thing) > creature_desire_sources::k_SightingUsefulness)
			{
				creature_desires::ChangeSource(desires, source, creature_desire_sources::k_SightingSourceAmount);
			}
		}
	}
	// A toy stirs its wish to play
	if (Locator::infoConstants::has_value() &&
	    ecs::physics_classes::IsToy(registry, belief.thing, Locator::infoConstants::value()))
	{
		creature_desires::ChangeSource(desires, creature_desires::sources::k_PlayFromWatchingPlayer,
		                               creature_desire_sources::k_ToySightingAmount);
	}
	// A thing that it can steal stirs its wish to steal
	if (CanSteal(creature, belief.thing))
	{
		creature_desires::ChangeSource(desires, creature_steal::k_StealSource, creature_steal::k_StealableSightingAmount);
	}
	// Another creature that does something with it in mind gives it attention
	if (registry.AllOf<Creature>(belief.thing))
	{
		NoteAttention(creature, mind, belief.thing);
	}
}

bool CreatureMindSystem::CanSteal(entt::entity creature, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
	if (body == nullptr || transform == nullptr || !Locator::infoConstants::has_value() ||
	    !Locator::creatureObjectActionSystem::has_value() || !Locator::influenceSystem::has_value())
	{
		return false;
	}
	return creature_steal::IsStealable({
	    .toy = ecs::physics_classes::IsToy(registry, thing, Locator::infoConstants::value()),
	    // openblack has no scaffolds yet
	    .scaffold = false,
	    .oneShot = registry.AllOf<OneOffSpellSeed>(thing),
	    .canPickUp = Locator::creatureObjectActionSystem::value().CanPickUp(creature, thing),
	    .ownInfluence = Locator::influenceSystem::value().PlayerRawInfluence(body->owner, transform->position),
	});
}

std::optional<map_coords::MapCoords> CreatureMindSystem::BelievedPlace(const components::CreatureMindState& mind,
                                                                       entt::entity thing)
{
	if (const auto* known = mind.beliefs.Find(thing))
	{
		return known->place;
	}
	return std::nullopt;
}

void CreatureMindSystem::ForgetGoneThings(entt::entity creature, components::CreatureMindState& mind)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& idle = mind.idle;
	// When a thing that the creature uses goes before the creature completes the work, the creature stops and decides again
	creature_lost_thing::InUse inUse {.stepThings = creature_lost_thing::StepThings(idle.agenda, idle.step)};
	if (mind.planActive && mind.planner.current.has_value() && idle.step < idle.agenda.size())
	{
		inUse.planThings = creature_lost_thing::PlanThings(*mind.planner.current);
	}
	auto* held = registry.TryGet<CreatureHeldObject>(creature);
	if (held != nullptr)
	{
		inUse.held = entt::to_integral(held->object);
	}
	if (const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	    action != nullptr && action->kind == creature_object_actions::Kind::Eat && action->target.has_value())
	{
		inUse.usedByItself.push_back(entt::to_integral(*action->target));
	}
	if (idle.usedUpHeld.has_value())
	{
		inUse.usedByItself.push_back(*idle.usedUpHeld);
	}
	const auto gone = [&registry](uint32_t thing) { return !registry.Valid(static_cast<entt::entity>(thing)); };
	const auto notThere = [&registry, &gone](uint32_t thing) {
		return gone(thing) || registry.AllOf<VillagerDeath>(static_cast<entt::entity>(thing));
	};
	if (creature_lost_thing::Lost(inUse, gone, notThere))
	{
		if (held != nullptr && gone(entt::to_integral(held->object)))
		{
			registry.Remove<CreatureHeldObject>(creature);
		}
		// A frightened creature that loses what it fled from wants to flee least of all
		if (mind.planActive && mind.planner.current.has_value() && mind.planner.current->desire == Desire::Fear &&
		    mind.desires.has_value())
		{
			creature_learning::MakeLeastDominant(*mind.desires, Desire::Fear, creature_learning::k_LeastDominantFactor,
			                                     mind_detail::DesireFloorOf(creature));
		}
		if (Locator::creatureHelpSystem::has_value())
		{
			Locator::creatureHelpSystem::value().Raise(
			    creature, {.kind = creature_help::Kind::FailureExplanation, .parameter = creature_lost_thing::k_LostThing});
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} lost a thing that it used", entt::to_integral(creature));
		Abandon(mind);
		idle.step = idle.agenda.size();
		idle.stepStarted = false;
		idle.gaveUp = true;
	}
	mind.beliefs.ForgetIfAnyWent(registry.GetEntitiesGone(),
	                             [&registry](entt::entity thing) { return !registry.Valid(thing); });
	// When the thing of its last try that fizzled goes, the creature forgets that try too
	auto* casting = registry.TryGet<components::CreatureCasting>(creature);
	if (casting == nullptr || casting->lastFizzleTarget == entt::null || registry.Valid(casting->lastFizzleTarget))
	{
		return;
	}
	const auto kept = creature_mind::ForgetGoneThing(
	    {.lastThing = entt::to_integral(casting->lastFizzleTarget), .pending = casting->fizzle.has_value()},
	    entt::to_integral(casting->lastFizzleTarget));
	if (!kept.lastThing.has_value())
	{
		casting->lastFizzleTarget = entt::null;
	}
	if (!kept.pending)
	{
		casting->fizzle.reset();
	}
}

void CreatureMindSystem::LookFarAbout(entt::entity creature, CreatureMindState& mind, const Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto from = map_coords::FromMetres(glm::xz(transform.position));
	// The nearest town but the one it came across last, and that town's storage pit
	std::vector<creature_object_beliefs::TownCell> towns;
	registry.Each<const Town, const Transform>([&](entt::entity town, const Town&, const Transform& at) {
		towns.push_back(
		    {.id = entt::to_integral(town), .cell = map_coords::Cell(map_coords::FromMetres(glm::xz(at.position)))});
	});
	const auto last = mind.lastTown != entt::null ? std::optional(entt::to_integral(mind.lastTown)) : std::nullopt;
	if (const auto town = creature_object_beliefs::NearestOtherTown(map_coords::Cell(from), towns, last); town.has_value())
	{
		mind.lastTown = static_cast<entt::entity>(*town);
		ComeAcross(creature, mind, mind.lastTown);
		if (const auto pit = ecs::construction::StoragePitOf(mind.lastTown); pit.has_value())
		{
			ComeAcross(creature, mind, *pit);
		}
	}
	// The nearest temple, if it is another than the last that it came across
	const auto temple = NearestTemple(registry, from);
	if (temple != entt::null && temple != mind.nearestTemple)
	{
		mind.nearestTemple = temple;
		ComeAcross(creature, mind, temple);
		// Then the temple's heart
		if (const auto heart = ecs::temple_heart::Of(registry, temple); heart != entt::null)
		{
			ComeAcross(creature, mind, heart);
		}
	}
}

namespace
{
/// The weights for what catches the eye of a creature, from its own traits
creature_look::Weights LookWeightsOf(const CreatureMindState& mind)
{
	return {.things = mind.innate.objectsLookWeight,
	        .mountains = mind.innate.mountainsLookWeight,
	        .water = mind.innate.waterLookWeight};
}

/// Where along the ground the creature finds what it watches: the thing's place, or the place of the mountains or the
/// water. None when it watches nothing, or a thing that is gone.
std::optional<glm::vec2> WatchedPlace(const ecs::Registry& registry, const creature_look::Target& look)
{
	if (look.place.has_value())
	{
		return look.place;
	}
	if (!look.id.has_value())
	{
		return std::nullopt;
	}
	const auto thing = static_cast<entt::entity>(*look.id);
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	return glm::xz(PlaceOf(registry, thing));
}
} // namespace

void CreatureMindSystem::LookAtLand(entt::entity entity, const Creature& creature, CreatureMindState& mind,
                                    const Transform& transform)
{
	if (!map_coords::InBounds(transform.position))
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 from = glm::xz(transform.position);
	const auto range = creature_look::LookRange(ShownSize(creature));
	const auto weights = LookWeightsOf(mind);
	// Each turn that it moves it finds on its map the nearest mountains, then the nearest water, and prefers the regions
	// that it did not go into yet. Each may take over what it watches.
	for (const auto& [sight, kind] : {std::pair {creature_look::Sight::Mountains, creature_exploration::Kind::Hill},
	                                  std::pair {creature_look::Sight::Water, creature_exploration::Kind::Sea}})
	{
		const auto place = ecs::creature_exploration_land::FindPlace(entity, kind, from, true);
		if (!place.has_value())
		{
			continue;
		}
		const auto watched = WatchedPlace(registry, mind.look);
		const auto watchedDistance = watched.has_value() ? gutils::GetDistanceInMetres(*watched, from) : 0.0f;
		creature_look::ConsiderLand(mind.look,
		                            {.sight = sight, .place = *place, .distance = gutils::GetDistanceInMetres(*place, from)},
		                            watchedDistance, range, weights, k_TurnsPerSecond);
	}
}

void CreatureMindSystem::SweepEyes(entt::entity entity, const Creature& creature, components::CreatureMindState& mind,
                                   const CreatureAnimation& animation, const Transform& transform)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto from = map_coords::FromMetres(glm::xz(transform.position));
	const auto centre = map_coords::Cell(from);
	const auto looking = mind_detail::LookingAngle(transform, animation.lookAt);
	const auto facing = mind_detail::LookingAngle(transform, std::nullopt);
	const auto range = creature_look::LookRange(ShownSize(creature));
	// How much things, mountains and water catch its own eye, which its personality spreads from its species' weights
	const auto weights = LookWeightsOf(mind);
	// Whether a place is in view, and how far it is along the ground
	const auto viewOf = [&](glm::vec2 at) {
		const auto coords = map_coords::FromMetres(at);
		const bool inView =
		    creature_look::InView(looking, facing, gutils::GetAngleFromXZ(from, coords), map_coords::Cell(coords) == centre);
		return std::pair {inView, gutils::GetDistanceInMetres(from, coords)};
	};
	const auto view = [&](entt::entity thing) { return viewOf(glm::xz(PlaceOf(registry, thing))); };
	for (const auto cell : beliefs::NextCells(mind.sweep, centre, ShownSize(creature)))
	{
		if (!map_coords::InBounds(cell))
		{
			continue;
		}
		const map_coords::MapCoords corner {cell.x * map_coords::k_FixedPerCell, cell.y * map_coords::k_FixedPerCell, 0.0f};
		if (!beliefs::CanSee(looking, gutils::GetAngleFromXZ(from, corner), cell == centre))
		{
			continue;
		}
		// The things that stay put, then the things that move
		for (const auto thing : map.GetAllInCell(cell))
		{
			// The thing in the map's cells for a temple is the temple's heart. The creature believes in the heart and
			// watches the heart.
			const auto seen = ecs::temple_heart::MapThingOf(registry, thing);
			ComeAcross(entity, mind, seen);
			// Each thing in view that it sees may take over what it watches
			if (seen == entity || !registry.Valid(seen))
			{
				continue;
			}
			const auto [inView, distance] = view(seen);
			if (!inView)
			{
				continue;
			}
			const auto watched = WatchedPlace(registry, mind.look);
			const auto watchedDistance = watched.has_value() ? viewOf(*watched).second : 0.0f;
			creature_look::Consider(
			    mind.look, {.id = entt::to_integral(seen), .kind = LookInterestOf(registry, seen), .distance = distance},
			    watchedDistance, range, weights, k_TurnsPerSecond);
		}
	}
	// Each turn it comes across what it carries
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(entity); held.has_value())
		{
			ComeAcross(entity, mind, *held);
		}
	}
	// It watches what it watches a turn longer while it is there and in view
	const auto watched = WatchedPlace(registry, mind.look);
	creature_look::Watch(mind.look, watched.has_value() && viewOf(*watched).first);
	if (!creature_look::Watches(mind.look))
	{
		return;
	}
	const auto ground = [](glm::vec3 at) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(at)) : at.y;
	};
	if (mind.look.place.has_value())
	{
		// The head looks at the ground there
		const glm::vec3 at {mind.look.place->x, 0.0f, mind.look.place->y};
		mind.look.point = {at.x, ground(at), at.z};
		return;
	}
	// The head looks at the thing's place, as high above the ground as the thing is tall
	const auto thing = static_cast<entt::entity>(*mind.look.id);
	const auto at = PlaceOf(registry, thing);
	mind.look.point = {at.x, ground(at) + HeightOf(registry, thing), at.z};
}
