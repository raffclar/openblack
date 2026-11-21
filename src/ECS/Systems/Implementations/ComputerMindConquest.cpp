/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "ComputerMindConquest.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ComputerMindCommon.h"
#include "ComputerMindMiracles.h"
#include "ComputerMindRuleTable.h"
#include "ComputerMindTowns.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindTables.h"
#include "ECS/AllyInfluence.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindConquestRules.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/GamePlayers.h"
#include "ECS/Map.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "ObjectMeasures.h"

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

/// The leash setting with which the god leads its creature to impress a town, and to help a town
constexpr int32_t k_ImpressLeash = 2;
constexpr int32_t k_HelpLeash = 3;
/// The god makes its creature want to impress for this many seconds
constexpr float k_ImpressDesireSeconds = 120.0f;
/// The land balance of the things that impress
constexpr size_t k_ImpressivenessBalance = 1;

template <typename T>
bool Is(entt::entity thing)
{
	auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<T>(thing);
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

/// The entity of a player, none when the land has no such player
entt::entity PlayerEntity(PlayerNames player)
{
	if (!Locator::playerSystem::has_value())
	{
		return entt::null;
	}
	const auto entity = Locator::playerSystem::value().FindPlayer(player).value_or(entt::null);
	return Is<Player>(entity) ? entity : entt::null;
}

/// Whether a person or a computer mind plays the player
bool PlaysInGame(PlayerNames player)
{
	return Locator::playerSystem::has_value() && Locator::playerSystem::value().PlaysInGame(player);
}

/// The player keeps the other player as an ally
bool KeepsAsAlly(entt::entity player, PlayerNames other)
{
	const auto* allies = player != entt::null ? Entities().TryGet<const PlayerAllies>(player) : nullptr;
	return allies != nullptr && player_relations::IsAlly(allies->strength.at(static_cast<size_t>(other)));
}

float PowerOf(entt::entity player)
{
	const auto* power = player != entt::null ? Entities().TryGet<const PlayerInfluencePower>(player) : nullptr;
	return power != nullptr ? power->power : 0.0f;
}

uint32_t PeopleOf(entt::entity town)
{
	const auto* stats = Entities().TryGet<const TownStats>(town);
	return stats != nullptr ? stats->adults + stats->children : 0;
}

/// The belief of the town in each player of the game, by number. A land has 8 players, the neutral player last.
std::vector<float> BeliefOf(entt::entity town)
{
	std::vector<float> belief(ecs::game_players::Count(), 0.0f);
	if (const auto* impression = Entities().TryGet<const TownImpression>(town); impression != nullptr)
	{
		for (size_t player = 0; player < belief.size() && player < impression->belief.belief.size(); ++player)
		{
			belief.at(player) = impression->belief.belief.at(player);
		}
	}
	return belief;
}

/// The town of a thing: a town itself, or the town of a building, a villager or a field. None for another thing.
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

/// The place of a thing for a step of the hand, in metres, on the land
glm::vec3 PlaceForHand(const RuleContext& context, const map_coords::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), context.hand.moves.LandHeight(coords) + coords.altitude,
	        map_coords::ToMetres(coords.z)};
}

// --- Choosing a town ---

/// The god puts the miracle icons in lists by town, and the towns in lists by player, if it has not done so since the
/// world last changed. The icons and the towns do not change while the god works out its needs.
void KnowTowns(RuleContext& context)
{
	const auto& registry = std::as_const(Entities());
	if (!context.townIcons.has_value())
	{
		auto& byTown = context.townIcons.emplace();
		registry.Each<const TownCentreIcon>(
		    [&byTown](entt::entity entity, const TownCentreIcon& icon) { byTown[icon.town].push_back(entity); });
	}
	if (!context.towns.has_value())
	{
		std::unordered_map<PlayerNames, std::vector<std::pair<uint32_t, entt::entity>>> gained;
		registry.Each<const Town>(
		    [&gained](entt::entity entity, const Town& town) { gained[town.owner].emplace_back(town.gained, entity); });
		auto& byOwner = context.towns.emplace();
		for (auto& [owner, towns] : gained)
		{
			byOwner.emplace(owner, InGainedOrder(std::move(towns)));
		}
	}
}

/// The towns of the player, as TownsOf gives them. Call KnowTowns before this.
std::span<const entt::entity> KnownTownsIn(const RuleContext& context, PlayerNames player)
{
	const auto found = context.towns->find(player);
	return found != context.towns->end() ? std::span<const entt::entity>(found->second) : std::span<const entt::entity> {};
}

