/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindTowns.h"

#include <optional>
#include <vector>

#include "Common/GUtilsDistance.h"
#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
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

namespace
{

/// How much the town wants a thing: what its people want, and the two boosts
float DesireOf(entt::entity town, TownDesireInfo desire)
{
	const auto* desires = Entities().TryGet<const TownDesire>(town);
	if (desires == nullptr)
	{
		return 0.0f;
	}
	const auto index = static_cast<size_t>(desire);
	return desires->desire.at(index) + desires->boost.at(index) + desires->boostA.at(index);
}

/// The town that wants food or wood most is the node's object. How much it wants it.
float TownWanting(RuleContext& context, computer_mind::Mind& mind, uint16_t node, TownDesireInfo desire)
{
	// The wood need looks only at towns with a storage pit that works
	std::vector<entt::entity> towns;
	std::vector<float> desires;
	for (const auto town : TownsOf(context.player))
	{
		if (desire == TownDesireInfo::ForWood && !WorkingPitOf(town).has_value())
		{
			continue;
		}
		towns.push_back(town);
		desires.push_back(DesireOf(town, desire));
	}
	const auto best = computer_mind_rules::MostWanting(desires);
	if (!best.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = towns[*best];
	return desires[*best];
}

/// How much the god wants to cast the miracle on the store of the town above the node, or on the thing above it
float ShallCast(RuleContext& context, computer_mind::Mind& mind, uint16_t node, MagicType type, ResourceType resource)
{
	// The place is the store of the town above, or the place of the thing above
	std::optional<map_coords::MapCoords> place;
	entt::entity above = entt::null;
	for (auto at = mind.nodes[node].parent; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			above = mind.nodes[*at].object;
			break;
		}
	}
	if (above != entt::null && Entities().Valid(above) && Entities().AllOf<Town>(above))
	{
		place = CoordsOfThing(StoreOf(above, resource));
	}
	else
	{
		place = CoordsOfThing(above);
	}

	return ShallCastAt(context, mind, node, type, place);
}

/// The field of the god's towns that the god wants to take food from most
float FoodFromField(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	auto& registry = Entities();
	const auto& types = Locator::infoConstants::value().fieldType;
	std::vector<entt::entity> fields;
	std::vector<float> wishes;
	for (const auto town : TownsOf(context.player))
	{
		const auto number = static_cast<int>(registry.Get<const Town>(town).id);
		// A town keeps its newest field first
		registry.Each<const Field>([&](entt::entity entity, const Field& field) {
			if (field.town != number)
			{
				return;
			}
			const auto coords = CoordsOfThing(entity);
			if (!coords.has_value())
			{
				return;
			}
			const auto& type = types.at(static_cast<size_t>(field.type));
			fields.push_back(entity);
			wishes.push_back(computer_mind_rules::FieldWish(field.crop.food, type.totalFoodInField,
			                                                gutils::GetDistanceInMetres(context.state.hand.coords, *coords),
			                                                PlacePull(context, *coords)));
		});
	}
	const auto best = computer_mind_rules::MostWantedField(wishes);
	if (!best.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = fields[*best];
	return wishes[*best];
}

/// The hand takes the thing that the chosen leaf found and drops it on the store of the town
bool PickUpForTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	// The thing is the object of the leaf that the node chose, or of the first node above that leaf with one
	auto leaf = node;
	while (mind.nodes[leaf].bestLeaf.has_value() && *mind.nodes[leaf].bestLeaf != leaf)
	{
		leaf = *mind.nodes[leaf].bestLeaf;
	}
	entt::entity thing = mind.nodes[leaf].object;
	for (auto at = mind.nodes[leaf].parent; thing == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		thing = mind.nodes[*at].object;
	}
	entt::entity town = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; town == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		town = mind.nodes[*at].object;
	}
	if (thing == entt::null || !registry.Valid(thing) || town == entt::null || !registry.Valid(town) ||
	    !registry.AllOf<Town>(town))
	{
		return false;
	}
	// The food or the wood of the thing goes to the town's store of it
	const auto resource = registry.AllOf<Field>(thing) ? ResourceType::Food : ResourceType::Wood;
	const auto store = StoreOf(town, resource);
	if (store == entt::null)
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = thing}, Argument {.value = context.state.hand.speed}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = store}});
	return true;
}

/// The god charges the miracle of the node's icon and casts it on the store of the town above
bool CastFromIcon(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	// The icon is the node's object, or the object of the first node above that has one
	entt::entity icon = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; icon == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		icon = mind.nodes[*at].object;
	}
	entt::entity town = entt::null;
	for (auto at = mind.nodes[node].parent; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			town = mind.nodes[*at].object;
			break;
		}
	}
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<WorshipIcon>(icon))
	{
		return false;
	}
	const auto type = PlainType(registry.Get<const WorshipIcon>(icon).seed);
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return false;
	}
	const auto store = StoreOf(town, type == MagicType::Wood ? ResourceType::Wood : ResourceType::Food);
	if (store == entt::null)
	{
		return false;
	}
	ReachIcon(context, icon, type);
	computer_hand::Add(context.state.hand, StepKind::MakeWorshippersChantForSpell,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	// Food, wood, heal, water and forest miracles go on the thing
	computer_hand::Add(
	    context.state.hand, StepKind::CastSpellAtObject,
	    {Argument {.object = store}, Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	return true;
}

} // namespace

void computer_world::RegisterTownsRules(RuleTable& table)
{
	constexpr auto k_Towns = MindDomain::Towns;
	AddRule(table, k_Towns, NodeKind::ActionTownWithHighestFoodDesire,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return TownWanting(context, mind, node, TownDesireInfo::ForFood); },
	         .perform = PickUpForTown});
	AddRule(table, k_Towns, NodeKind::ActionTownWithHighestWoodDesire,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return TownWanting(context, mind, node, TownDesireInfo::ForWood); },
	         .perform = PickUpForTown});
	AddRule(table, k_Towns, NodeKind::ObjectShallCastFoodSpell,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return ShallCast(context, mind, node, MagicType::Food, ResourceType::Food); },
	         .perform = CastFromIcon});
	AddRule(table, k_Towns, NodeKind::ObjectShallCastWoodSpell,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return ShallCast(context, mind, node, MagicType::Wood, ResourceType::Wood); },
	         .perform = CastFromIcon});
	AddRule(table, k_Towns, NodeKind::ObjectGetFoodFromField, {.intensity = FoodFromField});
}

float computer_world::ShallCastAt(RuleContext& context, computer_mind::Mind& mind, uint16_t node, MagicType type,
                                  const std::optional<map_coords::MapCoords>& place)
{
	const auto* player = [&context]() -> const Player* {
		const Player* found = nullptr;
		Entities().Each<const Player>([&found, &context](const Player& each) {
			if (each.name == context.player && found == nullptr)
			{
				found = &each;
			}
		});
		return found;
	}();
	if (player == nullptr || !player_miracles::IsMagicTypeEnabled(player->miracles, type))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	float charge = ChargeLevel(context, type, icon);
	// Short of a full charge the god asks again, so each look counts two requests
	charge = charge < 1.0f ? ChargeLevel(context, type, icon) : 1.0f;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	const float turns = computer_mind_rules::TurnsToCharge(FactsOf(icon));
	if (turns < 0.0f)
	{
		return 0.0f;
	}
	const float pull = place.has_value() ? PlacePull(context, *place) : 1.0f;
	mind.nodes[node].object = icon;
	return computer_mind_rules::SpellValue(charge, turns, pull);
}
