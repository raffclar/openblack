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

#include <algorithm>
#include <optional>
#include <vector>

#include "Common/GUtilsDistance.h"
#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Tree.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/WorldObjects.h"
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

/// The seconds that the hand stays over a thing after it casts a miracle on it
constexpr float k_CastOnThingSeconds = 8.0f;

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
	// The food need keeps the food that the town wants more than it has
	if (desire == TownDesireInfo::ForFood && Locator::townDesireSystem::has_value())
	{
		const auto food = Locator::townDesireSystem::value().FoodWantedAndHeld(towns[*best]);
		mind.nodes[node].amount = computer_mind_rules::FoodShortfall(food.wanted, food.held);
	}
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
	// The food or the wood of the thing goes to the town's store of it. A field and a fish farm give food.
	const auto resource = registry.AnyOf<Field, FishFarm>(thing) ? ResourceType::Food : ResourceType::Wood;
	const auto store = StoreOf(town, resource);
	if (store == entt::null)
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = thing}, Argument {.value = context.state.speed}});
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

/// The object of the node, or of the first node above that has one
entt::entity ObjectOfNodeOrAbove(const computer_mind::Mind& mind, uint16_t node)
{
	entt::entity found = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; found == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		found = mind.nodes[*at].object;
	}
	return found;
}

/// The object of the first node above the node that has one
entt::entity ObjectAboveNode(const computer_mind::Mind& mind, uint16_t node)
{
	const auto parent = mind.nodes[node].parent;
	return parent.has_value() ? ObjectOfNodeOrAbove(mind, *parent) : entt::null;
}

/// The place of a thing for a step of the hand, in metres, on the land
glm::vec3 PlaceForHand(const RuleContext& context, const map_coords::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), context.hand.moves.LandHeight(coords) + coords.altitude,
	        map_coords::ToMetres(coords.z)};
}

/// The fish farm of the god's towns that the god wants to take food from most
float FoodFromFishFarm(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	std::vector<entt::entity> farms;
	std::vector<float> wishes;
	for (const auto town : TownsOf(context.player))
	{
		// A town keeps its newest fish farm first
		registry.Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
			if (farm.town != town)
			{
				return;
			}
			const auto coords = CoordsOfThing(entity);
			if (!coords.has_value())
			{
				return;
			}
			farms.push_back(entity);
			wishes.push_back(computer_mind_rules::FishFarmWish(gutils::GetDistanceInMetres(context.state.hand.coords, *coords),
			                                                   PlacePull(context, *coords)));
		});
	}
	const auto best = computer_mind_rules::MostWanting(wishes);
	if (!best.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = farms[*best];
	return wishes[*best];
}

/// The map position of the hand that the god last reported: its place at the start of the last step of its hand.
/// Before the first report, the start of the map.
map_coords::MapCoords ReportedHandCoords(PlayerNames player)
{
	glm::vec3 hand {0.0f};
	const auto hands = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHandsOf(player) : std::nullopt;
	if (hands.has_value())
	{
		const auto left = hands->at(static_cast<size_t>(HandSystemInterface::Side::Left));
		const auto* reported = Entities().Valid(left) ? Entities().TryGet<const ReportedHand>(left) : nullptr;
		if (reported != nullptr)
		{
			hand = reported->handPoint;
		}
	}
	return {.x = map_coords::ToFixed(hand.x), .z = map_coords::ToFixed(hand.z), .altitude = 0.0f};
}

/// What the god knows of a thing that it saw, for the wood of a town, and the tree that the thing gives
struct SeenTree
{
	entt::entity tree {entt::null};
	computer_mind_rules::SeenWood wood;
};

/// A forest gives its centre tree, and a lone tree gives itself. None for a miracle seed, a forest with no grown trees,
/// and a tree that a script controls.
std::optional<SeenTree> TreeOfSeen(const RuleContext& context, entt::entity thing, const map_coords::MapCoords& hand)
{
	auto& registry = Entities();
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	const auto& forests = Locator::forestSystem::value();
	SeenTree seen;
	if (registry.AllOf<LandForest>(thing))
	{
		// A forest with no grown trees gives nothing, also when it has growing trees
		const auto grown = static_cast<uint32_t>(forests.TreesOf(thing, false).size());
		if (grown == 0)
		{
			return std::nullopt;
		}
		seen.tree = forests.CentreTreeOf(thing).value_or(entt::null);
		seen.wood = {.forest = true, .grownTrees = grown};
	}
	else if (registry.AllOf<Tree>(thing))
	{
		seen.tree = thing;
		const auto kind = static_cast<size_t>(registry.Get<const Tree>(thing).type);
		seen.wood = {.forest = false, .kindWood = Locator::infoConstants::value().tree.at(kind).woodValue};
	}
	const auto coords = CoordsOfThing(seen.tree);
	if (!coords.has_value() || registry.AllOf<ScriptControlled>(seen.tree))
	{
		return std::nullopt;
	}
	if (seen.wood.forest)
	{
		seen.wood.treeWood = forests.TreeWoodOf(seen.tree);
		seen.wood.pull = PlacePull(context, *coords);
	}
	// The distance goes from the hand to the edge of the tree
	seen.wood.distanceToEdge = gutils::GetDistanceInMetres(*coords, hand) - ecs::world_objects::SizeOf(seen.tree).radius;
	return seen;
}

