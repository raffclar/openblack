/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindDefence.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "Common/GUtilsDistance.h"
#include "ComputerMindCommon.h"
#include "ComputerMindMiracles.h"
#include "Creature/CreatureDesires.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/ComputerMindDefenceRules.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownAreaRules.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_mind::MindDomain;
using computer_mind::NodeKind;
using creature_desires::Desire;

namespace
{

constexpr auto k_PlayerCount = static_cast<size_t>(PlayerNames::_COUNT);
/// The players whose attacks a town keeps for the threat to it
constexpr size_t k_ThreatPlayers = computer_mind_rules::k_TownAttackPlayers;
/// The players in the game that a town looks at when it finds a close rival of the god
constexpr size_t k_RivalPlayers = 7;

/// The game turn now
uint32_t Turn()
{
	return Locator::time::has_value() ? static_cast<uint32_t>(Locator::time::value().GetTurn()) : 0;
}

/// The entity of the player, none when the land has no such player
entt::entity PlayerEntity(PlayerNames player)
{
	if (!Locator::playerSystem::has_value())
	{
		return entt::null;
	}
	const auto entity = Locator::playerSystem::value().FindPlayer(player).value_or(entt::null);
	return Entities().Valid(entity) && Entities().AllOf<Player>(entity) ? entity : entt::null;
}

/// The player plays in the game: a computer mind marks it, or a person plays it. The hands of a player do not make it a
/// human player, because a computer god has hands too.
bool InGame(PlayerNames player)
{
	const auto entity = PlayerEntity(player);
	if (entity == entt::null)
	{
		return false;
	}
	if (const auto* mind = Entities().TryGet<const ComputerPlayer>(entity); mind != nullptr && mind->marked)
	{
		return true;
	}
	return Locator::playerSystem::has_value() && Locator::playerSystem::value().IsHumanPlayer(player);
}

/// The player keeps the other player as an ally
bool KeepsAsAlly(PlayerNames player, PlayerNames other)
{
	const auto entity = PlayerEntity(player);
	const auto* allies = entity != entt::null ? Entities().TryGet<const PlayerAllies>(entity) : nullptr;
	const auto index = static_cast<size_t>(other);
	return allies != nullptr && index < allies->strength.size() && player_relations::IsAlly(allies->strength.at(index));
}

/// The first object above the node, none when no node above has one
entt::entity ObjectAbove(const computer_mind::Mind& mind, uint16_t node)
{
	for (auto at = mind.nodes[node].parent; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return mind.nodes[*at].object;
		}
	}
	return entt::null;
}

/// The node's object, or the first object above it
entt::entity ObjectOfNodeOrAbove(const computer_mind::Mind& mind, uint16_t node)
{
	return mind.nodes[node].object != entt::null ? mind.nodes[node].object : ObjectAbove(mind, node);
}

/// The thing is in the world and has the component
template <typename T>
bool Is(entt::entity thing)
{
	auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<T>(thing);
}

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

/// How free the creature is for the god. On the god's leash, the creature is not free for a time after the leash is tied.
float FreedomOf(PlayerNames player, entt::entity creature)
{
	auto& registry = Entities();
	const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	const bool leashed = leash != nullptr && leash->worn.has_value() && leash->worn->holder == player;
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	const auto body = needs != nullptr ? needs->needs : creature_physiology::Needs {};
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const bool restoring =
	    mind != nullptr && mind->planner.current.has_value() && mind->planner.current->desire == Desire::RestoreHealth;
	return computer_mind_rules::CreatureFreedom({
	    .leashed = leashed,
	    .turnsSinceTied = leashed ? Turn() - leash->lastTiedTurn : 0,
	    .fighting = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature),
	    .life = body.life,
	    .exhaustion = body.exhaustion,
	    .energy = body.energy,
	    .restoringHealth = restoring,
	});
}

/// The temple of the player, none when it has none
entt::entity TempleOf(PlayerNames player)
{
	return Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().TempleOf(player) : entt::null;
}

/// The heart of the temple, none when it has none
entt::entity HeartOf(entt::entity temple)
{
	entt::entity heart = entt::null;
	if (temple == entt::null)
	{
		return heart;
	}
	Entities().Each<const TempleHeart>([&heart, temple](entt::entity entity, const TempleHeart& each) {
		if (each.temple == temple && heart == entt::null)
		{
			heart = entity;
		}
	});
	return heart;
}