/// The miracles of the town as the god sees them. Call KnowTowns before this. This only reads the world.
std::vector<computer_mind_rules::TownMiracleFacts> TownMiraclesFor(const RuleContext& context, entt::entity town)
{
	const auto& registry = std::as_const(Entities());
	std::vector<computer_mind_rules::TownMiracleFacts> facts;
	const auto found = context.townIcons->find(town);
	if (found == context.townIcons->end())
	{
		return facts;
	}
	const auto god = PlayerEntity(context.player);
	const auto* player = god != entt::null ? registry.TryGet<const Player>(god) : nullptr;
	const auto& requested = context.state.mind.castRequested;
	for (const auto entity : found->second)
	{
		const auto type = PlainType(registry.Get<const TownCentreIcon>(entity).seed);
		const auto index = static_cast<size_t>(type);
		facts.push_back({
		    .enabled = player != nullptr && player_miracles::IsMagicTypeEnabled(player->miracles, type),
		    .requested = index < requested.size() && requested.at(index) != 0,
		});
	}
	return facts;
}

/// How much the god wants to win the town of the owner. Call KnowTowns before this. This only reads the world.
float TownWorthFor(const RuleContext& context, entt::entity town, PlayerNames owner, entt::entity ownerEntity, float godPeople)
{
	const float wish = computer_mind_rules::TownMiracleWish(TownMiraclesFor(context, town));
	const float share = computer_mind_rules::TownPeopleShare(PeopleOf(town), godPeople, wish);
	const float cut = computer_mind_rules::RivalPowerCut(owner == PlayerNames::NEUTRAL, PowerOf(ownerEntity),
	                                                     PowerOf(PlayerEntity(context.player)));
	const auto coords = CoordsOfThing(town);
	const float distance = coords.has_value() ? gutils::GetDistanceInMetres(context.state.hand.coords, *coords) : 0.0f;
	const float near = computer_mind_rules::TownNearness(distance);
	const auto belief = BeliefOf(town);
	const auto god = static_cast<size_t>(context.player);
	const auto townOwner = static_cast<size_t>(std::as_const(Entities()).Get<const Town>(town).owner);
	const float rival = computer_mind_rules::RivalBelief(belief, god, townOwner);
	const float hold = computer_mind_rules::TownHold(rival, townOwner == god);
	return computer_mind_rules::TownWorth(share, wish, hold, cut, near);
}

/// The town of the owner that the god wants most, and how much it wants it, not more than 1. Call KnowTowns before
/// this. This only reads the world.
std::pair<float, entt::entity> WantedTownIn(const RuleContext& context, PlayerNames owner, entt::entity ownerEntity)
{
	uint32_t people = 0;
	for (const auto town : KnownTownsIn(context, context.player))
	{
		people += PeopleOf(town);
	}
	const float godPeople = computer_mind_rules::PeopleCount(people);
	const auto towns = KnownTownsIn(context, owner);
	std::vector<float> worths;
	worths.reserve(towns.size());
	for (const auto town : towns)
	{
		worths.push_back(TownWorthFor(context, town, owner, ownerEntity, godPeople));
	}
	const auto best = computer_mind_rules::MostWantedTown(worths);
	if (!best.has_value())
	{
		return {0.0f, entt::null};
	}
	return {computer_mind_rules::CappedWorth(worths[*best]), towns[*best]};
}

/// The town of the owner that the god found before with the same owner entity, hand and miracle requests, or none
const WantedTown* KnownWantedTown(const RuleContext& context, PlayerNames owner, entt::entity ownerEntity)
{
	const auto& requested = context.state.mind.castRequested;
	const auto& hand = context.state.hand.coords;
	const auto found =
	    std::ranges::find_if(context.wantedTowns, [owner, ownerEntity, &hand, &requested](const WantedTown& wanted) {
		    return wanted.owner == owner && wanted.ownerEntity == ownerEntity && wanted.hand == hand &&
		           wanted.requested == requested;
	    });
	return found != context.wantedTowns.end() ? &*found : nullptr;
}

/// The god keeps the town of the owner that it wants most, with the facts that the worth of the town read
void KeepWantedTown(RuleContext& context, PlayerNames owner, entt::entity ownerEntity, std::pair<float, entt::entity> wanted)
{
	context.wantedTowns.push_back({.owner = owner,
	                               .ownerEntity = ownerEntity,
	                               .hand = context.state.hand.coords,
	                               .requested = context.state.mind.castRequested,
	                               .worth = wanted.first,
	                               .town = wanted.second});
}

/// The town of the owner that the god wants most, and how much it wants it, not more than 1. The god asks for the same
/// owner more than one time while it works out its needs, so it keeps each answer.
std::pair<float, entt::entity> MostWantedTownOf(RuleContext& context, PlayerNames owner, entt::entity ownerEntity)
{
	if (const auto* known = KnownWantedTown(context, owner, ownerEntity))
	{
		return {known->worth, known->town};
	}
	KnowTowns(context);
	const auto wanted = WantedTownIn(context, owner, ownerEntity);
	KeepWantedTown(context, owner, ownerEntity, wanted);
	return wanted;
}

