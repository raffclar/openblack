/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindWar.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/FieldCrop.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ComputerMindCommon.h"
#include "ComputerMindConquest.h"
#include "ComputerMindCreature.h"
#include "ComputerMindMiracles.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/ComputerMindWarRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/TownAreaRules.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;

namespace
{

/// The leash with which the god sends its creature to attack
constexpr int32_t k_AttackLeash = static_cast<int32_t>(LeashType::Evil);

/// The thing is in the world and has the component
template <typename T>
bool Is(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && Entities().AllOf<T>(thing);
}

/// The first creature that the player has, none when the land has no such player or creature
entt::entity PrimaryCreatureOf(PlayerNames player)
{
	if (!Locator::playerSystem::has_value())
	{
		return entt::null;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player).value_or(entt::null);
	return Is<Creature>(creature) ? creature : entt::null;
}

/// The town of a thing: the thing itself when it is a town, else the town that the building, the villager or the field
/// belongs to
entt::entity TownOfThing(entt::entity thing)
{
	auto& registry = Entities();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return entt::null;
	}
	if (registry.AllOf<Town>(thing))
	{
		return thing;
	}
	if (registry.AllOf<Abode>(thing))
	{
		return ecs::town_homes::TownOfAbode(registry, thing);
	}
	if (const auto* villager = registry.TryGet<const Villager>(thing); villager != nullptr)
	{
		return villager->town;
	}
	if (const auto* field = registry.TryGet<const Field>(thing); field != nullptr)
	{
		return TownNumbered(field->town);
	}
	return entt::null;
}

/// The centre of the town, none when it has none
entt::entity TownCentreOf(const Town& town)
{
	for (const auto abode : town.abodes)
	{
		const auto* info = Entities().Valid(abode) ? ecs::world_objects::AbodeInfoOf(abode) : nullptr;
		if (info != nullptr && info->abodeType == AbodeType::TownCentre)
		{
			return abode;
		}
	}
	return entt::null;
}

/// The magic type of the miracle of the icon
int32_t TypeOfIcon(entt::entity icon)
{
	return static_cast<int32_t>(PlainType(Entities().Get<const WorshipIcon>(icon).seed));
}

/// The value of a target in a town times its nearness to the hand
float NearValue(float value, entt::entity target, const map_coords::MapCoords& hand)
{
	const auto coords = CoordsOfThing(target);
	const float distance = coords.has_value() ? gutils::GetDistanceInMetres(hand, *coords) : 0.0f;
	const float nearness = gutils::GetDistanceModifier(distance, computer_mind_rules::k_TownTargetReach);
	return computer_mind_rules::NearTargetValue(value, nearness);
}

// --- Throw a thing at a player ---

/// The god never wants to throw a thing at a player
float ThrowAtPlayerWish(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return 0.0f;
}

/// The god has no steps that throw a thing at a player, so the action always fails
bool ThrowAtPlayer(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return false;
}

// --- Destroy a town ---

/// The icon of the miracle that the god most wants to cast at the town above the node is the node's object. The god aims
/// at the target in the town that it wants most, else at the middle of the town.
float DestroyTownWithSpellWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto above = ObjectAboveOf(mind, node);
	map_coords::MapCoords place {};
	if (Is<Town>(above))
	{
		const auto target = TownTarget(above, context.state.hand.coords);
		if (target != entt::null)
		{
			place = CoordsOfThing(target).value_or(place);
		}
		else
		{
			place = ecs::town_area::Centre(Entities().Get<const Town>(above).area);
		}
	}
	else if (const auto coords = CoordsOfThing(above))
	{
		place = *coords;
	}
	entt::entity icon = entt::null;
	const float best = BestSpellOfKind(context, computer_mind_rules::SpellKind::Attack, place, icon);
	if (icon != entt::null)
	{
		mind.nodes[node].object = icon;
	}
	return best;
}

