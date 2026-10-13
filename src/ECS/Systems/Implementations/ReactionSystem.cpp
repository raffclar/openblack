/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ReactionSystem.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureFireReaction.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureReactionRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/TownDesire.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/AreaEffect.h"
#include "Magic/Impressiveness.h"
#include "Magic/ReactionRules.h"
#include "Magic/ReactionTimingRules.h"
#include "Magic/TownBelief.h"
#include "Magic/VillagerReactionRules.h"
#include "ObjectMeasures.h"
#include "Physics/LivingRules.h"
#include "VillagerBreeding.h"
#include "VillagerCreatureReactions.h"
#include "VillagerFire.h"
#include "VillagerFood.h"
#include "VillagerForester.h"
#include "VillagerMourning.h"
#include "VillagerPhysics.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace villager_reaction = openblack::magic::villager_reaction;
namespace reaction_timing = openblack::magic::reaction_timing;

namespace
{
/// Whole seconds of game turns, at the game's ten turns a second
constexpr uint32_t k_TurnsPerSecond = 10;
/// A creature's way of moving, as those fleeing it see it: ten metres the way it faces
constexpr float k_CreatureFacingMetres = 10.0f;

/// The seed in the hand that cast a miracle, if the miracle has one
SpellSeed* SeedOf(ecs::Registry& registry, entt::entity spell)
{
	const auto* miracle = registry.Valid(spell) ? registry.TryGet<const ecs::components::Spell>(spell) : nullptr;
	return miracle != nullptr && registry.Valid(miracle->seed) ? registry.TryGet<SpellSeed>(miracle->seed) : nullptr;
}

/// Where a town's centre stands, if it has one
std::optional<glm::vec3> TownCentreOf(ecs::Registry& registry, uint32_t townId)
{
	std::optional<glm::vec3> found;
	registry.Each<const Abode, const Transform>([&](entt::entity, const Abode& abode, const Transform& transform) {
		if (!found.has_value() && abode.type == AbodeNumber::TownCentre && abode.townId == townId)
		{
			found = transform.position;
		}
	});
	return found;
}

const ReactionInfo* InfoOf(Reaction type)
{
	if (!Locator::infoConstants::has_value() || type == Reaction::None)
	{
		return nullptr;
	}
	const auto& table = Locator::infoConstants::value().reaction;
	const auto index = static_cast<size_t>(type);
	return index < table.size() ? &table.at(index) : nullptr;
}

/// The living table of a villager or creature
const GLivingInfo* LivingInfoOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto& info = Locator::infoConstants::value();
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto row = creature::InfoRow(creature->species);
		return row < info.creature.size() ? &info.creature.at(row) : nullptr;
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		const auto kind = static_cast<size_t>(GVillagerInfo::Find(villager->tribe, villager->number));
		return kind < info.villager.size() ? &info.villager.at(kind) : nullptr;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		const auto kind = static_cast<size_t>(animal->type);
		return kind < info.animal.size() ? &info.animal.at(kind) : nullptr;
	}
	return nullptr;
}

/// Whether a reaction reaches a kind of living thing: villagers and creatures every kind; animals, so far, only things
/// flying at them, fires and things to flee, all of which they flee
// TODO(animals): the other reactions animals take up (predators and the others whose priority the animal doesn't zero)
// belong to the animals' own behaviour, which openblack doesn't run yet
bool Reaches(const ecs::Registry& registry, entt::entity entity, Reaction type)
{
	if (registry.AnyOf<Villager, Creature>(entity))
	{
		return true;
	}
	return (type == Reaction::ReactToFlyingObject || type == Reaction::ReactToFire || type == Reaction::FleeFromObject) &&
	       registry.AllOf<Animal>(entity);
}

/// Whether a kind of living thing reacts to a kind of reaction at all, by its table's flags in the reactions' order
bool Reacts(const GLivingInfo& info, Reaction type)
{
	constexpr size_t k_Flags = sizeof(IsReacting) / sizeof(uint32_t);
	std::array<uint32_t, k_Flags> flags {};
	std::memcpy(flags.data(), &info.isReacting, sizeof(IsReacting));
	const auto index = static_cast<size_t>(type);
	return index < flags.size() && flags.at(index) != 0;
}

/// The shields' own reactions, which their villagers take up through the shields
bool ShieldKind(Reaction type)
{
	return type == Reaction::ReactToMagicShield || type == Reaction::ReactToMagicShieldStruck ||
	       type == Reaction::ReactToMagicShieldDestroyed;
}

villager_reaction::Distance DistanceOf(const ReactionInfo& info)
{
	return {.maxDistance = info.maxReactionDistance, .importance = info.howImportantIsDistance};
}

float SpreadDistance(const glm::vec3& at, const glm::vec3& source)
{
	return villager_reaction::SpreadDistance({at.x, at.z}, {source.x, source.z});
}

/// Who is reacting, as the reaction's timing tells them apart
reaction_timing::Reacting WhoOf(const ecs::Registry& registry, entt::entity living)
{
	if (registry.AllOf<Villager>(living))
	{
		return reaction_timing::Reacting::Villager;
	}
	return registry.AllOf<Creature>(living) ? reaction_timing::Reacting::Creature : reaction_timing::Reacting::Other;
}

/// Where the thing a reaction is about is now: the thing itself while it is there, else where the reaction is
glm::vec3 ThingAt(const ecs::Registry& registry, const ReactionSystem::Active& reaction)
{
	const auto thing = reaction.source.initiator;
	if (thing != entt::null && registry.Valid(thing))
	{
		if (const auto* transform = registry.TryGet<const Transform>(thing))
		{
			return transform->position;
		}
	}
	return reaction.source.position;
}

/// Whether what a living reacts to has gone: deleted, or a villager dying
bool ThingGone(const ecs::Registry& registry, entt::entity thing)
{
	if (thing == entt::null)
	{
		return false;
	}
	if (!registry.Valid(thing))
	{
		return true;
	}
	const auto* action = registry.TryGet<const LivingAction>(thing);
	return registry.AllOf<Villager>(thing) && action != nullptr && Locator::livingActionSystem::has_value() &&
	       Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Final) == VillagerStates::Dying;
}