/// The least count of owners whose towns the threads share
constexpr size_t k_OwnersOnCaller = 4;

/// The town of all the players and the neutral player that the god most wants to win is the node's object. The god
/// does not look at its own towns or at the towns of a player that keeps it as an ally.
float ExpandInfluence(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	// The owners whose towns the god looks at, in the order of the players
	std::vector<std::pair<PlayerNames, entt::entity>> owners;
	for (const auto name : ecs::game_players::All())
	{
		if (name == context.player)
		{
			continue;
		}
		// The neutral player always takes part. Its towns can be won when it has no player of its own.
		const auto entity = PlayerEntity(name);
		if ((entity == entt::null && name != PlayerNames::NEUTRAL) || KeepsAsAlly(entity, context.player))
		{
			continue;
		}
		owners.emplace_back(name, entity);
	}
	// The threads find the towns of the owners that the god did not look at before. The town of each owner is the same
	// as when the god asks for that owner alone.
	KnowTowns(context);
	std::vector<std::pair<float, entt::entity>> wanted(owners.size(), {0.0f, entt::null});
	std::vector<size_t> unknown;
	for (size_t index = 0; index < owners.size(); ++index)
	{
		if (const auto* known = KnownWantedTown(context, owners[index].first, owners[index].second))
		{
			wanted[index] = {known->worth, known->town};
		}
		else
		{
			unknown.push_back(index);
		}
	}
	ForEachOnThreads(unknown.size(), k_OwnersOnCaller, [&context, &owners, &wanted, &unknown](size_t job) {
		const auto index = unknown[job];
		wanted[index] = WantedTownIn(context, owners[index].first, owners[index].second);
	});
	for (const auto index : unknown)
	{
		KeepWantedTown(context, owners[index].first, owners[index].second, wanted[index]);
	}

	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto& [worth, town] : wanted)
	{
		if (town != entt::null && worth > best)
		{
			best = worth;
			chosen = town;
		}
	}
	if (chosen != entt::null)
	{
		mind.nodes[node].object = chosen;
	}
	return best;
}

// --- Meeting the needs of a town ---

/// How much the god wants to meet the needs of the town: fully for a town of another player, by the strength of the
/// best rival for a town of its own
float FeedFactorOf(const RuleContext& context, entt::entity town)
{
	if (Entities().Get<const Town>(town).owner != context.player)
	{
		return computer_mind_rules::FeedFactor(false, 0.0f);
	}
	const auto belief = BeliefOf(town);
	const auto god = PlayerEntity(context.player);
	float strongest = 0.0f;
	for (const auto name : ecs::game_players::Gods())
	{
		const auto number = static_cast<size_t>(name);
		// Only the players that play are rivals
		if (name == context.player || PlayerEntity(name) == entt::null || !PlaysInGame(name) || KeepsAsAlly(god, name))
		{
			continue;
		}
		if (belief.at(number) > strongest)
		{
			strongest = belief.at(number);
		}
	}
	const auto ratio = computer_mind_rules::RivalBeliefRatio(strongest, belief.at(static_cast<size_t>(context.player)));
	return computer_mind_rules::FeedFactor(true, ratio);
}

/// The town above the node is the node's object. How much the god wants to win it by meeting its need.
float TakeOverByNeed(RuleContext& context, computer_mind::Mind& mind, uint16_t node, TownDesireInfo desire)
{
	const auto town = ObjectAboveNode(mind, node);
	if (!Is<Town>(town) || !Locator::townDesireSystem::has_value())
	{
		return 0.0f;
	}
	const float raw = Locator::townDesireSystem::value().GetRawDesire(town, desire);
	const float factor = FeedFactorOf(context, town);
	mind.nodes[node].object = town;
	return computer_mind_rules::NeedWish(raw, factor);
}

float TakeOverByFood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return TakeOverByNeed(context, mind, node, TownDesireInfo::ForFood);
}

float TakeOverByWood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return TakeOverByNeed(context, mind, node, TownDesireInfo::ForWood);
}