/// The hand reaches the icon, makes the worshippers chant and casts the miracle at the target in the town above the node
bool DestroyTownWithSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto target = ObjectAboveOf(mind, node);
	const auto icon = ObjectOf(mind, node);
	if (target == entt::null || !Is<WorshipIcon>(icon))
	{
		return false;
	}
	if (Is<Town>(target))
	{
		target = TownTarget(target, context.state.hand.coords);
		if (target == entt::null)
		{
			return false;
		}
	}
	else if (!CoordsOfThing(target).has_value())
	{
		return false;
	}
	ReachAndChant(context, icon);
	CastAtThing(context, target, icon, 0.0f);
	return true;
}

/// The god puts the leash on its creature and ties it to the town of the thing above the node. The god counts the
/// times that it sends the creature while the creature knows no angry miracle.
bool DestroyTownWithCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto above = ObjectAboveOf(mind, node);
	if (!CoordsOfThing(above).has_value())
	{
		return false;
	}
	const auto town = TownOfThing(above);
	const auto creature = ObjectOf(mind, node);
	if (!Is<Town>(town) || !Is<Creature>(creature))
	{
		return false;
	}
	const auto& owned = Entities().Get<const Town>(town);
	auto target = WorkingPitOf(town).value_or(entt::null);
	if (target == entt::null)
	{
		target = TownCentreOf(owned);
	}
	if (target == entt::null && !owned.abodes.empty())
	{
		target = owned.abodes.front();
	}
	if (target == entt::null)
	{
		return false;
	}
	if (KnownFightMiracleShare(creature, CreatureDesires::Anger) < 1.0f)
	{
		++context.state.mind.untaughtSends.at(static_cast<size_t>(CreatureDesires::Anger));
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::SetLeash, {Argument {.object = creature}, Argument {.number = k_AttackLeash}});
	computer_hand::Add(hand, StepKind::AttachLeashToObject, {Argument {.object = creature}, Argument {.object = target}});
	return true;
}

/// The hand picks up the node's thing and throws it at the town above the node. The thing must be an object. Thus a
/// node that takes the town above it as its thing never throws.
bool DestroyTownWithObject(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto thing = ObjectOf(mind, node);
	const auto town = ObjectAboveOf(mind, node);
	if (thing == entt::null || !Is<Town>(town))
	{
		return false;
	}
	const auto coords = CoordsOfThing(town);
	if (!coords.has_value() || Is<Town>(thing) || !CoordsOfThing(thing).has_value())
	{
		return false;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::PickUpObject, {Argument {.object = thing}, Argument {.value = context.state.speed}});
	computer_hand::Add(hand, StepKind::ThrowObjectInHandAtPos,
	                   {Argument {.place = computer_hand::steps::PlaceOf(context.hand, *coords)}});
	return true;
}

// --- Attack a creature ---

/// The creature of the player above the node is the node's object. The god wants to attack it more when it is nearer
/// to the hand. The god does not attack its own creature or the creature of a player that has the god as its ally.
float AttackCreatureWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto player = ObjectAboveOf(mind, node);
	if (!Is<Player>(player))
	{
		return 0.0f;
	}
	const auto other = Entities().Get<const Player>(player).name;
	if (other == context.player)
	{
		return 0.0f;
	}
	const auto creature = PrimaryCreatureOf(other);
	const auto coords = creature != entt::null ? CoordsOfThing(creature) : std::nullopt;
	if (!coords.has_value() || Locator::playerSystem::value().AllyStrength(other, context.player) > 0.0f)
	{
		return 0.0f;
	}
	const float distance = gutils::GetDistanceInMetres(context.state.hand.coords, *coords);
	const float nearness = gutils::GetDistanceModifier(distance, computer_mind_rules::k_AttackCreatureReach);
	mind.nodes[node].object = creature;
	return computer_mind_rules::AttackCreatureWish(distance, nearness);
}