/// The player that has the most influence at the place, the players asked in the order of their numbers
PlayerNames MostInfluentialAt(const map_coords::MapCoords& place)
{
	std::vector<influence_rules::PlayerInfluenceAt> players;
	Entities().Each<const Player>([&players, &place](const Player& player) {
		// The neutral player is not asked: it is the answer when no player has influence there
		if (player.name == PlayerNames::NEUTRAL)
		{
			return;
		}
		const float influence = Locator::influenceSystem::has_value()
		                            ? Locator::influenceSystem::value().PlayerInfluence(player.name, place)
		                            : 0.0f;
		players.push_back({.player = player.name, .influence = influence});
	});
	std::ranges::sort(players, {}, &influence_rules::PlayerInfluenceAt::player);
	return influence_rules::MostInfluentialPlayer(players);
}

/// What the town believes in each player, and what each player gave it lately
computer_mind_rules::TownBeliefFacts BeliefOf(entt::entity town)
{
	computer_mind_rules::TownBeliefFacts facts;
	if (const auto* impression = Entities().TryGet<const TownImpression>(town))
	{
		facts.belief = impression->belief.belief;
		facts.recent = impression->belief.recent;
	}
	return facts;
}

/// The town has a thing that a miracle can defend: a building or a storage pit
bool HasThingToDefend(entt::entity town)
{
	return !Entities().Get<const Town>(town).abodes.empty() || ecs::construction::StoragePitOf(town).has_value();
}