/// The icon of the food or the wood miracle that the god most wants to cast on the store of the town above the node
/// is the node's object. The town's food need weighs the wood miracle, and its wood need weighs the food miracle.
float CastToFulfilNeeds(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = ObjectAboveNode(mind, node);
	if (!Is<Town>(town) || !Locator::townDesireSystem::has_value())
	{
		return 0.0f;
	}
	const auto store = StoreOf(town, ResourceType::Food);
	const auto place = CoordsOfThing(store);
	const auto& desires = Locator::townDesireSystem::value();
	const float food = desires.GetRawDesire(town, TownDesireInfo::ForFood);
	const float wood = desires.GetRawDesire(town, TownDesireInfo::ForWood);
	entt::entity woodIcon = entt::null;
	const float woodWish = SpellWish(context, MagicType::Wood, place, woodIcon) * food;
	entt::entity foodIcon = entt::null;
	const float foodWish = SpellWish(context, MagicType::Food, place, foodIcon) * wood;
	const auto choice = computer_mind_rules::ChooseNeedsMiracle(foodWish, woodWish);
	if (choice == computer_mind_rules::NeedsMiracle::None)
	{
		return 0.0f;
	}
	const bool isFood = choice == computer_mind_rules::NeedsMiracle::Food;
	const auto icon = isFood ? foodIcon : woodIcon;
	if (icon == entt::null || store == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = icon;
	return isFood ? foodWish : woodWish;
}

// --- The god's creature ---

/// The creature of the god, none when it has none or a script controls it
entt::entity CreatureOf(PlayerNames player)
{
	if (!Locator::playerSystem::has_value())
	{
		return entt::null;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	if (!creature.has_value() || !Is<Creature>(*creature) || Entities().AllOf<ScriptControlled>(*creature))
	{
		return entt::null;
	}
	return *creature;
}

/// What the leash of the creature is tied to. None when it wears no leash or the leash is not tied. With a holder, only
/// the leash that the holder holds counts.
entt::entity TiedTo(entt::entity creature, std::optional<PlayerNames> holder)
{
	const auto* leash = Entities().TryGet<const CreatureLeash>(creature);
	if (leash == nullptr || !leash->worn.has_value() || (holder.has_value() && leash->worn->holder != *holder))
	{
		return entt::null;
	}
	return leash->worn->tiedTo.value_or(entt::null);
}

creature_physiology::Needs NeedsOf(entt::entity creature)
{
	const auto* needs = Entities().TryGet<const CreatureNeeds>(creature);
	return needs != nullptr ? needs->needs : creature_physiology::Needs {};
}

/// How free the creature is for the god. On the god's leash, the creature is not free for a time after the leash is tied.
float FreedomOf(PlayerNames player, entt::entity creature)
{
	const auto* leash = Entities().TryGet<const CreatureLeash>(creature);
	const bool leashed = leash != nullptr && leash->worn.has_value() && leash->worn->holder == player;
	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const auto needs = NeedsOf(creature);
	const auto* mind = Entities().TryGet<const CreatureMindState>(creature);
	const bool restoring =
	    mind != nullptr && mind->planner.current.has_value() && mind->planner.current->desire == Desire::RestoreHealth;
	return computer_mind_rules::CreatureFreedom({
	    .leashed = leashed,
	    .turnsSinceTied = leashed ? turn - leash->lastTiedTurn : 0,
	    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
	    .life = needs.life,
	    .exhaustion = needs.exhaustion,
	    .energy = needs.energy,
	    .restoringHealth = restoring,
	});
}

/// How much the god wants its creature to go to the target. Not when the leash ties the creature to the target, or to a
/// thing of the target's town.
float CreatureHelpFor(const RuleContext& context, entt::entity creature, entt::entity target)
{
	const auto tied = TiedTo(creature, std::nullopt);
	if (tied != entt::null)
	{
		if (tied == target)
		{
			return 0.0f;
		}
		const auto tiedTown = TownOfThing(tied);
		if (tiedTown != entt::null && tiedTown == TownOfThing(target))
		{
			return 0.0f;
		}
	}
	const auto from = CoordsOfThing(creature);
	const auto to = CoordsOfThing(target);
	const float distance = from.has_value() && to.has_value() ? gutils::GetDistanceInMetres(*from, *to) : 0.0f;
	return computer_mind_rules::CreatureHelpWish(FreedomOf(context.player, creature), NeedsOf(creature).exhaustion, distance,
	                                             Entities().Get<const Creature>(creature).size);
}

/// Where the creature goes in a town: the storage pit that works, else the newest building
entt::entity CreatureTargetIn(entt::entity town)
{
	if (const auto pit = WorkingPitOf(town))
	{
		return *pit;
	}
	const auto& abodes = Entities().Get<const Town>(town).abodes;
	return abodes.empty() ? entt::null : abodes.front();
}

/// The hand puts the leash on the creature and ties it to the storage pit or the newest building of the town above
/// the node. The creature then wants to impress.
bool CreatureImpressesTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = TownOfThing(ObjectAboveNode(mind, node));
	const auto creature = ObjectOfNodeOrAbove(mind, node);
	if (!Is<Town>(town) || !Is<Creature>(creature))
	{
		return false;
	}
	const auto target = CreatureTargetIn(town);
	if (target == entt::null)
	{
		return false;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::SetLeash, {Argument {.object = creature}, Argument {.number = k_ImpressLeash}});
	computer_hand::Add(hand, StepKind::AttachLeashToObject, {Argument {.object = creature}, Argument {.object = target}});
	computer_hand::Add(hand, StepKind::ForceCreatureDominantDesire,
	                   {Argument {.object = creature}, Argument {.number = static_cast<int32_t>(CreatureDesires::ToImpress)},
	                    Argument {.value = k_ImpressDesireSeconds}});
	return true;
}

/// The creature goes on a journey to the storage pit or the newest building of the town of the thing above the node,
/// on the leash, and the hand ties it there. When the creature does not know all the kind miracles, the god counts one
/// more wish to teach it one.
bool CreatureHelpsTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = TownOfThing(ObjectAboveNode(mind, node));
	const auto creature = ObjectOfNodeOrAbove(mind, node);
	if (!Is<Town>(town) || !Is<Creature>(creature))
	{
		return false;
	}
	const auto target = CreatureTargetIn(town);
	const auto targetCoords = CoordsOfThing(target);
	if (target == entt::null || !targetCoords.has_value())
	{
		return false;
	}
	if (KnownFightMiracleShare(creature, CreatureDesires::Compassion) < 1.0f)
	{
		++context.state.mind.untaughtSends.at(static_cast<size_t>(CreatureDesires::Compassion));
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::SetCreatureSpeedForJourney,
	                   {Argument {.object = creature}, Argument {.place = PlaceForHand(context, *targetCoords)}});
	computer_hand::Add(hand, StepKind::SetLeash, {Argument {.object = creature}, Argument {.number = k_HelpLeash}});
	computer_hand::Add(hand, StepKind::AttachLeashToObject, {Argument {.object = creature}, Argument {.object = target}});
	return true;
}