/// The hand goes near the creature above the node, makes the worshippers chant and casts the miracle of the node's icon
/// at the creature, two or three times
bool AttackCreatureWithSpells(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto target = ObjectAboveOf(mind, node);
	const auto icon = ObjectOf(mind, node);
	if (!Is<Creature>(target) || !Is<WorshipIcon>(icon) || !Locator::gameRandom::has_value())
	{
		return false;
	}
	const auto type = TypeOfIcon(icon);
	auto& random = Locator::gameRandom::value();
	auto& hand = context.state.hand;
	const auto casts = [&random]() {
		return random.GameRand(computer_mind_rules::k_AttackCastRange) + computer_mind_rules::k_LeastAttackCasts;
	};
	// The god draws the number of casts again after each cast
	for (uint32_t cast = 0; cast < casts(); ++cast)
	{
		computer_hand::Add(hand, StepKind::MoveHandNearObject, {Argument {.object = target}});
		computer_hand::Add(hand, StepKind::MakeWorshippersChantForSpell,
		                   {Argument {.number = type}, Argument {.object = icon}});
		computer_hand::Add(hand, StepKind::CastSpellAtObject,
		                   {Argument {.object = target}, Argument {.number = type}, Argument {.object = icon}});
	}
	return true;
}

/// The god's creature is the node's object when the god can send it at the creature above the node and its leash is
/// not yet tied to that creature
float AttackCreatureWithCreatureWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto target = ObjectAboveOf(mind, node);
	const auto own = CreatureOf(context.player);
	if (own == entt::null || !Is<Creature>(target))
	{
		return 0.0f;
	}
	const auto* leash = Entities().TryGet<const CreatureLeash>(own);
	if (leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == target)
	{
		return 0.0f;
	}
	mind.nodes[node].object = own;
	return 1.0f;
}

/// The god puts the leash on its creature and ties it to the creature above the node
bool AttackCreatureWithCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto target = ObjectAboveOf(mind, node);
	const auto own = ObjectOf(mind, node);
	if (!Is<Creature>(target) || !Is<Creature>(own))
	{
		return false;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::SetLeash, {Argument {.object = own}, Argument {.number = k_AttackLeash}});
	computer_hand::Add(hand, StepKind::AttachLeashToObject, {Argument {.object = own}, Argument {.object = target}});
	return true;
}

/// The score of a thing that the god can throw at a creature
float RockScoreOf(entt::entity thing)
{
	const auto* dynamics = Locator::dynamicsSystem::has_value() ? &Locator::dynamicsSystem::value() : nullptr;
	return computer_mind_rules::RockScore({
	    .throwable = CanPlayerThrow(thing),
	    .destroysBuildings = dynamics != nullptr && dynamics->PhysicallyDestroysAbodes(thing),
	    .tree = Entities().AllOf<Tree>(thing),
	    .weight = dynamics != nullptr ? dynamics->WeightOf(thing) : 0.0f,
	});
}

/// The heaviest rock or tree near the hand is the node's object. With a creature above the node, the god wants to
/// throw as much as it wants a place past the edge of its influence, toward the creature. Else it fully wants to.
float ThrowRockWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	float wish = 1.0f;
	const auto target = ObjectAboveOf(mind, node);
	if (const auto targetCoords = Is<Creature>(target) ? CoordsOfThing(target) : std::nullopt)
	{
		const auto own = PrimaryCreatureOf(context.player);
		if (const auto ownCoords = own != entt::null ? CoordsOfThing(own) : std::nullopt)
		{
			const float distance = gutils::GetDistanceInMetres(*targetCoords, *ownCoords);
			if (computer_mind_rules::TooNearOwnCreature(distance, Entities().Get<const Creature>(own).size,
			                                            Entities().Get<const Creature>(target).size))
			{
				return 0.0f;
			}
		}
		const auto edge = InfluenceEdgeOf(context, *targetCoords);
		if (!edge.has_value())
		{
			return 0.0f;
		}
		wish = PullPastEdge(context, *edge, *targetCoords);
	}
	const auto rock = BestThingNear(context, context.state.hand.coords, computer_mind_rules::k_RockSearchRadius, RockScoreOf);
	if (rock == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = rock;
	return wish;
}