/// What a living weighs about a predator it reacts to
reaction_timing::PredatorWatch PredatorWatchOf(const ecs::Registry& registry, entt::entity living, entt::entity predator,
                                               const ReactionInfo& info)
{
	reaction_timing::PredatorWatch watch {.leastRunDistance = info.minDistanceToRunAwayFromObject};
	if (predator == entt::null || !registry.Valid(predator) || !registry.AllOf<Transform>(predator) ||
	    !registry.AllOf<Transform>(living))
	{
		return watch;
	}
	const auto& predatorAt = registry.Get<const Transform>(predator).position;
	const auto& livingAt = registry.Get<const Transform>(living).position;
	watch.animal = registry.AllOf<Animal>(predator);
	watch.distance = gutils::GetDistanceInMetres(livingAt, predatorAt);
	if (const auto* walker = registry.TryGet<const WallHug>(predator))
	{
		watch.speed = walker->speed;
		watch.comingAt = reaction_timing::ComingAt(predatorAt, glm::vec2(walker->step), livingAt);
	}
	if (const auto* action = registry.TryGet<const LivingAction>(living);
	    action != nullptr && registry.AllOf<Villager>(living) && Locator::livingActionSystem::has_value())
	{
		const auto& states = Locator::livingActionSystem::value();
		const auto top = states.VillagerGetState(*action, LivingAction::Index::Top);
		const auto final = states.VillagerGetState(*action, LivingAction::Index::Final);
		watch.lookingAtThing = top == VillagerStates::LookingAtObjectReaction;
		watch.hidingOrLooking =
		    final == VillagerStates::GoAndHideInNearbyBuilding || final == VillagerStates::LookToSeeIfItIsSafe;
		watch.lookingIfSafe = final == VillagerStates::LookToSeeIfItIsSafe;
	}
	// An animal looks at a thing while it keeps one of the reactions to look at something
	if (const auto* state = registry.TryGet<const LivingReaction>(living);
	    state != nullptr && state->reaction != 0 && registry.AllOf<Animal>(living))
	{
		watch.lookingAtThing = reaction_timing::LooksAtThing(state->type);
	}
	return watch;
}

/// The plan a creature is carrying out, if any
const creature_planner::Plan* CurrentPlanOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return mind != nullptr && mind->planner.current.has_value() ? &*mind->planner.current : nullptr;
}

/// Whether the creature's plan is an action of the game's table by its name
bool PlanIs(const ecs::Registry& registry, entt::entity creature, std::string_view action)
{
	const auto* plan = CurrentPlanOf(registry, creature);
	if (plan == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	const auto* tables = Locator::creatureMindSystem::value().GetTables();
	return tables != nullptr && plan->action < tables->actions.size() && tables->actions[plan->action].name == action;
}

/// Whether a creature sees a point: near enough the way its head looks, or the way it faces when it looks at nothing, or
/// in its own map cell
bool CreatureSees(const ecs::Registry& registry, entt::entity creature, const glm::vec3& point)
{
	const auto& at = registry.Get<const Transform>(creature).position;
	glm::vec2 way {0.0f};
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (body != nullptr && body->lookAt.has_value() && glm::vec2(body->lookAt->x, body->lookAt->z) != glm::vec2(at.x, at.z))
	{
		way = {body->lookAt->x - at.x, body->lookAt->z - at.z};
	}
	else if (const auto* walker = registry.TryGet<const CreatureLocomotion>(creature))
	{
		way = creature_locomotion::DirectionOf(walker->heading);
	}
	const glm::vec2 from(at.x, at.z);
	const glm::vec2 to(point.x, point.z);
	const bool sameCell = map_coords::Cell(map_coords::FromMetres(from)) == map_coords::Cell(map_coords::FromMetres(to));
	return creature_reaction::CanSee(creature_reaction::LookAngle(way), gutils::GetAngleFromXZ(from, to), sameCell);
}

/// What a creature knows as it weighs a reaction
creature_reaction::Facts CreatureFactsOf(const ecs::Registry& registry, entt::entity creature,
                                         const ReactionSystem::Active& reaction, const ReactionInfo& info, uint32_t turn)
{
	const auto& source = reaction.source;
	const auto& body = registry.Get<const Creature>(creature);
	const auto thing = source.initiator;
	const bool there = thing != entt::null && registry.Valid(thing);
	const bool computer = Locator::playerSystem::has_value()
	                          ? Locator::playerSystem::value().IsComputerPlayer(body.owner)
	                          : body.owner != PlayerNames::PLAYER_ONE && body.owner != PlayerNames::NEUTRAL;
	creature_reaction::Facts facts {
	    .tablePriority = static_cast<uint8_t>(std::min<uint32_t>(info.priority, 255)),
	    .humanPlayer = !computer,
	    .otherPlayers = !source.playerless && source.player != body.owner,
	    .seesThing = CreatureSees(registry, creature, ThingAt(registry, reaction)),
	    .thingIsSelf = thing == creature,
	    .thingGone = !there,
	    .thingIsCreature = there && registry.AllOf<Creature>(thing),
	    .turn = turn,
	    .catching = PlanIs(registry, creature, "Catch"),
	    .fighting = PlanIs(registry, creature, "Fight"),
	};
	if (const auto* state = registry.TryGet<const LivingReaction>(creature))
	{
		facts.sawFlying = state->sawFlying;
		facts.sawCrushed = state->sawCrushed;
	}
	if (const auto* plan = CurrentPlanOf(registry, creature))
	{
		facts.angry = plan->desire == creature_desires::Desire::Anger;
	}
	if (facts.thingIsCreature && Locator::creatureFightSystem::has_value())
	{
		facts.thingFightsSelf = Locator::creatureFightSystem::value().OpponentOf(thing) == std::optional(creature);
	}
	if (source.type == Reaction::ReactToFlyingObject && there)
	{
		const auto* entry = Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().Find(thing) : nullptr;
		facts.thrownFromHand = entry != nullptr && entry->Has(ecs::PhysicsEntry::k_FromHand);
		facts.inTheAir = ecs::villager_physics::IsActuallyInTheAir(thing);
	}
	if (source.type == Reaction::ReactToHandPickUp && there)
	{
		const auto* grab = registry.TryGet<const HandGrab>(thing);
		facts.ownPlayersHand = registry.AllOf<Hand>(thing) && source.player == body.owner;
		facts.handHolds = grab != nullptr && registry.Valid(grab->object) &&
		                  (grab->state == HandGrab::State::Holding || grab->state == HandGrab::State::ReadyToThrow);
	}
	return facts;
}

/// What a creature does on taking up a reaction, for the kinds whose rules are its own; none for fire, teleport stones and
/// miracles
std::optional<creature_reaction::Setup> CreatureSetupOf(const ecs::Registry& registry, const ReactionSystem::Active& reaction)
{
	const auto thing = reaction.source.initiator;
	const bool inTheAir = reaction.source.type == Reaction::ReactToFlyingObject && thing != entt::null &&
	                      registry.Valid(thing) && ecs::villager_physics::IsActuallyInTheAir(thing);
	return creature_reaction::SetupFor(reaction.source.type, inTheAir);
}

/// Whether a creature keeps a reaction it takes up: not another player's nice miracle or shield, nor a magic tree's,
/// nor the kinds it heeds without keeping
bool CreatureKeeps(const ecs::Registry& registry, entt::entity creature, const ReactionSystem::Active& reaction)
{
	const auto& source = reaction.source;
	const bool nice = source.type == Reaction::LookAtNiceSpell || source.type == Reaction::ReactToImpressiveSpell ||
	                  source.type == Reaction::ReactToMagicShield;
	if ((nice && source.player != registry.Get<const Creature>(creature).owner) || source.type == Reaction::ReactToMagicTree)
	{
		return false;
	}
	const auto setup = CreatureSetupOf(registry, reaction);
	return !setup.has_value() || setup->keeps;
}

/// The kinds of reaction a living thing took up lately: a villager's shared with the shields
villager_reaction::Memory& MemoryOf(ecs::Registry& registry, entt::entity living, LivingReaction& state)
{
	if (!registry.AllOf<Villager>(living))
	{
		return state.creatureMemory;
	}
	auto* memory = registry.TryGet<VillagerReactionMemory>(living);
	if (memory == nullptr)
	{
		memory = &registry.Assign<VillagerReactionMemory>(living);
	}
	return memory->memory;
}

/// A creature copying the player takes up only reactions more urgent than this
constexpr uint32_t k_LeastPriorityWhileMimicking = 150;

/// Whether a living thing may take up a kind of reaction now: a villager able and alive and in a state that allows it, a
/// creature not fighting or out cold, and while copying the player only for the most urgent kinds
bool Available(const ecs::Registry& registry, entt::entity entity, Reaction type)
{
	if (registry.AllOf<Villager>(entity))
	{
		return villager_reactions::Available(entity, type);
	}
	if (registry.AllOf<Animal>(entity))
	{
		return Locator::animalSystem::has_value() && Locator::animalSystem::value().IsAvailableForReaction(entity);
	}
	if (!registry.AllOf<Creature>(entity))
	{
		return false;
	}
	if (Locator::creatureFightSystem::has_value())
	{
		const auto& fights = Locator::creatureFightSystem::value();
		if (fights.IsFighting(entity) || fights.IsKnockedOut(entity))
		{
			return false;
		}
	}
	// TODO(raffclar): the game also keeps a creature reactions are turned off for by script, one in the dance editor, one
	// forced into an action by fainting or a teleport, and a few more states, from reacting; none of them is modelled
	if (const auto* mind = registry.TryGet<const CreatureMindState>(entity);
	    mind != nullptr && mind->learnt.has_value() && mind->learnt->mimicry.has_value())
	{
		const auto* info = InfoOf(type);
		return info != nullptr && info->priority > k_LeastPriorityWhileMimicking;
	}
	return true;
}

/// A villager hiding in a building, or one of the kinds of reaction only believed in, is impressed by what it sees
/// without taking the reaction up
bool BelievesWithoutReacting(const ecs::Registry& registry, entt::entity entity, Reaction type)
{
	if (!registry.AllOf<Villager>(entity))
	{
		return false;
	}
	if (type == Reaction::ReactToMagicWaterPuttingOutFire || type == Reaction::ReactToMissionary ||
	    type == Reaction::ReactToFightWon)
	{
		return true;
	}
	// TODO(raffclar): the game also counts a villager out of reach by two flags whose meaning is unknown
	const auto* action = registry.TryGet<const LivingAction>(entity);
	return action != nullptr && Locator::livingActionSystem::has_value() &&
	       Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Top) ==
	           VillagerStates::GoAndHideInNearbyBuilding;
}