/// The tree that the god takes for a town that needs wood, from the forests and the lone trees that it saw. The thing
/// that it saw last comes first, and the first of equal wishes wins. The node's object is the tree.
float WoodFromForest(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	if (!Locator::forestSystem::has_value() || !Locator::influenceSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const float power = Locator::influenceSystem::value().InfluencePower(context.player);
	const auto hand = ReportedHandCoords(context.player);
	std::vector<entt::entity> trees;
	std::vector<float> wishes;
	for (const auto& seen : context.state.seen)
	{
		if (const auto tree = TreeOfSeen(context, seen.thing, hand))
		{
			trees.push_back(tree->tree);
			wishes.push_back(computer_mind_rules::SeenWoodWish(tree->wood, power));
		}
	}
	const auto best = computer_mind_rules::MostWanting(wishes);
	if (!best.has_value())
	{
		return 0.0f;
	}
	const auto chosen = trees[*best];
	mind.nodes[node].object = chosen;
	// The value is how near the tree is to the place of the hand in the action, not the wish that chose it
	return computer_mind_rules::ChosenTreeWish(
	    gutils::GetDistanceInMetres(context.state.hand.coords, CoordsOfThing(chosen).value()), power);
}

/// The pile of a storage pit that the hand takes food or wood from: the food pile, or the last wood pile that holds wood.
/// None when the pile is gone or empty.
entt::entity PileToTakeFrom(entt::entity pit, ResourceType resource)
{
	auto& registry = Entities();
	const auto* store = registry.TryGet<const StoragePit>(pit);
	if (store == nullptr)
	{
		return entt::null;
	}
	const auto holds = [&registry](entt::entity pile) {
		const auto* pot = registry.Valid(pile) ? registry.TryGet<const Pot>(pile) : nullptr;
		return pot != nullptr && pot->amount != 0;
	};
	if (resource == ResourceType::Food)
	{
		return holds(store->foodPile) ? store->foodPile : entt::null;
	}
	const auto found = std::find_if(store->woodPiles.rbegin(), store->woodPiles.rend(), holds);
	return found != store->woodPiles.rend() ? *found : entt::null;
}

/// Another town of the god that gives food or wood to the town above. The node's object is the pile of the storage pit
/// of the town that gives, and the node's amount is what the god asks for.
float FromOtherTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node, ResourceType resource)
{
	const auto above = ObjectAboveNode(mind, node);
	const auto parent = mind.nodes[node].parent;
	if (above == entt::null || !parent.has_value() || !Locator::resourceStoreSystem::has_value() ||
	    !Locator::townDesireSystem::has_value())
	{
		return 0.0f;
	}
	const float need = mind.nodes[*parent].value;
	const auto& stores = Locator::resourceStoreSystem::value();
	const auto& desires = Locator::townDesireSystem::value();
	const auto desire = resource == ResourceType::Food ? TownDesireInfo::ForFood : TownDesireInfo::ForWood;
	std::vector<entt::entity> pits;
	std::vector<computer_mind_rules::GivingTown> towns;
	for (const auto town : TownsOf(context.player))
	{
		const auto pit = town != above ? WorkingPitOf(town) : std::nullopt;
		if (!pit.has_value())
		{
			continue;
		}
		const auto coords = CoordsOfThing(*pit);
		pits.push_back(*pit);
		towns.push_back({.stock = stores.GetResource(*pit, resource),
		                 .rawDesire = desires.GetRawDesire(town, desire),
		                 .pull = coords.has_value() ? PlacePull(context, *coords) : 0.0f});
	}
	const auto choice = computer_mind_rules::ChooseGivingTown(towns, need);
	if (!choice.has_value())
	{
		return 0.0f;
	}
	const auto pile = PileToTakeFrom(pits[choice->index], resource);
	if (pile == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = pile;
	mind.nodes[node].amount = map_coords::FtoL(computer_mind_rules::OtherTownAsk(need));
	return choice->value;
}

/// The hand takes food from the pile of another town and drops it in the store of the town above
bool FoodForStoragePit(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto pile = ObjectOfNodeOrAbove(mind, node);
	const auto town = ObjectAboveNode(mind, node);
	if (pile == entt::null || !registry.Valid(pile) || town == entt::null || !registry.Valid(town) ||
	    !registry.AllOf<Town>(town) || !Locator::resourceStoreSystem::has_value())
	{
		return false;
	}
	const auto store = StoreOf(town, Locator::resourceStoreSystem::value().ResourceOf(pile).type);
	const auto coords = CoordsOfThing(pile);
	if (store == entt::null || !coords.has_value())
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::MoveHandToPos, {Argument {.place = PlaceForHand(context, *coords)}});
	computer_hand::Add(context.state.hand, StepKind::CreateFoodFromContainer,
	                   {Argument {.object = pile}, Argument {.number = mind.nodes[node].amount}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = store}});
	return true;
}

