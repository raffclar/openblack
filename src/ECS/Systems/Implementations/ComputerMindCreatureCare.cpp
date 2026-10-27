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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/CreatureBody.h"
#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureLayers.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::MindDomain;
using computer_mind::NodeKind;
using creature_desires::Desire;

namespace
{

/// The actions of the game's creature table that the god orders, by their rows
constexpr uint32_t k_SleepAtHomeAction = 17;
constexpr uint32_t k_DrinkFromTheSeaAction = 55;
constexpr uint32_t k_GoHomeAction = 162;

/// The creature of the god, none when it has none or a script controls it
entt::entity CreatureOf(PlayerNames player)
{
	if (!Locator::playerSystem::has_value())
	{
		return entt::null;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	auto& registry = Entities();
	if (!creature.has_value() || !registry.Valid(*creature) || registry.AllOf<ScriptControlled>(*creature))
	{
		return entt::null;
	}
	return *creature;
}

/// The body of the creature plays an action, walks, turns or catches
bool BodyBusy(entt::entity creature)
{
	const auto* animation = Entities().TryGet<const CreatureAnimation>(creature);
	const bool playing = animation != nullptr && creature_layers::IsPlaying(animation->body);
	const bool moving =
	    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
	const bool catching =
	    Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().IsCatching(creature);
	return playing || moving || catching;
}

/// The needs of the body of the creature, as the body starts when it has none yet
creature_physiology::Needs NeedsOf(entt::entity creature)
{
	const auto* needs = Entities().TryGet<const CreatureNeeds>(creature);
	return needs != nullptr ? needs->needs : creature_physiology::Needs {};
}

/// How much the creature wants a thing now
float DesireOf(entt::entity creature, Desire desire)
{
	const auto* mind = Entities().TryGet<const CreatureMindState>(creature);
	return mind != nullptr && mind->desires.has_value() ? (*mind->desires)[desire].value : 0.0f;
}

/// The desire of the plan that the creature made last, if any
std::optional<Desire> PlanDesireOf(entt::entity creature)
{
	const auto* mind = Entities().TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planner.current.has_value())
	{
		return std::nullopt;
	}
	return mind->planner.current->desire;
}

/// How free the creature is for the god. On the god's leash, the creature is not free for a time after the leash is tied.
float FreedomOf(PlayerNames player, entt::entity creature)
{
	const auto* leash = Entities().TryGet<const CreatureLeash>(creature);
	const bool leashed = leash != nullptr && leash->worn.has_value() && leash->worn->holder == player;
	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const auto needs = NeedsOf(creature);
	return computer_mind_rules::CreatureFreedom({
	    .leashed = leashed,
	    .turnsSinceTied = leashed ? turn - leash->worn->tiedTurn : 0,
	    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
	    .life = needs.life,
	    .exhaustion = needs.exhaustion,
	    .energy = needs.energy,
	    .restoringHealth = PlanDesireOf(creature) == Desire::RestoreHealth,
	});
}

/// The god wants to interact with its creature as much as the creature is free, but not while its body is busy
float CreatureInteract(RuleContext& context, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null || BodyBusy(creature))
	{
		return 0.0f;
	}
	return FreedomOf(context.player, creature);
}

/// The creature is the node's object. The god wants the action as much as the creature has the desire, times a share.
float CreatureDesire(RuleContext& context, computer_mind::Mind& mind, uint16_t node, Desire desire, float share)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	return DesireOf(creature, desire) * share;
}

float Hungry(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CreatureDesire(context, mind, node, Desire::Hunger, 1.0f);
}

float Thirsty(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CreatureDesire(context, mind, node, Desire::Water, 1.0f);
}

float Tired(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CreatureDesire(context, mind, node, Desire::Tiredness, 1.0f);
}

float FarFromHome(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CreatureDesire(context, mind, node, Desire::StayNearHome, 1.0f);
}

float Cold(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CreatureDesire(context, mind, node, Desire::GetWarmer, computer_mind_rules::k_WarmerShare);
}

float Hot(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CreatureDesire(context, mind, node, Desire::GetColder, computer_mind_rules::k_CoolerShare);
}

/// The god sees that its creature starves when the energy of the creature is at the low mark of its species or under
float Starving(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& infos = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(Entities().Get<const Creature>(creature).species);
	if (row >= infos.size() || NeedsOf(creature).energy > infos.at(row).lowEnergyThreshold)
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	const float wish = computer_mind_rules::StarvingWish(DesireOf(creature, Desire::Hunger));
	mind.nodes[node].amount = computer_mind_rules::StarvingAmount(wish);
	return wish;
}

/// The god sees that its creature is badly hurt, and wants to heal it as much as its life is short
float Damaged(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	const float life = NeedsOf(creature).life;
	if (life > computer_mind_rules::k_DamagedLife)
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	return 1.0f - life;
}