/// The kinds of reaction a living thing switching to another reaction of the same kind may take up: fire, a predator, a
/// shield struck
bool SameKindRetakes(Reaction type)
{
	return type == Reaction::ReactToFire || type == Reaction::FleeFromPredator || type == Reaction::ReactToMagicShieldStruck;
}
} // namespace

uint32_t ReactionSystem::Create(const Source& source)
{
	const auto* info = InfoOf(source.type);
	const uint32_t id = _nextId++;
	_reactions.push_back(
	    {.id = id,
	     .source = source,
	     .age = 0,
	     .reach = source.reach.value_or(
	         info != nullptr ? magic::StartingReach(info->whetherReactionGrows != 0, info->maxReactionDistance) : 0.0f),
	     .firstTaken = source.onCast ? std::optional<uint32_t>(0) : std::nullopt});
	// It is spread at once
	Spread(_reactions.back());
	return id;
}

void ReactionSystem::Move(uint32_t id, const glm::vec3& position, const glm::vec3& velocity, float strength)
{
	const auto found = std::ranges::find(_reactions, id, &Active::id);
	if (found != _reactions.end())
	{
		found->source.position = position;
		found->source.strength = strength;
		found->velocity = velocity;
	}
}

void ReactionSystem::SetInitiator(uint32_t id, entt::entity initiator)
{
	const auto found = std::ranges::find(_reactions, id, &Active::id);
	if (found != _reactions.end())
	{
		found->source.initiator = initiator;
	}
}

void ReactionSystem::SetDance(uint32_t id, entt::entity dance)
{
	const auto found = std::ranges::find(_reactions, id, &Active::id);
	if (found != _reactions.end())
	{
		found->dance = dance;
	}
}

std::optional<ReactionSystemInterface::Active> ReactionSystem::Find(uint32_t id) const
{
	const auto found = std::ranges::find(_reactions, id, &Active::id);
	return found != _reactions.end() ? std::optional(*found) : std::nullopt;
}

void ReactionSystem::RemoveFrom(entt::entity initiator, Reaction type)
{
	std::vector<uint32_t> gone;
	for (const auto& reaction : _reactions)
	{
		if (reaction.source.initiator == initiator && reaction.source.type == type)
		{
			gone.push_back(reaction.id);
		}
	}
	for (const auto id : gone)
	{
		ShutDown(id);
	}
}

bool ReactionSystem::IsActive(uint32_t id) const
{
	return std::ranges::any_of(_reactions, [id](const Active& reaction) { return reaction.id == id; });
}

void ReactionSystem::RemoveFrom(entt::entity initiator)
{
	std::vector<uint32_t> gone;
	for (const auto& reaction : _reactions)
	{
		if (reaction.source.initiator == initiator)
		{
			gone.push_back(reaction.id);
		}
	}
	for (const auto id : gone)
	{
		ShutDown(id);
	}
}

void ReactionSystem::ShutDown(uint32_t id)
{
	// It ends before those reacting to it are let go, so a state they go back to already finds it over
	const auto found = std::ranges::find(_reactions, id, &Active::id);
	const auto dance = found != _reactions.end() ? found->dance : entt::null;
	std::erase_if(_reactions, [id](const Active& reaction) { return reaction.id == id; });
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> reacting;
		registry.Each<LivingReaction>([&](entt::entity entity, const LivingReaction& state) {
			if (state.reaction == id)
			{
				reacting.push_back(entity);
			}
		});
		for (const auto entity : reacting)
		{
			Stop(entity, registry.Get<LivingReaction>(entity), true);
		}
	}
	// Its dance goes with it, once its dancers have gone back
	if (dance != entt::null && Locator::danceSystem::has_value() && Locator::entitiesRegistry::has_value() &&
	    Locator::entitiesRegistry::value().Valid(dance))
	{
		Locator::danceSystem::value().Destroy(dance);
	}
}

void ReactionSystem::Remove(uint32_t id)
{
	ShutDown(id);
}

bool ReactionSystem::HasReaction(entt::entity initiator) const
{
	return std::ranges::any_of(_reactions,
	                           [initiator](const Active& reaction) { return reaction.source.initiator == initiator; });
}

uint32_t ReactionSystem::PriorityTo(const Active& reaction, entt::entity living, const glm::vec3& at) const
{
	return PriorityTo(reaction, living, at, SpreadDistance(at, reaction.source.position));
}