// --- Impressive miracles ---

/// The hand goes to the icon, the worshippers chant, and the hand casts the miracle at the place of the node
bool CastToImpress(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = ObjectAboveNode(mind, node);
	const auto icon = ObjectOfNodeOrAbove(mind, node);
	if (!Is<Town>(town) || !Is<WorshipIcon>(icon))
	{
		return false;
	}
	const auto type = static_cast<int32_t>(PlainType(Entities().Get<const WorshipIcon>(icon).seed));
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::MoveHandToObject, {Argument {.object = icon}});
	computer_hand::Add(hand, StepKind::MakeWorshippersChantForSpell, {Argument {.number = type}, Argument {.object = icon}});
	computer_hand::Add(hand, StepKind::CastSpellAtPos,
	                   {Argument {.place = mind.nodes[node].place}, Argument {.number = type}, Argument {.object = icon}});
	return true;
}

// --- Thrown things ---

/// The rule by which the player may throw a thing of the kind
computer_mind_rules::ThrowKind ThrowKindOf(entt::entity thing)
{
	using computer_mind_rules::ThrowKind;
	const auto& registry = std::as_const(Entities());
	if (registry.AllOf<Tree>(thing))
	{
		return ThrowKind::Always;
	}
	if (registry.AllOf<DeadTree>(thing))
	{
		return ThrowKind::RockLike;
	}
	if (registry.AnyOf<OneOffSpellSeed, Ball, MobileObject>(thing))
	{
		return ThrowKind::Always;
	}
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	if (const auto* pot = registry.TryGet<const Pot>(thing); pot != nullptr)
	{
		const auto index = static_cast<size_t>(pot->type);
		const bool loose = info != nullptr && index < info->pot.size() && info->pot.at(index).potType == PotType::Pot;
		return loose ? ThrowKind::Always : ThrowKind::Pile;
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(thing); still != nullptr)
	{
		if (still->type == MobileStaticInfo::PhysicalShield)
		{
			return ThrowKind::None;
		}
		const auto index = static_cast<size_t>(still->type);
		const auto kind = info != nullptr && index < info->mobileStatic.size() ? info->mobileStatic.at(index).mobileType
		                                                                       : MobileStaticInfo::None;
		if (kind == MobileStaticInfo::Rock || still->type == MobileStaticInfo::Bonfire)
		{
			return ThrowKind::RockLike;
		}
		return ThrowKind::Always;
	}
	return ThrowKind::None;
}

/// The town's boredom with things that fly, 1 when it has seen none
float FlyingBoredomOf(entt::entity town)
{
	const auto* impression = Entities().TryGet<const TownImpression>(town);
	if (impression == nullptr)
	{
		return 1.0f;
	}
	const auto found = impression->boredom.find(Reaction::ReactToFlyingObject);
	return found != impression->boredom.end() ? found->second : 1.0f;
}