/// The hand picks up the node's rock and throws it at the creature above the node
bool ThrowRock(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto rock = ObjectOf(mind, node);
	const auto target = ObjectAboveOf(mind, node);
	const auto coords = Is<Creature>(target) ? CoordsOfThing(target) : std::nullopt;
	if (!coords.has_value() || rock == entt::null || !CoordsOfThing(rock).has_value())
	{
		return false;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::PickUpObject, {Argument {.object = rock}, Argument {.value = context.state.speed}});
	computer_hand::Add(hand, StepKind::ThrowObjectInHandAtPos,
	                   {Argument {.place = computer_hand::steps::PlaceOf(context.hand, *coords)}});
	return true;
}

} // namespace

entt::entity computer_world::TownTarget(entt::entity town, const map_coords::MapCoords& hand)
{
	if (!Is<Town>(town))
	{
		return entt::null;
	}
	auto& registry = Entities();
	const auto& owned = registry.Get<const Town>(town);
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	std::vector<entt::entity> targets;
	std::vector<float> values;
	// The buildings come first, then the fields, each in the order of the town
	for (const auto abode : owned.abodes)
	{
		if (!registry.Valid(abode) || registry.AllOf<Field>(abode))
		{
			continue;
		}
		const auto* abodeInfo = ecs::world_objects::AbodeInfoOf(abode);
		const auto* progress = registry.TryGet<const BuildProgress>(abode);
		const float built = progress != nullptr ? progress->built : 1.0f;
		const float desire = abodeInfo != nullptr ? abodeInfo->computerAttackDesire : 0.0f;
		const float value = computer_mind_rules::BuildingTargetValue(built, ecs::world_objects::LifeOf(abode), desire);
		targets.push_back(abode);
		values.push_back(NearValue(value, abode, hand));
	}
	for (const auto abode : owned.abodes)
	{
		const auto* field = registry.Valid(abode) ? registry.TryGet<const Field>(abode) : nullptr;
		const auto row = field != nullptr ? static_cast<size_t>(field->type) : 0;
		if (field == nullptr || info == nullptr || row >= info->fieldType.size())
		{
			continue;
		}
		const auto& fieldInfo = info->fieldType.at(row);
		const float value =
		    computer_mind_rules::FieldTargetValue(field->crop.food, fieldInfo.totalFoodInField, fieldInfo.computerAttackDesire);
		targets.push_back(abode);
		values.push_back(NearValue(value, abode, hand));
	}
	const auto best = computer_mind_rules::MostWantedTarget(values);
	return best.has_value() ? targets.at(*best) : entt::null;
}

void computer_world::RegisterWarRules(RuleTable& table)
{
	using computer_mind::MindDomain;
	using computer_mind::NodeKind;
	AddRule(table, MindDomain::War, NodeKind::ActionThrowThingAtPlayer,
	        {.intensity = ThrowAtPlayerWish, .perform = ThrowAtPlayer});
	// The god wants to destroy a town as much as it wants to win it
	AddRule(table, MindDomain::War, NodeKind::ActionDestroyTown, {.intensity = TakeOverTown});
	AddRule(table, MindDomain::War, NodeKind::ActionAttackCreature, {.intensity = AttackCreatureWish});
	AddRule(table, MindDomain::War, NodeKind::ObjectDestroytownWithSpell,
	        {.intensity = DestroyTownWithSpellWish, .perform = DestroyTownWithSpell});
	// The god wants to send its creature to destroy a town as much as it wants to send it to impress the town
	AddRule(table, MindDomain::War, NodeKind::ObjectDestroytownWithCreature,
	        {.intensity = CreatureToTown, .perform = DestroyTownWithCreature});
	AddRule(table, MindDomain::War, NodeKind::ObjectDestroytownWithObject, {.perform = DestroyTownWithObject});
	AddRule(table, MindDomain::War, NodeKind::ObjectAttackCreatureWithSpells,
	        {.intensity = AggressiveSpell, .perform = AttackCreatureWithSpells});
	AddRule(table, MindDomain::War, NodeKind::ObjectAttackCreatureWithCreature,
	        {.intensity = AttackCreatureWithCreatureWish, .perform = AttackCreatureWithCreature});
	AddRule(table, MindDomain::War, NodeKind::ObjectThrowrockatCreature, {.intensity = ThrowRockWish, .perform = ThrowRock});
}