/// The god never sees its creature ill: the body keeps no share of illness that is more than 0
float Ill(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return 0.0f;
}

/// How much the god wants to cast the food miracle on its creature: the same as on the store of a town, at the place of
/// the creature
float CastFoodOnCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto& rule = Rules().at(static_cast<size_t>(NodeKind::ObjectShallCastFoodSpell));
	return rule.intensity != nullptr ? rule.intensity(context, mind, node) : 0.0f;
}

/// How much the god wants to cast the heal miracle: the share of the charge that is there, less the further the charge
/// is from done. The icon is the node's object.
float CastHeal(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const Player* player = nullptr;
	Entities().Each<const Player>([&player, &context](const Player& each) {
		if (each.name == context.player && player == nullptr)
		{
			player = &each;
		}
	});
	if (player == nullptr || !player_miracles::IsMagicTypeEnabled(player->miracles, MagicType::Heal))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	float charge = ChargeLevel(context, MagicType::Heal, icon);
	// Short of a full charge the god asks again, so each look counts two requests
	charge = charge < 1.0f ? ChargeLevel(context, MagicType::Heal, icon) : 1.0f;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	const float turns = computer_mind_rules::TurnsToCharge(FactsOf(icon));
	if (turns < 0.0f)
	{
		return 0.0f;
	}
	mind.nodes[node].object = icon;
	return computer_mind_rules::SpellValue(charge, turns, 1.0f);
}

/// The object of the node, or of the first node above it that has one
entt::entity ObjectOf(const computer_mind::Mind& mind, uint16_t node)
{
	for (std::optional<uint16_t> at = node; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return mind.nodes[*at].object;
		}
	}
	return entt::null;
}

/// The object of the first node above the node that has one
entt::entity ObjectAboveOf(const computer_mind::Mind& mind, uint16_t node)
{
	const auto parent = mind.nodes[node].parent;
	return parent.has_value() ? ObjectOf(mind, *parent) : entt::null;
}

/// The object of the leaf that the node chose, or of the first node above that leaf that has one
entt::entity LeafObjectOf(const computer_mind::Mind& mind, uint16_t node)
{
	auto leaf = node;
	while (mind.nodes[leaf].bestLeaf.has_value() && *mind.nodes[leaf].bestLeaf != leaf)
	{
		leaf = *mind.nodes[leaf].bestLeaf;
	}
	return ObjectOf(mind, leaf);
}

/// A thing that is still in the world and has a place
bool HasPlace(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && CoordsOfThing(thing).has_value();
}

bool IsCreature(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && Entities().AllOf<Creature>(thing);
}

/// The hand takes the thing of the chosen leaf, then drops it on the creature or gives it to the creature
bool TakeToCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node, StepKind give)
{
	const auto thing = LeafObjectOf(mind, node);
	const auto creature = ObjectOf(mind, node);
	if (!HasPlace(thing) || !HasPlace(creature))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = thing}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, give, {Argument {.object = creature}});
	return true;
}

bool GiveFood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return TakeToCreature(context, mind, node, StepKind::DropObjectInHandOnObject);
}

bool FeedStarving(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return TakeToCreature(context, mind, node, StepKind::GiveObjectInHandToCreature);
}

/// The worshippers chant for the miracle at the node's icon, and the hand casts it on the creature above
bool CastOnCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node, MagicType type)
{
	const auto creature = ObjectAboveOf(mind, node);
	const auto icon = ObjectOf(mind, node);
	if (!IsCreature(creature) || icon == entt::null || !Entities().Valid(icon) || !Entities().AllOf<WorshipIcon>(icon))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::MoveHandToObject, {Argument {.object = icon}});
	computer_hand::Add(context.state.hand, StepKind::MakeWorshippersChantForSpell,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	computer_hand::Add(
	    context.state.hand, StepKind::CastSpellAtObject,
	    {Argument {.object = creature}, Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	return true;
}

bool CastFood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CastOnCreature(context, mind, node, MagicType::Food);
}

bool CastHealOnCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return CastOnCreature(context, mind, node, MagicType::Heal);
}

/// The place in metres of a point on the land
glm::vec3 PlaceOnLand(RuleContext& context, glm::vec2 point)
{
	const map_coords::MapCoords coords {.x = map_coords::ToFixed(point.x), .z = map_coords::ToFixed(point.y), .altitude = 0.0f};
	return computer_hand::steps::PlaceOf(context.hand, coords);
}