/// The thing near the hand that impresses the most is the node's object, and the best place of the town above the node
/// is the node's place. How much the god wants to throw the thing there.
float ThrowToImpress(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = ObjectAboveNode(mind, node);
	if (!Is<Town>(town))
	{
		return 0.0f;
	}
	const float boredom = FlyingBoredomOf(town);
	const auto& hand = context.state.hand.coords;
	map_coords::MapCoords place {};
	auto spot = TownImpressPlace(context, town, hand);
	float value = spot.has_value() ? spot->value : 0.0f;
	if (spot.has_value())
	{
		place = spot->place;
	}
	if (value < 1.0f)
	{
		// Short of a full value, the god looks at the places again
		spot = TownImpressPlace(context, town, hand);
		value = spot.has_value() ? spot->value : 0.0f;
		if (spot.has_value())
		{
			place = spot->place;
		}
		if (!(value > computer_mind_rules::k_LeastImpressValue))
		{
			return 0.0f;
		}
	}
	else
	{
		value = 1.0f;
	}
	const auto edge = InfluenceEdgeOf(context, place);
	if (!edge.has_value())
	{
		return 0.0f;
	}
	const float pull = PullPastEdge(context, *edge, place);
	const float wish = computer_mind_rules::ThrowWish(pull, value, boredom);
	const auto thing = BestThingNear(context, hand, computer_mind_rules::k_ThrowSearchRadius, ThrowScoreOf);
	if (thing == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = thing;
	mind.nodes[node].place = PlaceForHand(context, place);
	return wish;
}

/// The hand picks up the node's thing and throws it at the node's place, when the place is near enough to the hand
bool ThrowThing(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto thing = ObjectOfNodeOrAbove(mind, node);
	const auto town = ObjectAboveNode(mind, node);
	if (thing == entt::null || !Entities().Valid(thing) || !Is<Town>(town))
	{
		return false;
	}
	const auto& place = mind.nodes[node].place;
	const auto coords = map_coords::FromMetres({place.x, place.z});
	if (!(gutils::GetDistanceInMetres(coords, context.state.hand.coords) < computer_mind_rules::k_ThrowReach))
	{
		return false;
	}
	if (!TakeUp(context, thing))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::ThrowObjectInHandAtPos, {Argument {.place = place}});
	return true;
}

} // namespace

float computer_world::TakeOverTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto player = ObjectAboveNode(mind, node);
	if (!Is<Player>(player))
	{
		return 0.0f;
	}
	const auto [worth, town] = MostWantedTownOf(context, Entities().Get<const Player>(player).name, player);
	if (town != entt::null)
	{
		mind.nodes[node].object = town;
	}
	return worth;
}

float computer_world::CreatureToTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = ObjectAboveNode(mind, node);
	if (!Is<Town>(town))
	{
		return 0.0f;
	}
	const auto pit = WorkingPitOf(town);
	const auto creature = CreatureOf(context.player);
	if (!pit.has_value() || creature == entt::null)
	{
		return 0.0f;
	}
	// The god does not send the creature to a town that its own leash already ties it to
	const auto tied = TiedTo(creature, context.player);
	if (tied != entt::null && TownOfThing(tied) == town)
	{
		return 0.0f;
	}
	const float wish = CreatureHelpFor(context, creature, *pit);
	mind.nodes[node].object = creature;
	return wish;
}

