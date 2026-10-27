/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/vec3.hpp>

#include "Common/GameRandom.h"
#include "ComputerMindCommon.h"
#include "ComputerMindCreature.h"
#include "ComputerMindMiracles.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::MindDomain;
using computer_mind::NodeKind;

namespace
{

/// The miracles that the god casts on its own creature for a laugh: the first, and the count of them
constexpr auto k_FirstLaughSpell = MagicType::CreatureSpellFreeze;
constexpr uint32_t k_LaughSpellCount = 5;

[[nodiscard]] bool IsCreature(entt::entity thing)
{
	const auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Creature>(thing);
}

[[nodiscard]] bool HasPlace(entt::entity thing)
{
	return thing != entt::null && CoordsOfThing(thing).has_value();
}

/// The creature is the object of the node. The god wants the play as much as a random number in the range, plus the
/// least wish. When the god has no creature, it has no wish and draws no random number.
float PlayWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node, float range, float least)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	const float draw = Locator::gameRandom::value().GameFloatRand(range);
	return draw + least;
}

float ThrowFoodWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return PlayWish(context, mind, node, computer_mind_rules::k_ThrowFoodWishRange, computer_mind_rules::k_ThrowFoodLeastWish);
}

float SlapWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return PlayWish(context, mind, node, computer_mind_rules::k_SlapWishRange, 0.0f);
}

float StrokeWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return PlayWish(context, mind, node, computer_mind_rules::k_StrokeWishRange, computer_mind_rules::k_StrokeLeastWish);
}

/// The god wants to tie the leash only while the creature wears no leash
float LeashWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	const auto* leash = Entities().TryGet<const CreatureLeash>(creature);
	if (leash != nullptr && leash->worn.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	const float draw = Locator::gameRandom::value().GameFloatRand(computer_mind_rules::k_LeashWishRange);
	return draw + computer_mind_rules::k_LeashLeastWish;
}

/// The god never wants this node, and no node above it in the tree lists it
float NoWish(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return 0.0f;
}

/// The god wants a laugh only while no miracle acts on its creature
float LaughWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	return context.hand.casts.UnderSpell(creature) ? 0.0f : computer_mind_rules::k_LaughWish;
}

/// The best miracle to attack with at the place of the thing above the node. Its icon becomes the object of the node.
/// The part for a town above the node belongs to the attack on towns.
float LaughSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto above = ObjectAboveOf(mind, node);
	// When the node has nothing above it, the god looks at the corner of the map
	const auto place = above != entt::null ? CoordsOfThing(above).value_or(map_coords::MapCoords {}) : map_coords::MapCoords {};
	entt::entity icon = entt::null;
	const float best = BestSpellOfKind(context, computer_mind_rules::SpellKind::Attack, place, icon);
	if (icon != entt::null)
	{
		mind.nodes[node].object = icon;
	}
	return best;
}

/// The hand picks up the food, goes near the creature and throws the food at it
bool ThrowFood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto food = LeafObjectOf(mind, node);
	const auto creature = ObjectOf(mind, node);
	if (!HasPlace(food) || !IsCreature(creature))
	{
		return false;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::PickUpObject, {Argument {.object = food}, Argument {.value = context.state.speed}});
	const auto at = computer_hand::steps::PlaceOf(context.hand, *CoordsOfThing(creature));
	computer_hand::Add(hand, StepKind::MoveHandToPos,
	                   {Argument {.place = computer_mind_rules::ThrowFoodHandPlace(hand.hand, at)}});
	const float size = Entities().Get<const Creature>(creature).size;
	auto& random = Locator::gameRandom::value();
	const float firstDraw = random.GameFloatRand(computer_mind_rules::k_ThrowFoodScatter);
	const float secondDraw = random.GameFloatRand(computer_mind_rules::k_ThrowFoodScatter);
	computer_hand::Add(hand, StepKind::ThrowObjectInHandAtPos,
	                   {Argument {.place = computer_mind_rules::ThrowFoodAim(at, size, firstDraw, secondDraw)}});
	return true;
}

/// The hand goes to the head of the creature and touches it
bool Touch(RuleContext& context, computer_mind::Mind& mind, uint16_t node, StepKind touch)
{
	const auto creature = ObjectOf(mind, node);
	if (!IsCreature(creature))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::MoveHandToCreatureHead, {Argument {.object = creature}});
	computer_hand::Add(context.state.hand, touch, {Argument {.object = creature}});
	return true;
}