/// The god sets the speed of the creature for its journey to a place, the hand goes to the creature, and the creature
/// gets the order to do the action
void SendCreature(RuleContext& context, entt::entity creature, std::optional<glm::vec3> journeyTo, uint32_t action)
{
	const auto coords = CoordsOfThing(creature);
	const auto at = coords.has_value() ? computer_hand::steps::PlaceOf(context.hand, *coords) : glm::vec3 {0.0f};
	if (journeyTo.has_value())
	{
		computer_hand::Add(context.state.hand, StepKind::SetCreatureSpeedForJourney,
		                   {Argument {.object = creature}, Argument {.place = *journeyTo}});
	}
	computer_hand::Add(context.state.hand, StepKind::MoveHandToPos, {Argument {.place = at}});
	computer_hand::Add(
	    context.state.hand, StepKind::ForceCreatureToPerformAction,
	    {Argument {.object = creature}, Argument {.number = static_cast<int32_t>(action)}, Argument {.object = creature}});
}

/// The creature goes to drink at the coast nearest to it on its map of the land
bool Drink(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = ObjectOf(mind, node);
	const auto coords = IsCreature(creature) ? CoordsOfThing(creature) : std::nullopt;
	if (!coords.has_value())
	{
		return false;
	}
	const glm::vec2 from {map_coords::ToMetres(coords->x), map_coords::ToMetres(coords->z)};
	const auto coast = ecs::creature_exploration_land::FindPlace(creature, creature_exploration::Kind::Coast, from, false);
	if (!coast.has_value())
	{
		return false;
	}
	SendCreature(context, creature, PlaceOnLand(context, *coast), k_DrinkFromTheSeaAction);
	return true;
}

/// The creature goes home, to sleep there or only to be there
bool SendHome(RuleContext& context, computer_mind::Mind& mind, uint16_t node, uint32_t action)
{
	const auto creature = ObjectOf(mind, node);
	if (!IsCreature(creature))
	{
		return false;
	}
	const auto home = ecs::creature_home::HomeOf(Entities(), creature).value_or(glm::vec3 {0.0f});
	SendCreature(context, creature, PlaceOnLand(context, {home.x, home.z}), action);
	return true;
}

bool Sleep(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return SendHome(context, mind, node, k_SleepAtHomeAction);
}

bool GoHome(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return SendHome(context, mind, node, k_GoHomeAction);
}

/// The god does not look for a warmer or a cooler place: it sends the creature home, with no journey speed
bool SendHomeForWarmth(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = ObjectOf(mind, node);
	if (!IsCreature(creature))
	{
		return false;
	}
	SendCreature(context, creature, std::nullopt, k_GoHomeAction);
	return true;
}

/// The action of the illness emergency always fails
bool NoCure(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return false;
}

} // namespace

void computer_world::RegisterCreatureCareRules(RuleTable& table)
{
	constexpr auto k_Care = MindDomain::CreatureCare;
	AddRule(table, k_Care, NodeKind::TopCreatureInteract, {.intensity = CreatureInteract});
	AddRule(table, k_Care, NodeKind::ActionGiveCreatureFood, {.intensity = Hungry, .perform = GiveFood});
	AddRule(table, k_Care, NodeKind::ActionCastFoodForMyCreature, {.intensity = Hungry});
	AddRule(table, k_Care, NodeKind::ActionMakeCreatureDrink, {.intensity = Thirsty, .perform = Drink});
	AddRule(table, k_Care, NodeKind::ActionRestoreCreatureHealth, {.intensity = Damaged});
	AddRule(table, k_Care, NodeKind::ActionMakeCreatureSleep, {.intensity = Tired, .perform = Sleep});
	AddRule(table, k_Care, NodeKind::ActionSendCreatureHome, {.intensity = FarFromHome, .perform = GoHome});
	AddRule(table, k_Care, NodeKind::ActionMoveCreaturetoWarmerArea, {.intensity = Cold, .perform = SendHomeForWarmth});
	AddRule(table, k_Care, NodeKind::ActionMoveCreaturetoCoollerArea, {.intensity = Hot, .perform = SendHomeForWarmth});
	AddRule(table, k_Care, NodeKind::ActionReacttoCreatureStarvingEmergency, {.intensity = Starving, .perform = FeedStarving});
	AddRule(table, k_Care, NodeKind::ActionReacttoCreatureDamagedEmergency, {.intensity = Damaged});
	AddRule(table, k_Care, NodeKind::ActionReacttoCreatureIllEmergency, {.intensity = Ill, .perform = NoCure});
	AddRule(table, k_Care, NodeKind::ObjectCastFoodForMyCreature, {.intensity = CastFoodOnCreature, .perform = CastFood});
	AddRule(table, k_Care, NodeKind::ObjectRestoreMyCreatureHealth, {.intensity = CastHeal, .perform = CastHealOnCreature});
	AddRule(table, k_Care, NodeKind::ObjectRestoreMyCreatureHealthAgain,
	        {.intensity = CastHeal, .perform = CastHealOnCreature});
}