/// The hand takes food from the pile of another town and gives it to the thing above, as much as the node above asks
bool FoodForObject(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto pile = ObjectOfNodeOrAbove(mind, node);
	const auto target = ObjectAboveNode(mind, node);
	const auto parent = mind.nodes[node].parent;
	// A town is not a thing that the hand can give food to
	if (pile == entt::null || !registry.Valid(pile) || target == entt::null || !registry.Valid(target) ||
	    registry.AllOf<Town>(target) || !parent.has_value())
	{
		return false;
	}
	const auto coords = CoordsOfThing(pile);
	if (!coords.has_value())
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::MoveHandToPos, {Argument {.place = PlaceForHand(context, *coords)}});
	computer_hand::Add(context.state.hand, StepKind::CreateFoodFromContainer,
	                   {Argument {.object = pile}, Argument {.number = mind.nodes[*parent].amount}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = target}});
	return true;
}

/// The hand takes all the wood of the store of another town and uses it on the thing above. Above a town need, the
/// thing is the town, which takes no pot, so the hand keeps the wood.
bool WoodFromOtherTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto pile = ObjectOfNodeOrAbove(mind, node);
	const auto target = ObjectAboveNode(mind, node);
	if (pile == entt::null || !registry.Valid(pile) || target == entt::null || !registry.Valid(target) ||
	    !Locator::resourceStoreSystem::has_value())
	{
		return false;
	}
	const auto wood = Locator::resourceStoreSystem::value().GetResource(pile, ResourceType::Wood);
	if (wood == 0)
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::CreateWoodFromContainer,
	                   {Argument {.object = pile}, Argument {.number = static_cast<int32_t>(wood)}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = target}});
	return true;
}

/// The god charges the miracle of the node's icon and casts it on the thing above. The hand stays over the thing for
/// eight seconds, then the miracle ends.
bool CastOnThing(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto icon = ObjectOfNodeOrAbove(mind, node);
	const auto target = ObjectAboveNode(mind, node);
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<WorshipIcon>(icon) || target == entt::null ||
	    !registry.Valid(target) || registry.AllOf<Town>(target))
	{
		return false;
	}
	const auto type = PlainType(registry.Get<const WorshipIcon>(icon).seed);
	ReachIcon(context, icon, type);
	computer_hand::Add(context.state.hand, StepKind::MakeWorshippersChantForSpell,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	// Food and wood miracles go on the thing
	if (type == MagicType::Food || type == MagicType::Wood)
	{
		computer_hand::Add(
		    context.state.hand, StepKind::CastSpellAtObject,
		    {Argument {.object = target}, Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	}
	computer_hand::Add(context.state.hand, StepKind::Wait, {Argument {.value = k_CastOnThingSeconds}});
	computer_hand::Add(context.state.hand, StepKind::CloseDownCurrentSpell, {});
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
	AddRule(table, k_Towns, NodeKind::ObjectGetFoodFromFishFarm, {.intensity = FoodFromFishFarm});
	AddRule(table, k_Towns, NodeKind::ObjectGetWoodFromForest, {.intensity = WoodFromForest});
	const auto otherFood = [](RuleContext& context, computer_mind::Mind& mind, uint16_t node) {
		return FromOtherTown(context, mind, node, ResourceType::Food);
	};
	AddRule(table, k_Towns, NodeKind::ObjectGetFoodFromOtherTownForStoragePit,
	        {.intensity = otherFood, .perform = FoodForStoragePit});
	AddRule(table, k_Towns, NodeKind::ObjectGetFoodFromOtherTownForObject, {.intensity = otherFood, .perform = FoodForObject});
	AddRule(table, k_Towns, NodeKind::ObjectGetWoodFromOtherTown,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return FromOtherTown(context, mind, node, ResourceType::Wood); },
	         .perform = WoodFromOtherTown});
	AddRule(table, k_Towns, NodeKind::ObjectCastFoodSpellOn,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return ShallCast(context, mind, node, MagicType::Food, ResourceType::Food); },
	         .perform = CastOnThing});
	AddRule(table, k_Towns, NodeKind::ObjectCastWoodSpellOn,
	        {.intensity = [](RuleContext& context, computer_mind::Mind& mind,
	                         uint16_t node) { return ShallCast(context, mind, node, MagicType::Wood, ResourceType::Wood); },
	         .perform = CastOnThing});
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