/// How much the god wants to react to a creature of another player in its influence. The creature that is the largest
/// threat is the node's object.
float ReactToAggressiveCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto player = PlayerEntity(context.player);
	const auto* power = player != entt::null ? registry.TryGet<const PlayerInfluencePower>(player) : nullptr;
	const float influencePower = power != nullptr ? power->power : 0.0f;
	const auto own = CreatureOf(context.player);
	const auto temple = CoordsOfThing(TempleOf(context.player));
	// The god looks at the creatures in the order of the game's list of creatures: the newest first
	std::vector<std::pair<uint64_t, entt::entity>> creatures;
	registry.Each<const Creature, const LivingListPlace>(
	    [&creatures](entt::entity entity, const Creature& /*creature*/, const LivingListPlace& place) {
		    creatures.emplace_back(place.made, entity);
	    });
	std::ranges::sort(creatures, std::greater {});
	const auto towns = TownsOf(context.player);
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto& [made, creature] : creatures)
	{
		const auto coords = CoordsOfThing(creature);
		if (creature == own || !coords.has_value() || MostInfluentialAt(*coords) != context.player || !temple.has_value())
		{
			continue;
		}
		if (KeepsAsAlly(registry.Get<const Creature>(creature).owner, context.player))
		{
			continue;
		}
		std::vector<computer_mind_rules::TownNearCreature> near;
		near.reserve(towns.size());
		for (const auto town : towns)
		{
			const auto& area = registry.Get<const Town>(town).area;
			const auto* reach = registry.TryGet<const TownInfluence>(town);
			near.push_back({.distance = gutils::GetDistanceInMetres(*coords, ecs::town_area::Centre(area)),
			                .radius = ecs::town_area::Radius(area),
			                .influence = reach != nullptr ? reach->radius : 0.0f});
		}
		const float threat =
		    computer_mind_rules::CreatureThreat(gutils::GetDistanceInMetres(*coords, *temple), influencePower, near);
		if (threat > best)
		{
			best = threat;
			chosen = creature;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return best < 1.0f ? best : 1.0f;
}

/// The god never wants to react to the miracles of the other players
float ReactToPlayerCastingSpells(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return 0.0f;
}

/// How much the god wants to react to the things that hit its temple. The heart of the temple is the node's object.
float ReactToPlayerThrowingStuff(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto temple = TempleOf(context.player);
	const auto heart = HeartOf(temple);
	if (heart == entt::null || !Is<Temple>(temple))
	{
		return 0.0f;
	}
	const auto reaction = computer_mind_rules::HitReaction(Turn(), Entities().Get<const Temple>(temple).lastHitTurn);
	if (!reaction.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = heart;
	return *reaction;
}

/// How much the god wants to defend the town of its towns that another player attacked. The town is the node's object.
/// When the node above has no object, the attacker becomes its object.
float DefendTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto towns = TownsOf(context.player);
	std::vector<computer_mind_rules::TownAttackFacts> attacks;
	for (size_t index = 0; index < towns.size(); ++index)
	{
		const auto* aggression = registry.TryGet<const TownAggression>(towns[index]);
		for (size_t number = 0; number < k_PlayerCount; ++number)
		{
			const auto attacker = static_cast<PlayerNames>(number);
			if (!InGame(attacker) || attacker == context.player)
			{
				continue;
			}
			attacks.push_back({.town = index,
			                   .attacker = attacker,
			                   .lastTurn = aggression != nullptr ? aggression->record.lastTurns.at(number) : 0,
			                   .aggression = aggression != nullptr ? aggression->record.aggression.at(number) : 0.0f});
		}
	}
	const auto choice = computer_mind_rules::ChooseDefence(attacks, Turn());
	if (!choice.has_value())
	{
		return 0.0f;
	}
	const auto& attack = attacks[choice->index];
	mind.nodes[node].object = towns[attack.town];
	if (const auto parent = mind.nodes[node].parent; parent.has_value() && ObjectOfNodeOrAbove(mind, *parent) == entt::null)
	{
		mind.nodes[*parent].object = PlayerEntity(attack.attacker);
	}
	return choice->value;
}

/// How much the god wants to look at the protection of its towns: the town that fears an attack most and does not have
/// the shield of the god against it yet. The town is the node's object.
float CheckTownForProtection(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : TownsOf(context.player))
	{
		const auto threat = ThreatToTown(town, Turn());
		if (!threat.has_value() || !(threat->fear > best))
		{
			continue;
		}
		const auto centre = ecs::town_area::Centre(Entities().Get<const Town>(town).area);
		if (OwnCastAt(context, centre, computer_mind_rules::ShieldToCheck(*threat)))
		{
			continue;
		}
		best = threat->fear;
		chosen = town;
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return best;
}

/// How much the god wants to defend the belief of its towns: the town that is most under an attack on its belief. The
/// town is the node's object.
float CheckTownsForBeliefAttack(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	std::array<bool, k_PlayerCount> allies {};
	for (size_t number = 0; number < k_PlayerCount; ++number)
	{
		allies.at(number) = KeepsAsAlly(context.player, static_cast<PlayerNames>(number));
	}
	const auto towns = TownsOf(context.player);
	std::vector<float> attacks;
	std::vector<float> margins;
	for (const auto town : towns)
	{
		const auto belief = BeliefOf(town);
		attacks.push_back(computer_mind_rules::BeliefAttack(belief, context.player, allies));
		margins.push_back(computer_mind_rules::BeliefMargin(belief, context.player));
	}
	const auto choice = computer_mind_rules::ChooseBeliefDefence(attacks, margins);
	if (!choice.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = towns[choice->index];
	return choice->value;
}

/// The building above the node does not burn: the god wants all of it
float FindDamagedAbode(RuleContext& /*context*/, computer_mind::Mind& mind, uint16_t node)
{
	const auto abode = ObjectAbove(mind, node);
	if (!Is<Abode>(abode) || Entities().AllOf<Fire>(abode))
	{
		return 0.0f;
	}
	mind.nodes[node].object = abode;
	return 1.0f;
}

/// How much the god wants its creature to help at the thing above the node: as much as the creature is free, less the
/// further the creature is from the thing. The creature is the node's object.
float CreatureToHelp(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = CreatureOf(context.player);
	const auto thing = CoordsOfThing(ObjectAbove(mind, node));
	const auto at = CoordsOfThing(creature);
	if (creature == entt::null || !thing.has_value() || !at.has_value())
	{
		return 0.0f;
	}
	const float distance = gutils::GetDistanceInMetres(*at, *thing);
	const float reach = computer_mind_rules::CreatureHelpReach(Entities().Get<const Creature>(creature).size);
	const float nearness = gutils::GetDistanceModifier(distance, reach);
	mind.nodes[node].object = creature;
	return FreedomOf(context.player, creature) * nearness;
}

/// The god sends its creature to help defend the town above the node, as it sends its creature to help a town
bool DefendTownWithCreature(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto help = Rules().at(static_cast<size_t>(NodeKind::ObjectGetCreaturetoHelp)).perform;
	return help != nullptr && help(context, mind, node);
}

/// The god never starts an action to use its creature against a player
bool UseCreatureAsDiversion(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return false;
}

/// How much the god wants to defend the belief of the town of the node: the best shield for the town against how
/// important the defence of the town is. The icon of the shield is the node's object.
float CastSpellToDefendAgainstBelief(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto town = ObjectOfNodeOrAbove(mind, node);
	if (!Is<Town>(town) || !HasThingToDefend(town))
	{
		return 0.0f;
	}
	const auto centre = ecs::town_area::Centre(registry.Get<const Town>(town).area);
	float best = 0.0f;
	entt::entity icon = entt::null;
	for (const auto type : {MagicType::Shield, MagicType::PhysicalShield})
	{
		const float suit = computer_mind_rules::BeliefDefenceSuit(type, OwnCastAt(context, centre, type));
		if (suit == 0.0f)
		{
			continue;
		}
		entt::entity found = entt::null;
		const float value = computer_mind_rules::KindWish(SpellWish(context, type, centre, found), suit);
		if (value > best)
		{
			best = value;
			icon = found;
		}
	}
	const bool ownTown = registry.Get<const Town>(town).owner == context.player;
	float share = 0.0f;
	if (ownTown)
	{
		const auto belief = BeliefOf(town);
		std::vector<float> rivals;
		for (size_t number = 0; number < k_RivalPlayers; ++number)
		{
			const auto other = static_cast<PlayerNames>(number);
			if (other != context.player && InGame(other) && !KeepsAsAlly(context.player, other))
			{
				rivals.push_back(belief.belief.at(number));
			}
		}
		share = computer_mind_rules::CloseRivalShare(belief.belief.at(static_cast<size_t>(context.player)), rivals);
	}
	const float value = computer_mind_rules::BeliefDefenceImportance(ownTown, share) * best;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = icon;
	return value;
}

/// The god charges the shield of the node's icon and casts it over the town above, as wide as the town
bool PerformDefendAgainstBelief(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = ObjectOfNodeOrAbove(mind, node);
	const auto town = ObjectAbove(mind, node);
	if (!Is<Town>(town) || !Is<WorshipIcon>(icon) || !HasThingToDefend(town))
	{
		return false;
	}
	const auto& area = Entities().Get<const Town>(town).area;
	ReachAndChant(context, icon);
	CastAtPlace(context, ecs::town_area::Centre(area), icon, ecs::town_area::Radius(area));
	return true;
}

} // namespace

std::optional<computer_mind_rules::TownThreat> computer_world::ThreatToTown(entt::entity town, uint32_t turn)
{
	std::array<computer_mind_rules::TownAttack, k_ThreatPlayers> attacks {};
	if (const auto* aggression = Entities().TryGet<const TownAggression>(town))
	{
		const auto& record = aggression->record;
		for (size_t number = 0; number < k_ThreatPlayers; ++number)
		{
			const auto& harm = record.lastHarms.at(number);
			attacks.at(number) = {.turn = record.lastTurns.at(number),
			                      .miracleHarm = harm.burn,
			                      .thingHarm = harm.crush,
			                      .otherThingHarm = harm.hit};
		}
	}
	return computer_mind_rules::ThreatOf(attacks, turn);
}

float computer_world::ShieldForTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = ObjectAbove(mind, node);
	if (!Is<Town>(town))
	{
		return 0.0f;
	}
	const auto threat = ThreatToTown(town, Turn());
	if (!threat.has_value())
	{
		return 0.0f;
	}
	const auto type = computer_mind_rules::ShieldAgainst(*threat);
	const auto centre = ecs::town_area::Centre(Entities().Get<const Town>(town).area);
	if (OwnCastAt(context, centre, type))
	{
		return 0.0f;
	}
	entt::entity icon = entt::null;
	const float wish = SpellWish(context, type, centre, icon) * threat->fear;
	if (icon == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = icon;
	return wish + wish;
}

void computer_world::RegisterDefenceRules(RuleTable& table)
{
	constexpr auto k_Defence = MindDomain::Defence;
	AddRule(table, k_Defence, NodeKind::NeedCheckTownsForBeliefAttack, {.intensity = CheckTownsForBeliefAttack});
	AddRule(table, k_Defence, NodeKind::NeedCheckTownForProtection, {.intensity = CheckTownForProtection});
	AddRule(table, k_Defence, NodeKind::ActionReactToAggressiveCreature, {.intensity = ReactToAggressiveCreature});
	AddRule(table, k_Defence, NodeKind::ActionReactToPlayerCastingSpells, {.intensity = ReactToPlayerCastingSpells});
	AddRule(table, k_Defence, NodeKind::ActionReactToPlayerThrowingStuff, {.intensity = ReactToPlayerThrowingStuff});
	AddRule(table, k_Defence, NodeKind::ActionDefendTown, {.intensity = DefendTown});
	AddRule(table, k_Defence, NodeKind::ActionFindDamagedAbode, {.intensity = FindDamagedAbode, .perform = CarryNear});
	AddRule(table, k_Defence, NodeKind::ObjectDefendTownWithCreature,
	        {.intensity = CreatureToHelp, .perform = DefendTownWithCreature});
	AddRule(table, k_Defence, NodeKind::ObjectDefendtownWithSpells,
	        {.intensity = ShieldForTown, .perform = CastShieldOverTown});
	AddRule(table, k_Defence, NodeKind::ObjectUseCreatureAsDiverseionAgainstPlayer,
	        {.intensity = CreatureToHelp, .perform = UseCreatureAsDiversion});
	AddRule(table, k_Defence, NodeKind::ObjectCastSpellToDefendAgainstBelief,
	        {.intensity = CastSpellToDefendAgainstBelief, .perform = PerformDefendAgainstBelief});
}