uint32_t ReactionSystem::PriorityTo(const Active& reaction, entt::entity living, const glm::vec3& at, float distance) const
{
	const auto* info = InfoOf(reaction.source.type);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* livingInfo = LivingInfoOf(registry, living);
	if (info == nullptr || livingInfo == nullptr)
	{
		return 0;
	}
	// A teleport stone matters only to one whose walk it would shorten enough
	if (reaction.source.type == Reaction::ReactToTeleport &&
	    (!Locator::teleportSystem::has_value() ||
	     !Locator::teleportSystem::value().ShouldReact(reaction.source.initiator, living)))
	{
		return 0;
	}
	// A villager flying or coming down takes no notice of what else flies
	if (reaction.source.type == Reaction::ReactToFlyingObject && registry.AllOf<Villager>(living) &&
	    !villager_physics::TakesFlyingObjectReaction(living))
	{
		return 0;
	}
	// A dancing villager notices a flying thing only when it comes at it
	if (reaction.source.type == Reaction::ReactToFlyingObject && registry.AllOf<Villager, Dancer>(living))
	{
		const auto* entry =
		    Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().Find(reaction.source.initiator) : nullptr;
		const auto* object =
		    registry.Valid(reaction.source.initiator) ? registry.TryGet<const Transform>(reaction.source.initiator) : nullptr;
		if (entry == nullptr || entry->body == nullptr || object == nullptr ||
		    !physics::living::FlyingAt(registry.Get<const Transform>(living).position, object->position, entry->body->velocity))
		{
			return 0;
		}
	}
	// A villager sheltering under a magic shield takes no notice of what flies outside the shield
	if (reaction.source.type == Reaction::ReactToFlyingObject && registry.AllOf<Villager>(living))
	{
		if (const auto* shelter = registry.TryGet<const VillagerShieldReaction>(living);
		    shelter != nullptr && shelter->type == Reaction::ReactToMagicShield)
		{
			const auto* shield =
			    registry.Valid(shelter->shield) ? registry.TryGet<const MagicShield>(shelter->shield) : nullptr;
			const auto* where = registry.Valid(shelter->shield) ? registry.TryGet<const Transform>(shelter->shield) : nullptr;
			if (shield == nullptr || where == nullptr ||
			    !physics::living::ShelterSeesFlyer(where->position, shield->radius, reaction.source.position))
			{
				return 0;
			}
		}
	}
	// Only a villager that might become the mate of one of its people held in the hand heeds it; nothing else does
	if (reaction.source.type == Reaction::ReactToVillagerInHand)
	{
		return registry.AllOf<Villager>(living)
		           ? villager_reactions::ReactToVillagerInHandPriority(living, reaction.source.initiator)
		           : 0;
	}
	// Only a villager of an age and of the other sex heeds a breeder's call, wherever it is in its reach
	if (reaction.source.type == Reaction::ReactToBreeder)
	{
		return registry.AllOf<Villager>(living)
		           ? ecs::villager_breeding::ReactToBreederPriority(living, reaction.source.initiator)
		           : 0;
	}
	// A creature weighs several kinds its own way: by whether it sees the thing, whether a person plays its player, and
	// what it is doing
	if (registry.AllOf<Creature>(living) && registry.AllOf<Transform>(living))
	{
		const auto random = [](uint32_t range) {
			return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(range) : 0u;
		};
		if (const auto kind = creature_reaction::Priority(reaction.source.type,
		                                                  CreatureFactsOf(registry, living, reaction, *info, _turn), random);
		    kind.has_value())
		{
			return villager_reaction::Priority(*kind, Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
		}
	}
	// A villager weighs a creature by how its town feels about it
	// TODO(villager-social): creatures and animals weigh a creature by rules of their own that aren't ported yet; until
	// then they take no notice of it
	if (reaction.source.type == Reaction::ReactToCreature)
	{
		if (!registry.AllOf<Villager>(living))
		{
			return 0;
		}
		const auto kind = ecs::villager_creature::Priority(living, reaction.source.initiator);
		return villager_reaction::Priority(kind, Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	// A villager heeds a burning thing in the hand when near it or the thing moves fast
	if (reaction.source.type == Reaction::ReactToBurningObjectInHand && registry.AllOf<Villager>(living))
	{
		const auto kind = villager_reactions::BurningInHandPriority(living, reaction.source.initiator, info->priority);
		return villager_reaction::Priority(kind, Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	// A villager heeds its town's celebration only when the town itself makes it
	if (reaction.source.type == Reaction::ReactToTownCelebration && registry.AllOf<Villager>(living))
	{
		const auto& villager = registry.Get<const Villager>(living);
		const auto* disciple = registry.TryGet<const Disciple>(living);
		const auto kind = villager_reaction::TownCelebrationPriority(
		    {.disciple = disciple != nullptr && disciple->type != VillagerDisciple::None,
		     .dancing = registry.AllOf<Dancer>(living),
		     .hasTown = villager.town != entt::null,
		     .madeByOwnTown = villager.town != entt::null && villager.town == reaction.source.initiator},
		    info->priority);
		return villager_reaction::Priority(kind, Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	// A villager heeds food lying about only when it catches its eye
	if (reaction.source.type == Reaction::ReactToFood && registry.AllOf<Villager>(living))
	{
		const auto kind = ecs::villager_food::ReactToFoodPriority(living, reaction.source.initiator, info->priority);
		return villager_reaction::Priority(static_cast<uint8_t>(std::min<uint32_t>(kind, 255)),
		                                   Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	// A villager heeds wood lying about only when it catches its eye
	if (reaction.source.type == Reaction::ReactToWood && registry.AllOf<Villager>(living))
	{
		const auto kind = ecs::villager_forester::ReactToWoodPriority(living, reaction.source.initiator, info->priority);
		return villager_reaction::Priority(static_cast<uint8_t>(std::min<uint32_t>(kind, 255)),
		                                   Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	// A villager heeds a death unless its town has a working graveyard, or enough heed it already
	if (reaction.source.type == Reaction::ReactToDeath && registry.AllOf<Villager>(living))
	{
		const auto kind = ecs::villager_mourning::Priority(living, reaction.source.initiator, reaction.id,
		                                                   static_cast<uint8_t>(std::min<uint32_t>(info->priority, 255)));
		return villager_reaction::Priority(kind, Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	// A villager weighs a fire by its own rule: how far the fire reaches of its fiercest, and whether it fights it already
	if (reaction.source.type == Reaction::ReactToFire && registry.AllOf<Villager>(living))
	{
		return villager_fire::ReactToFirePriority(living, reaction.source.initiator, reaction.id);
	}
	// A creature heeds a fire only when it is near enough the burning thing and big enough beside it
	if (reaction.source.type == Reaction::ReactToFire && registry.AllOf<Creature>(living))
	{
		const auto burning = reaction.source.initiator;
		const auto burningAt = registry.Valid(burning) ? object_measures::PositionOf(registry, burning) : std::nullopt;
		if (!burningAt.has_value())
		{
			return 0;
		}
		const auto kind =
		    creature_fire::Priority(gutils::GetDistanceInMetres(*burningAt, at), object_measures::TwoDRadius(registry, burning),
		                            object_measures::TwoDRadius(registry, living), object_measures::Height(registry, living),
		                            object_measures::Height(registry, burning), static_cast<uint8_t>(info->priority));
		return villager_reaction::Priority(kind, Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
	}
	const auto kind =
	    magic::KindPriority(reaction.source.type, info->priority, magic::FastMapDistance(at, reaction.source.position),
	                        living == reaction.source.casterCreature);
	return villager_reaction::Priority(static_cast<uint8_t>(std::min<uint32_t>(kind, 255)),
	                                   Reacts(*livingInfo, reaction.source.type), DistanceOf(*info), distance);
}

void ReactionSystem::Spread(Active& reaction)
{
	const auto* info = InfoOf(reaction.source.type);
	if (info == nullptr || !Locator::entitiesRegistry::has_value() || !Locator::entitiesMap::has_value())
	{
		return;
	}
	// The reaction spreads over the map's cells in a spiral out to its reach, as many cells as its initiator's strength
	// makes of the spiral, to the living each cell keeps, the last to come into it first
	const auto& map = Locator::entitiesMap::value();
	const glm::vec2 centre(reaction.source.position.x, reaction.source.position.z);
	const auto start = map_coords::FromMetres(centre);
	auto cell = start;
	map_coords::Spiral spiral;
	const auto steps =
	    static_cast<int32_t>(static_cast<float>(map_coords::CellSpiralSize(reaction.reach)) * reaction.source.strength);
	for (int32_t i = 0; i < steps; ++i)
	{
		if (i > 0)
		{
			map_coords::AddCells(cell, spiral.Next());
		}
		if (!map_coords::InBounds(cell) || gutils::GetDistanceInMetres(start, cell) > reaction.reach)
		{
			continue;
		}
		const auto id = ecs::MapInterface::CellId(map_coords::Cell(cell));
		const auto& mobiles = map.GetMobileInGridCell(id);
		SpreadInCell(reaction, std::vector<entt::entity>(mobiles.begin(), mobiles.end()));
	}
}

void ReactionSystem::SpreadInCell(Active& reaction, const std::vector<entt::entity>& mobiles)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = static_cast<uint32_t>(reaction.source.type);
	for (const auto entity : mobiles)
	{
		// The shields' villagers react to the shields' own reactions through the shields, and while they do, to nothing
		// else but what flies inside the shield, which their priority weighs
		// TODO(raffclar): the game weighs another reaction against a villager's shield reaction as against any other
		// A burning creature takes up its own fire's reaction, as any other within reach does
		const bool ownFire = reaction.source.type == Reaction::ReactToFire && registry.AllOf<Creature>(entity);
		if (!registry.Valid(entity) || !registry.AllOf<Transform>(entity) ||
		    (entity == reaction.source.initiator && !ownFire) || !Reaches(registry, entity, reaction.source.type) ||
		    (registry.AllOf<Villager>(entity) &&
		     (ShieldKind(reaction.source.type) ||
		      (registry.AllOf<VillagerShieldReaction>(entity) && reaction.source.type != Reaction::ReactToFlyingObject))) ||
		    !Available(registry, entity, reaction.source.type))
		{
			continue;
		}
		const auto at = registry.Get<const Transform>(entity).position;
		// Under a shield that what made the reaction isn't inside, where it is now, it is kept off
		const auto* initiatorAt =
		    registry.Valid(reaction.source.initiator) ? registry.TryGet<const Transform>(reaction.source.initiator) : nullptr;
		if (Locator::magicShieldSystem::has_value() &&
		    Locator::magicShieldSystem::value().KeepsReactionOff(at, initiatorAt != nullptr ? initiatorAt->position
		                                                                                    : reaction.source.position))
		{
			continue;
		}
		auto* state = registry.TryGet<LivingReaction>(entity);
		if (state == nullptr)
		{
			state = &registry.Assign<LivingReaction>(entity);
		}
		auto& memory = MemoryOf(registry, entity, *state);
		const auto again = AgainTurns(reaction, entity, at);
		// Impressed without reacting, it is the last in the cell the reaction reaches
		if (BelievesWithoutReacting(registry, entity, reaction.source.type))
		{
			if (memory.MayReactAgain(type, _turn, again))
			{
				Impress(reaction, entity, at);
			}
			return;
		}
		if (state->reaction == 0)
		{
			if (PriorityTo(reaction, entity, at) != 0 && memory.MayReactAgain(type, _turn, again))
			{
				if (!reaction.firstTaken.has_value())
				{
					reaction.firstTaken = reaction.age;
				}
				// A villager heeds some kinds without ever taking them up
				if (registry.AllOf<Villager>(entity) && !villager_reaction::TakesUp(reaction.source.type))
				{
					continue;
				}
				Start(reaction, entity, *state);
			}
			continue;
		}
		const auto current = Find(state->reaction);
		if (!current.has_value())
		{
			continue;
		}
		// Weighed against what it reacts to now, by how far it is from the middle of that reaction's cell
		const auto cellOfCurrent =
		    map_coords::Cell(map_coords::FromMetres({current->source.position.x, current->source.position.z}));
		const auto currentPriority = PriorityTo(
		    *current, entity, at,
		    gutils::GetDistanceInMetresToCell(map_coords::FromMetres({at.x, at.z}),
		                                      {static_cast<int16_t>(cellOfCurrent.x), static_cast<int16_t>(cellOfCurrent.y)}));
		const auto priority = PriorityTo(reaction, entity, at);
		const bool differs =
		    state->type != reaction.source.type || (SameKindRetakes(reaction.source.type) && state->reaction != reaction.id);
		const auto seconds = (_turn - memory.LastReacted(static_cast<uint32_t>(state->type))) / k_TurnsPerSecond;
		if (differs && magic::ChangesReaction(state->type, currentPriority, priority, seconds))
		{
			memory.Record(type, _turn);
			// A villager keeps what it reacts to rather than change to a kind it never takes up
			// TODO(raffclar): the game also drops the villager from the list of the reaction it keeps, so that reaction
			// ending doesn't send it back; it still goes back once its own time with it is over
			if (registry.AllOf<Villager>(entity) && !villager_reaction::TakesUp(reaction.source.type))
			{
				continue;
			}
			// A creature heeds the new one, but keeps what it reacts to rather than change to one it doesn't keep
			// TODO(raffclar): the game also drops the creature from the list of the reaction it keeps, as for villagers
			if (registry.AllOf<Creature>(entity) && !CreatureKeeps(registry, entity, reaction))
			{
				Start(reaction, entity, *state, true);
				continue;
			}
			Stop(entity, *state, false);
			Start(reaction, entity, *state, true);
		}
	}
}

uint32_t ReactionSystem::AgainTurns(const Active& reaction, entt::entity living, const glm::vec3& at) const
{
	const auto* info = InfoOf(reaction.source.type);
	if (info == nullptr)
	{
		return 0;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto who = WhoOf(registry, living);
	const auto thing = reaction.source.initiator;
	// Weighed by how far it is from the reaction's thing as the reaction spreads
	const float distance = SpreadDistance(at, ThingAt(registry, reaction));
	const auto standard = reaction_timing::StandardTurnsBeforeReactingAgain(reaction_timing::RowOf(*info), who, distance);
	int32_t turns = standard;
	switch (reaction_timing::AgainRule(reaction.source.type))
	{
	case reaction_timing::Rule::Creature:
		// Waiting for a creature goes by how impressive it is and how the town feels about it, for all alike
		turns = static_cast<int32_t>(ecs::villager_creature::TurnsBeforeReactingAgain(living, thing, distance));
		break;
	case reaction_timing::Rule::Predator:
		turns =
		    reaction_timing::PredatorTurnsBeforeReactingAgain(PredatorWatchOf(registry, living, thing, *info), who, standard);
		break;
	case reaction_timing::Rule::BurningObject:
		if (who == reaction_timing::Reacting::Villager)
		{
			turns = static_cast<int32_t>(
			    villager_reactions::BurningInHandAgainTurns(living, thing, static_cast<uint32_t>(standard)));
		}
		break;
	case reaction_timing::Rule::Wood:
		// A forester at work goes for wood again straight away
		if (who == reaction_timing::Reacting::Villager && ecs::villager_forester::ReactsToWoodAgainAtOnce(living))
		{
			turns = 0;
		}
		break;
	default:
		break;
	}
	// The wait is counted as a whole number of turns, so a negative one is a very long wait
	return static_cast<uint32_t>(turns);
}

int32_t ReactionSystem::TurnsToReact(const Active& reaction, entt::entity living, Reaction type) const
{
	const auto* info = InfoOf(type);
	const auto& registry = Locator::entitiesRegistry::value();
	if (info == nullptr || !registry.AllOf<Transform>(living))
	{
		return 0;
	}
	const auto who = WhoOf(registry, living);
	const auto thing = reaction.source.initiator;
	// Weighed by how far it is from what it reacts to: the thing, or itself
	const auto& at = registry.Get<const Transform>(living).position;
	const float distance =
	    reaction_timing::ReactsToItself(type) ? 0.0f : gutils::GetDistanceInMetres(at, ThingAt(registry, reaction));
	const auto standard = reaction_timing::StandardTurnsToReact(reaction_timing::RowOf(*info), who, distance);
	switch (reaction_timing::ReactRule(type))
	{
	case reaction_timing::Rule::Creature:
		return static_cast<int32_t>(ecs::villager_creature::TurnsToReact(living, thing, distance));
	case reaction_timing::Rule::Predator:
		return reaction_timing::PredatorTurnsToReact(PredatorWatchOf(registry, living, thing, *info), who, standard);
	case reaction_timing::Rule::None:
		return 0;
	default:
		// A burning thing in the hand and a shield go by the standard turns for whoever holds them here (the shields'
		// villagers are timed by the shields)
		return standard;
	}
}

void ReactionSystem::Start(Active& reaction, entt::entity living, LivingReaction& state, bool takingOver)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& source = reaction.source;
	const bool nice = source.type == Reaction::LookAtNiceSpell || source.type == Reaction::ReactToImpressiveSpell;
	const bool shield = source.type == Reaction::ReactToMagicShield;
	if (registry.AllOf<Creature>(living))
	{
		// It notes when it took up a flying or crushed thing's reaction, whether it keeps it or not
		const auto setup = CreatureSetupOf(registry, reaction);
		if (setup.has_value() && setup->notesFlying)
		{
			state.sawFlying = _turn;
		}
		if (setup.has_value() && setup->notesCrushed)
		{
			state.sawCrushed = _turn;
		}
		// What it doesn't keep it heeds without being impressed by it, carrying on with what it was reacting to
		if (!CreatureKeeps(registry, living, reaction))
		{
			return;
		}
		if (setup.has_value())
		{
			state.reaction = reaction.id;
			state.type = reaction.source.type;
			state.startTurn = _turn;
			if (Locator::creatureMindSystem::has_value())
			{
				Locator::creatureMindSystem::value().TakeUpReaction(living, source.initiator, *setup);
			}
			Impress(reaction, living, registry.Get<const Transform>(living).position);
			return;
		}
	}
	// An animal flees a fire or a thing to flee at once, and takes up a flying thing's reaction only when the thing is near
	// enough to flee
	if (registry.AllOf<Animal>(living))
	{
		const auto* entry =
		    Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().Find(source.initiator) : nullptr;
		const bool flees =
		    Locator::animalSystem::has_value() &&
		    (((source.type == Reaction::ReactToFire || source.type == Reaction::FleeFromObject) &&
		      Locator::animalSystem::value().SetupFleeFromObject(living, source.initiator, source.type)) ||
		     (source.type == Reaction::ReactToFlyingObject && entry != nullptr && entry->body != nullptr &&
		      Locator::animalSystem::value().SetupReactToFlyingObject(living, source.initiator, entry->body->Speed())));
		if (!flees)
		{
			return;
		}
		state.reaction = reaction.id;
		state.type = reaction.source.type;
		state.startTurn = _turn;
		Impress(reaction, living, registry.Get<const Transform>(living).position);
		return;
	}
	const bool wasReacting = takingOver || state.reaction != 0;
	state.reaction = reaction.id;
	state.type = reaction.source.type;
	state.startTurn = _turn;
	if (registry.AllOf<Villager>(living))
	{
		// A villager is impressed only by what it takes up the usual way: not joining a dance round the thing
		const auto taken = villager_reactions::Start(living, reaction.source.type, state, wasReacting);
		if (taken == villager_reactions::Taken::No)
		{
			state.reaction = 0;
			state.type = Reaction::None;
		}
		else if (taken == villager_reactions::Taken::Impressed)
		{
			Impress(reaction, living, registry.Get<const Transform>(living).position);
		}
		return;
	}
	Impress(reaction, living, registry.Get<const Transform>(living).position);
	// A creature goes to put out a fire or runs from it
	if (source.type == Reaction::ReactToFire && registry.AllOf<Creature>(living) && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ReactToFire(living, source.initiator);
		return;
	}
	// A creature turns aside into a teleport stone
	if (source.type == Reaction::ReactToTeleport && registry.AllOf<Creature>(living) && Locator::teleportSystem::has_value())
	{
		Locator::teleportSystem::value().SetupReact(source.initiator, living);
		return;
	}
	if (!registry.AllOf<Creature>(living) || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	// A creature learns a miracle it reacts to: one another creature cast, or a player's, but not a computer player's, nor
	// for a nasty one the neutral player's
	auto& minds = Locator::creatureMindSystem::value();
	const bool byCreature = source.casterCreature != entt::null;
	const bool computer = source.player != PlayerNames::PLAYER_ONE && source.player != PlayerNames::NEUTRAL;
	// A shield, either kind, teaches the spiritual shield
	const auto magic = shield                                ? std::optional(static_cast<size_t>(MagicType::Shield))
	                   : source.magicType != MagicType::None ? std::optional(static_cast<size_t>(source.magicType))
	                                                         : std::nullopt;
	// A miracle from a seed teaches only the first creature that learns from it
	auto* seed = SeedOf(registry, source.initiator);
	const bool seedSpent = seed != nullptr && seed->learnedFrom;
	if (source.type == Reaction::FleeFromSpell)
	{
		const bool learnable = (byCreature || (!computer && source.player != PlayerNames::NEUTRAL)) && !seedSpent;
		minds.ReactToNastyMagic(living, source.position, learnable ? magic : std::nullopt);
		if (learnable && magic.has_value() && seed != nullptr)
		{
			seed->learnedFrom = true;
		}
	}
	else if ((nice || shield) && magic.has_value() && (byCreature || !computer) && !seedSpent)
	{
		// A seed's miracle a creature has already learnt from isn't looked at again
		minds.ReactToNiceMagic(living, source.position, magic);
		if (seed != nullptr)
		{
			seed->learnedFrom = true;
		}
	}
}

void ReactionSystem::Stop(entt::entity living, LivingReaction& state, bool resetState)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (state.reaction == 0)
	{
		return;
	}
	if (registry.AllOf<Villager>(living))
	{
		villager_reactions::Stop(living, state, resetState);
	}
	else if (registry.AllOf<Animal>(living) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().StopReaction(living);
	}
	state.reaction = 0;
	state.type = Reaction::None;
}

void ReactionSystem::StopReacting(entt::entity living)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* state = registry.TryGet<LivingReaction>(living);
	if (state == nullptr || state->reaction == 0)
	{
		return;
	}
	// Its memory of the kind is brought up to now, so it waits its time before taking up that kind again
	MemoryOf(registry, living, *state).Refresh(static_cast<uint32_t>(state->type), _turn);
	state->reaction = 0;
	state->type = Reaction::None;
}

void ReactionSystem::ProcessLiving()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> reacting;
	registry.Each<const LivingReaction>([&](entt::entity entity, const LivingReaction& state) {
		if (state.reaction != 0)
		{
			reacting.push_back(entity);
		}
	});
	for (const auto entity : reacting)
	{
		auto& state = registry.Get<LivingReaction>(entity);
		const auto reaction = Find(state.reaction);
		if (!reaction.has_value())
		{
			Stop(entity, state, false);
			continue;
		}
		// What it reacts to gone (deleted, or a villager dying), it stops at once; fainting or confused, it reacts to
		// itself
		const auto* info = InfoOf(state.type);
		const auto target = reaction_timing::ReactsToItself(state.type) ? entity : reaction->source.initiator;
		if (info == nullptr || !registry.AllOf<Transform>(entity) || ThingGone(registry, target))
		{
			Stop(entity, state, true);
			continue;
		}
		// Its time counts from when it last took up a reaction of the kind, and lasts by how far it is from what it reacts
		// to now
		const auto since =
		    static_cast<int32_t>(_turn - MemoryOf(registry, entity, state).LastReacted(static_cast<uint32_t>(state.type)));
		if (const auto turns = TurnsToReact(*reaction, entity, state.type); since > turns)
		{
			if (const auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_DEBUG(logger, "Reaction: {} stops reacting to kind {} after {} turns (its time {})",
				                    static_cast<uint32_t>(entity), static_cast<int>(state.type), since, turns);
			}
			Stop(entity, state, true);
		}
	}
}

void ReactionSystem::ProcessTurn()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	++_turn;
	for (auto& reaction : _reactions)
	{
		++reaction.age;
	}
	// A reaction goes once what made it has
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<uint32_t> gone;
	for (const auto& reaction : _reactions)
	{
		if (reaction.source.initiator != entt::null && !registry.Valid(reaction.source.initiator))
		{
			gone.push_back(reaction.id);
		}
	}
	for (const auto id : gone)
	{
		ShutDown(id);
	}
	// Every creature has a reaction of its own, which follows it for as long as it lives
	std::vector<Source> creatures;
	registry.Each<const Creature, const Transform>(
	    [this, &creatures](entt::entity creature, const Creature& component, const Transform& transform) {
		    const bool has = std::ranges::any_of(_reactions, [creature](const Active& reaction) {
			    return reaction.source.initiator == creature && reaction.source.type == Reaction::ReactToCreature;
		    });
		    if (!has)
		    {
			    creatures.push_back({.initiator = creature,
			                         .type = Reaction::ReactToCreature,
			                         .player = component.owner,
			                         .position = transform.position});
		    }
	    });
	for (const auto& source : creatures)
	{
		Create(source);
	}
	// A reaction to a flying thing, to a villager or a burning thing held in the hand, or to a creature, is where the
	// thing is now; a
	// creature moves the way it faces, as far as matters to those fleeing it
	for (auto& reaction : _reactions)
	{
		const bool creature = registry.Valid(reaction.source.initiator) && registry.AllOf<Creature>(reaction.source.initiator);
		const bool follows = creature || reaction.source.type == Reaction::ReactToFlyingObject ||
		                     reaction.source.type == Reaction::ReactToVillagerInHand ||
		                     reaction.source.type == Reaction::ReactToBurningObjectInHand;
		if (!follows || !registry.Valid(reaction.source.initiator))
		{
			continue;
		}
		if (const auto* transform = registry.TryGet<const Transform>(reaction.source.initiator))
		{
			reaction.source.position = transform->position;
		}
		const auto* locomotion = creature ? registry.TryGet<const CreatureLocomotion>(reaction.source.initiator) : nullptr;
		if (locomotion != nullptr && locomotion->started)
		{
			const auto facing = creature_locomotion::DirectionOf(locomotion->heading) * k_CreatureFacingMetres;
			reaction.velocity = {facing.x, 0.0f, facing.y};
		}
	}
	// One reaction a turn, in turn, grows, ends once its time is up, or is spread again
	if (!_reactions.empty())
	{
		_cursor %= _reactions.size();
		auto& reaction = _reactions.at(_cursor);
		++_cursor;
		const auto* info = InfoOf(reaction.source.type);
		if (info != nullptr && info->whetherReactionGrows != 0 && !reaction.source.reach.has_value())
		{
			reaction.reach += info->reactionGrowthPerGameTurn;
		}
		if (info != nullptr &&
		    magic::TimedOut(reaction.source.type, reaction.firstTaken, reaction.age, info->numGameTurnsForNormalThingsToReact))
		{
			ShutDown(reaction.id);
		}
		else
		{
			Spread(reaction);
		}
	}
	ProcessLiving();
	BelieveInTowns();
}

void ReactionSystem::BelieveInTowns()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	const float decay = info.player.computerPlayerBeliefChangeDecay;
	const float lostTownScale = registry.Context().mapScriptGlobals.lostTownScale;
	// Towns that change hands this turn, given to their new players once every town has had its turn
	std::vector<std::pair<entt::entity, PlayerNames>> claimed;
	registry.Each<TownImpression>([&](entt::entity entity, TownImpression& impression) {
		// Its boredom with each kind of reaction wears off, as long as it is still bored
		for (auto& [type, boredom] : impression.boredom)
		{
			if (const auto* reactionInfo = InfoOf(type))
			{
				boredom = magic::BoredomAtTownTurn(boredom, reactionInfo->additionToTownBoredomMultipliers, lostTownScale);
			}
		}
		const auto gained = magic::town_belief::Turn(impression.belief, decay);
		const auto* town = registry.TryGet<const Town>(entity);
		if (town == nullptr)
		{
			return;
		}
		// The belief gained rises as a symbol from the town centre's foot, in the believer's colour
		if (!gained.empty() && Locator::particleSystem::has_value())
		{
			if (const auto centre = TownCentreOf(registry, town->id); centre.has_value())
			{
				for (const auto& [player, amount] : gained)
				{
					villager_reactions::ShowTownBelief(*centre, player, amount);
				}
			}
		}
		// The player it believes in most takes it; a player losing it believes less in every town they hold
		const auto taker =
		    magic::town_belief::Ownership(impression.belief, town->owner, info.belief.claimedTownBeliefMultiplier);
		if (!taker.has_value())
		{
			return;
		}
		const auto loser = town->owner;
		if (loser != PlayerNames::NEUTRAL)
		{
			registry.Each<const Town>([&](entt::entity held, const Town& heldTown) {
				auto* heldImpression = heldTown.owner == loser ? registry.TryGet<TownImpression>(held) : nullptr;
				if (heldImpression != nullptr)
				{
					magic::town_belief::LostTown(heldImpression->belief, loser, info.belief.lostATownBeliefInPlayerMultiplier,
					                             lostTownScale);
				}
			});
		}
		claimed.emplace_back(entity, *taker);
	});
	if (!Locator::townSystem::has_value())
	{
		return;
	}
	for (const auto& [town, player] : claimed)
	{
		Locator::townSystem::value().ClaimTown(town, player);
	}
}

void ReactionSystem::Impress(const Active& reaction, entt::entity living, const glm::vec3& at)
{
	const auto* info = InfoOf(reaction.source.type);
	const auto& source = reaction.source;
	// What belongs to no player impresses no one
	if (info == nullptr || source.playerless)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// Measured across the land between map positions, as the game measures it
	const float distance = gutils::GetDistanceInMetres(at, source.position);
	// A reaction to food or wood impresses a villager by how much its town wants what it shows; one without a town by 1
	const auto multiplier = [&](entt::entity town) {
		std::optional<float> desire;
		if (info->correspondingTownDesire != TownDesireInfo::None && town != entt::null && registry.Valid(town))
		{
			if (const auto* wants = registry.TryGet<const TownDesire>(town))
			{
				desire = town_desire::GetDesire(*wants, static_cast<size_t>(info->correspondingTownDesire));
			}
		}
		return magic::ReactionMultiplier(source.type, info->defaultReactionImpressiveMultiplier, desire);
	};
	// An artefact worth enough makes the thing it is more impressive to whoever sees it, as long as it is worth so much
	const float artefact = Locator::artefactSystem::has_value() && source.initiator != entt::null
	                           ? Locator::artefactSystem::value().ImpressiveModifier(source.initiator)
	                           : 1.0f;
	const auto value = [&](float boredom, entt::entity town) {
		return artefact * magic::ImpressiveValue({.landBalance = GetLandBalance(source.balance),
		                                          .impressiveValue = source.impressiveValue,
		                                          .reactionMultiplier = multiplier(town),
		                                          .distance = distance,
		                                          .maxDistance = info->maxReactionDistance,
		                                          .power = source.power,
		                                          .boredom = boredom});
	};
	if (const auto* villager = registry.TryGet<const Villager>(living))
	{
		// A villager believes in the player, through its town, which tires of the same kind of thing
		if (villager->town == entt::null || !registry.Valid(villager->town))
		{
			return;
		}
		auto* impression = &villager_reactions::ImpressionOf(villager->town);
		auto& boredom = impression->boredom.try_emplace(source.type, 1.0f).first->second;
		const float share = villager_reactions::TownShare(villager->town);
		const float belief = value(boredom, villager->town) * share;
		magic::town_belief::Add(impression->belief, source.player, belief);
		impression->lastImpression = belief;
		villager_reactions::ShowBelief(living, source.player, belief, info->alignmentForSFX, _voice, _turn);
		// Whatever a villager is impressed by moves the player's alignment a little, by how much its town wants what it
		// shows for those kinds that look to a desire
		std::optional<float> desire;
		if (info->correspondingTownDesireForAlignment != TownDesireInfo::None)
		{
			if (const auto* wants = registry.TryGet<const TownDesire>(villager->town))
			{
				desire = town_desire::GetRawDesire(*wants, static_cast<size_t>(info->correspondingTownDesireForAlignment));
			}
		}
		if (Locator::alignmentSystem::has_value())
		{
			auto& alignment = Locator::alignmentSystem::value();
			alignment.AddPendingAlignment(
			    source.player, magic::DampAlignmentChange(magic::ImpressionAlignment(info->alignmentModifier, desire),
			                                              alignment.GetPlayerAlignment(source.player)));
		}
		// The town tires of the kind of thing, by its share of the town
		if (Locator::infoConstants::has_value())
		{
			boredom = magic::BoredomAfterImpression(boredom, Locator::infoConstants::value().belief.defaultBoredomOfMe * share);
		}
		return;
	}
	if (const auto* creature = registry.TryGet<const Creature>(living))
	{
		// A creature is impressed by what its own player does, by a creature's hand or the player's, and by what another
		// creature itself does; a miracle another player made impresses it not at all. Its own doings don't impress it.
		if (living == source.initiator)
		{
			return;
		}
		// A creature has no town
		const float base = value(1.0f, entt::null);
		auto* impression = registry.TryGet<CreatureImpression>(living);
		if (impression == nullptr)
		{
			impression = &registry.Assign<CreatureImpression>(living);
		}
		if (creature->owner == source.player)
		{
			impression->byOwnPlayer += base * magic::k_CreatureImpressedByOwnPlayer;
		}
		else if (registry.Valid(source.initiator) && registry.AllOf<Creature>(source.initiator))
		{
			impression->byOtherCreatures += base * magic::k_CreatureImpressedByOtherCreature;
		}
	}
}

void ReactionSystem::GiveTownBelief(entt::entity town, PlayerNames player, float amount, entt::entity from,
                                    GuidanceAlignment alignment)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	magic::town_belief::Add(villager_reactions::ImpressionOf(town).belief, player, amount);
	if (from != entt::null && registry.Valid(from) && registry.AllOf<Transform>(from))
	{
		villager_reactions::ShowObjectBelief(from, town, player, amount, alignment, _voice, _turn);
	}
}

void ReactionSystem::Reset()
{
	_reactions.clear();
	_cursor = 0;
	_turn = 0;
	_voice = {};
	_landBalances = {1.0f, 1.0f, 1.0f};
}

std::vector<ReactionSystemInterface::Active> ReactionSystem::ReactionsAt(const glm::vec3& point) const
{
	std::vector<Active> found;
	std::ranges::copy_if(_reactions, std::back_inserter(found), [&point](const Active& reaction) {
		return glm::distance(point, reaction.source.position) <= reaction.reach;
	});
	return found;
}