namespace
{

/// The things of one cell of a search, as a run of the list of the things of the search
struct SearchCell
{
	uint32_t key {0};
	map_coords::MapCoords centre;
	size_t first {0};
	size_t end {0};
};

/// One thing of a cell of a search
struct SeenThing
{
	entt::entity thing {entt::null};
	/// The facts that the searches know of the thing in this cell
	ThingFacts* facts {nullptr};
	float distance {0.0f};
	/// The score of the thing, none when the calling thread must find it
	std::optional<float> score;
};

/// The least count of things of a search that the threads share. Below it the calling thread does all the scores,
/// because the time to wake the threads is more than the work.
constexpr size_t k_SearchThingsOnCaller = 48;

/// Finds the position, the distance and the score of the thing, and adds to the facts of the thing. This only reads the
/// world. Without measure, it does not measure the model of the thing.
void SeeThing(SeenThing& seen, const map_coords::MapCoords& centre, ThingScore score, bool measure)
{
	auto& facts = *seen.facts;
	if (!facts.placed)
	{
		facts.coords = CoordsOfThing(seen.thing);
		facts.placed = true;
	}
	if (facts.coords.has_value())
	{
		seen.distance = gutils::GetDistanceInMetres(centre, *facts.coords);
		seen.score = score(seen.thing, facts, measure);
	}
}

/// The search of BestThingNear, with no known result.
///
/// The calling thread lists the things of the cells in the order of the search. The threads find the scores of the
/// things. Then the calling thread keeps the best thing of each cell in the same order as one thread does.
entt::entity SearchThingNear(RuleContext& context, const map_coords::MapCoords& centre, float radius, ThingScore score)
{
	const auto& map = Locator::entitiesMap::value();
	const auto xs = computer_mind_rules::SearchCells(centre.x, radius);
	const auto zs = computer_mind_rules::SearchCells(centre.z, radius);
	std::vector<SearchCell> cells;
	std::vector<SeenThing> seen;
	for (int32_t x = xs.first; x <= xs.last; ++x)
	{
		for (int32_t z = zs.first; z <= zs.last; ++z)
		{
			if (x < 0 || z < 0)
			{
				continue;
			}
			const auto cell = map_coords::CellCentre(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			const auto id = ecs::MapInterface::CellId(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			const auto fixed = map.GetFixedInGridCell(id);
			const auto mobile = map.GetMobileInGridCell(id);
			// A cell with no things gives nothing, so the search does not ask for its influence
			if (fixed.empty() && mobile.empty())
			{
				continue;
			}
			const auto key = (static_cast<uint32_t>(x) << 16u) | static_cast<uint32_t>(z);
			if (const auto known = context.searchCells.find(key); known != context.searchCells.end() && !known->second)
			{
				continue;
			}
			// The things of a cell do not change while the god works out its needs, so the facts of a cell keep the
			// order of its things. In each cell the things that stay put come first.
			auto& facts = context.cellFacts[key];
			if (facts.size() != fixed.size() + mobile.size())
			{
				facts.assign(fixed.size() + mobile.size(), ThingFacts {});
			}
			const size_t first = seen.size();
			size_t slot = 0;
			for (const auto things : {fixed, mobile})
			{
				for (const auto thing : things)
				{
					seen.push_back({.thing = thing, .facts = &facts[slot]});
					++slot;
				}
			}
			cells.push_back({.key = key, .centre = cell, .first = first, .end = seen.size()});
		}
	}

	// The threads only read the world, and each writes only the facts of its own things. They do not measure the
	// models of the things: the calling thread does that after them.
	ForEachOnThreads(seen.size(), k_SearchThingsOnCaller,
	                 [&seen, &centre, score](size_t index) { SeeThing(seen[index], centre, score, false); });
	for (auto& each : seen)
	{
		if (each.facts->coords.has_value() && !each.score.has_value())
		{
			each.score = score(each.thing, *each.facts, true);
		}
	}

	float best = 0.0f;
	entt::entity found = entt::null;
	for (const auto& cell : cells)
	{
		// The search looks at the things of the cell as if the cell were in the influence. When it keeps none of them,
		// the cell changes nothing whatever its influence, so the search does not ask for it. A question about influence
		// and the scores of the things change nothing in the world.
		float cellBest = best;
		entt::entity cellFound = entt::null;
		for (size_t index = cell.first; index < cell.end; ++index)
		{
			const auto& each = seen[index];
			if (!each.facts->coords.has_value())
			{
				continue;
			}
			if (const auto kept = computer_mind_rules::SearchKeeps(*each.score, each.distance, radius, cellBest))
			{
				cellBest = *kept;
				cellFound = each.thing;
			}
		}
		if (cellFound == entt::null)
		{
			continue;
		}
		auto known = context.searchCells.find(cell.key);
		if (known == context.searchCells.end())
		{
			known = context.searchCells
			            .emplace(cell.key, ecs::ally_influence::InfluenceWithAllies(context.player, cell.centre) > 0.0f)
			            .first;
		}
		if (known->second)
		{
			best = cellBest;
			found = cellFound;
		}
	}
	return found;
}

} // namespace

entt::entity computer_world::BestThingNear(RuleContext& context, const map_coords::MapCoords& centre, float radius,
                                           ThingScore score)
{
	if (!Locator::entitiesMap::has_value())
	{
		return entt::null;
	}
	// The world does not change until the context forgets its searches, so the same search finds the same thing again
	const auto known = std::ranges::find_if(context.searches, [&centre, radius, score](const KnownSearch& search) {
		return search.centre == centre && search.radius == radius && search.score == score;
	});
	if (known != context.searches.end())
	{
		return known->found;
	}
	const auto found = SearchThingNear(context, centre, radius, score);
	context.searches.push_back({.centre = centre, .radius = radius, .score = score, .found = found});
	return found;
}

bool computer_world::CanPlayerThrow(entt::entity thing)
{
	ThingFacts facts;
	[[maybe_unused]] const bool known = KnowThrowable(thing, facts, true);
	return *facts.throwable;
}

bool computer_world::KnowThrowable(entt::entity thing, ThingFacts& facts, bool measure)
{
	if (facts.throwable.has_value())
	{
		return true;
	}
	const auto& registry = std::as_const(Entities());
	const auto kind = ThrowKindOf(thing);
	// Only a thing like a rock needs its radius, so the check does not measure the other things
	if (kind == computer_mind_rules::ThrowKind::RockLike && !measure)
	{
		return false;
	}
	const float radius = kind == computer_mind_rules::ThrowKind::RockLike ? object_measures::TwoDRadius(registry, thing) : 0.0f;
	const auto* pot = registry.TryGet<const Pot>(thing);
	facts.throwable = computer_mind_rules::CanBeThrownByPlayer(kind, radius, pot != nullptr ? pot->amount : 0);
	return true;
}

bool computer_world::KnowDestroysAbodes(entt::entity thing, ThingFacts& facts)
{
	if (!facts.destroysAbodes.has_value())
	{
		facts.destroysAbodes =
		    Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().PhysicallyDestroysAbodes(thing);
	}
	return *facts.destroysAbodes;
}

float computer_world::PullPastEdge(RuleContext& context, const map_coords::MapCoords& edge, const map_coords::MapCoords& target)
{
	const float angle = gutils::Get3DAngleFromXZ(target, edge);
	const float step = context.state.speed * computer_mind_rules::k_ThrowPullSteps;
	return PlacePull(context, target + gutils::GetPosFromAngle(angle, step));
}

std::optional<float> computer_world::ThrowScoreOf(entt::entity thing, ThingFacts& facts, bool measure)
{
	const auto& registry = std::as_const(Entities());
	if (!KnowThrowable(thing, facts, measure))
	{
		return std::nullopt;
	}
	if (!*facts.throwable || KnowDestroysAbodes(thing, facts))
	{
		return 0.0f;
	}
	const auto* info = ecs::world_objects::InfoOf(thing);
	const float balance = registry.Context().mapScriptGlobals.landBalance.at(k_ImpressivenessBalance);
	const float impressiveness = info != nullptr ? balance * info->impressiveValue : 0.0f;
	const float modifier =
	    Locator::artefactSystem::has_value() ? Locator::artefactSystem::value().ImpressiveModifier(thing) : 1.0f;
	return computer_mind_rules::ThrowScore(impressiveness, modifier);
}

float computer_world::KnownFightMiracleShare(entt::entity creature, CreatureDesires kind)
{
	if (!Locator::infoConstants::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return 1.0f;
	}
	const auto* tables = Locator::creatureMindSystem::value().GetTables();
	if (tables == nullptr)
	{
		return 1.0f;
	}
	const auto& info = Locator::infoConstants::value();
	const auto* mind = Entities().TryGet<const CreatureMindState>(creature);
	const auto* knowledge = mind != nullptr && mind->learnt.has_value() ? &mind->learnt->knowledge : nullptr;
	uint32_t miracles = 0;
	uint32_t unknown = 0;
	for (size_t number = 0; number < computer_mind::k_MagicTypes && number < tables->miracles.size(); ++number)
	{
		const auto type = static_cast<MagicType>(number);
		if (!tables->miracles[number].teachable || magic::GetMagicEffectInfo(info, type).perceivedPlayerDesire.at(0) != kind)
		{
			continue;
		}
		const auto seed = static_cast<size_t>(magic::GetMagicInfo(info, type).spellSeedType);
		if (seed >= info.spellSeed.size() || info.spellSeed.at(seed).unknown0x17C == 0)
		{
			continue;
		}
		++miracles;
		if (knowledge == nullptr || number >= knowledge->miraclesKnown.size() || !knowledge->miraclesKnown[number])
		{
			++unknown;
		}
	}
	return computer_mind_rules::KnownMiracleShare(miracles, unknown);
}

void computer_world::RegisterConquestRules(RuleTable& table)
{
	constexpr auto k_Conquest = MindDomain::Conquest;
	AddRule(table, k_Conquest, NodeKind::NeedExpandInfluence, {.intensity = ExpandInfluence});
	AddRule(table, k_Conquest, NodeKind::ActionTakeOverTown, {.intensity = TakeOverTown});
	AddRule(table, k_Conquest, NodeKind::ActionTakeOverTownByFulfillingFoodDesire,
	        {.intensity = TakeOverByFood, .perform = DropOnTownStore});
	AddRule(table, k_Conquest, NodeKind::ActionTakeOverTownByFulfillingWoodDesire,
	        {.intensity = TakeOverByWood, .perform = DropOnTownStore});
	AddRule(table, k_Conquest, NodeKind::ObjectUseCreatureToImpressTown,
	        {.intensity = CreatureToTown, .perform = CreatureImpressesTown});
	AddRule(table, k_Conquest, NodeKind::ObjectGetCreaturetoHelp, {.intensity = CreatureToTown, .perform = CreatureHelpsTown});
	AddRule(table, k_Conquest, NodeKind::ObjectThrowThingsToImpress, {.intensity = ThrowToImpress, .perform = ThrowThing});
	AddRule(table, k_Conquest, NodeKind::ObjectCastSpellToImpress, {.intensity = ImpressiveSpell, .perform = CastToImpress});
	AddRule(table, k_Conquest, NodeKind::ObjectCastSpellToFulfilTownNeeds,
	        {.intensity = CastToFulfilNeeds, .perform = CastIconOnTownStore});
}