bool Slap(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return Touch(context, mind, node, StepKind::SlapCreature);
}

bool Stroke(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return Touch(context, mind, node, StepKind::StrokeCreature);
}

/// The creature gets the speed for its journey to the thing. Then the hand goes to the thing, and the god ties the leash
/// of the creature to it.
bool TieLeash(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto thing = LeafObjectOf(mind, node);
	const auto creature = ObjectOf(mind, node);
	if (!HasPlace(thing) || !IsCreature(creature))
	{
		return false;
	}
	auto& hand = context.state.hand;
	const auto at = computer_hand::steps::PlaceOf(context.hand, *CoordsOfThing(thing));
	computer_hand::Add(hand, StepKind::SetCreatureSpeedForJourney, {Argument {.object = creature}, Argument {.place = at}});
	computer_hand::Add(hand, StepKind::MoveHandToObject, {Argument {.object = thing}});
	computer_hand::Add(hand, StepKind::AttachLeashToObject, {Argument {.object = creature}, Argument {.object = thing}});
	return true;
}

/// The worshippers chant at the icon of the best attack miracle. But the hand casts a random creature miracle on the
/// creature, and it waits until the miracle stops.
bool Laugh(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = ObjectAboveOf(mind, node);
	const auto icon = ObjectOf(mind, node);
	if (!IsCreature(creature) || context.hand.casts.UnderSpell(creature) || icon == entt::null || !Entities().Valid(icon) ||
	    !Entities().AllOf<WorshipIcon>(icon))
	{
		return false;
	}
	const auto draw = Locator::gameRandom::value().GameRand(k_LaughSpellCount);
	const auto type = static_cast<int32_t>(static_cast<uint32_t>(k_FirstLaughSpell) + draw);
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::MoveHandToObject, {Argument {.object = icon}});
	computer_hand::Add(hand, StepKind::MakeWorshippersChantForSpell, {Argument {.number = type}, Argument {.object = icon}});
	computer_hand::Add(hand, StepKind::CastSpellAtObject,
	                   {Argument {.object = creature}, Argument {.number = type}, Argument {.object = icon}});
	computer_hand::Add(hand, StepKind::WaitForSpellToWearOff, {Argument {.object = creature}});
	return true;
}

} // namespace

void computer_world::CheckCreatureLeash(PlayerNames player)
{
	const auto creature = CreatureOf(player);
	if (creature == entt::null || !Locator::leashSystem::has_value())
	{
		return;
	}
	const auto* leash = Entities().TryGet<const CreatureLeash>(creature);
	if (leash == nullptr || !leash->worn.has_value() || !leash->worn->tiedTo.has_value() ||
	    leash->worn->type != LeashType::Evil || !IsCreature(*leash->worn->tiedTo))
	{
		return;
	}
	const auto coords = CoordsOfThing(*leash->worn->tiedTo);
	const float influence = coords.has_value() && Locator::influenceSystem::has_value()
	                            ? Locator::influenceSystem::value().PlayerInfluence(player, *coords)
	                            : 0.0f;
	if (computer_mind_rules::UntiesEvilLeash(influence, PlanDesireOf(creature) == creature_desires::Desire::RestoreHealth))
	{
		Locator::leashSystem::value().TakeOff(creature);
	}
}

void computer_world::RegisterCreaturePlayRules(RuleTable& table)
{
	constexpr auto k_Play = MindDomain::CreaturePlay;
	AddRule(table, k_Play, NodeKind::ActionThrowFoodatCreature, {.intensity = ThrowFoodWish, .perform = ThrowFood});
	AddRule(table, k_Play, NodeKind::ActionSlapCreature, {.intensity = SlapWish, .perform = Slap});
	AddRule(table, k_Play, NodeKind::ActionStrokeCreature, {.intensity = StrokeWish, .perform = Stroke});
	AddRule(table, k_Play, NodeKind::ActionAttachLeashtoMovingObject, {.intensity = LeashWish, .perform = TieLeash});
	AddRule(table, k_Play, NodeKind::ObjectObjectToLeashTo, {.intensity = NoWish});
	AddRule(table, k_Play, NodeKind::ActionCastSpellOnMyCreatureForALaugh, {.intensity = LaughWish});
	AddRule(table, k_Play, NodeKind::ObjectCastSpellOnMyCreatureForALaugh, {.intensity = LaughSpell, .perform = Laugh});
}
