/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <vector>

#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/ComputerMind.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_mind::MindDomain;
using computer_mind::NodeKind;

namespace
{

/// A player that the land has, with its entity
struct KnownPlayer
{
	PlayerNames name;
	entt::entity entity;
};

/// The players that the land has, in the order of their numbers. The neutral player is last, and only when asked.
std::vector<KnownPlayer> PlayersInOrder(bool withNeutral)
{
	std::vector<KnownPlayer> players;
	if (!Locator::playerSystem::has_value())
	{
		return players;
	}
	auto& registry = Entities();
	for (uint8_t number = 0; number < static_cast<uint8_t>(PlayerNames::_COUNT); ++number)
	{
		const auto name = static_cast<PlayerNames>(number);
		if (name == PlayerNames::NEUTRAL && !withNeutral)
		{
			continue;
		}
		const auto entity = Locator::playerSystem::value().FindPlayer(name).value_or(entt::null);
		if (registry.Valid(entity) && registry.AllOf<Player>(entity))
		{
			players.push_back({.name = name, .entity = entity});
		}
	}
	return players;
}

/// The other player keeps the god as an ally
bool KeepsAsAlly(const KnownPlayer& other, PlayerNames god)
{
	const auto* allies = Entities().TryGet<const PlayerAllies>(other.entity);
	return allies != nullptr && player_relations::IsAlly(allies->strength.at(static_cast<size_t>(god)));
}

/// The rivals of the god: the other players that do not keep the god as an ally, in the order of the players
std::vector<KnownPlayer> RivalsOf(PlayerNames god, bool withNeutral)
{
	std::vector<KnownPlayer> rivals;
	for (const auto& player : PlayersInOrder(withNeutral))
	{
		if (player.name != god && !KeepsAsAlly(player, god))
		{
			rivals.push_back(player);
		}
	}
	return rivals;
}

/// The map position of a hand that is in metres
map_coords::MapCoords HandCoords(const glm::vec3& hand)
{
	return map_coords::FromMetres({hand.x, hand.z});
}

/// The heart of the temple of the player, none when the player has no temple
std::optional<map_coords::MapCoords> HeartOf(PlayerNames player)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return std::nullopt;
	}
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	std::optional<map_coords::MapCoords> heart;
	if (temple != entt::null)
	{
		Entities().Each<const TempleHeart>([&heart, temple](entt::entity entity, const TempleHeart& each) {
			if (each.temple == temple && !heart.has_value())
			{
				heart = CoordsOfThing(entity);
			}
		});
	}
	return heart;
}

/// The place of a player: the heart of its temple, else the hand of its computer mind while the mind plays, else the
/// place of its hand that the last report gave. A player with no report is at the start of the map.
map_coords::MapCoords FixedPlace(const KnownPlayer& player)
{
	if (const auto heart = HeartOf(player.name); heart.has_value())
	{
		return *heart;
	}
	auto& registry = Entities();
	if (const auto* mind = registry.TryGet<const ComputerPlayer>(player.entity); mind != nullptr && mind->switchedOn)
	{
		return HandCoords(mind->hand.hand);
	}
	if (Locator::handSystem::has_value())
	{
		if (const auto hands = Locator::handSystem::value().GetHandsOf(player.name); hands.has_value())
		{
			const auto hand = (*hands)[static_cast<size_t>(HandSystemInterface::Side::Left)];
			const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
			if (reported != nullptr)
			{
				return HandCoords(reported->handPoint);
			}
		}
	}
	return {};
}

/// What the god knows of one side of an attack. The harm is the harm from the other side.
player_relations::AttackSide SideOf(const KnownPlayer& player, PlayerNames other, const map_coords::MapCoords& place)
{
	auto& registry = Entities();
	player_relations::AttackSide side {.place = place};
	if (const auto* power = registry.TryGet<const PlayerInfluencePower>(player.entity); power != nullptr)
	{
		side.power = power->power;
		side.averagePower = power->average;
	}
	if (const auto* alignment = registry.TryGet<const Alignment>(player.entity); alignment != nullptr)
	{
		side.alignment = alignment->value;
	}
	const auto& record = registry.Get<const Player>(player.entity);
	if (const auto index = static_cast<size_t>(other); index < record.damageFrom.size())
	{
		side.harmFromOther = record.damageFrom.at(index);
	}
	return side;
}

/// The map positions of the abodes of the towns of the player
std::vector<map_coords::MapCoords> AbodesOf(PlayerNames player)
{
	auto& registry = Entities();
	std::vector<uint32_t> towns;
	for (const auto town : TownsOf(player))
	{
		towns.push_back(registry.Get<const Town>(town).id);
	}
	std::vector<map_coords::MapCoords> abodes;
	registry.Each<const Abode>([&abodes, &towns](entt::entity entity, const Abode& abode) {
		if (std::ranges::find(towns, abode.townId) != towns.end())
		{
			if (const auto place = CoordsOfThing(entity); place.has_value())
			{
				abodes.push_back(*place);
			}
		}
	});
	return abodes;
}

/// The god always wants to look at the players that it can defeat
float AlwaysWanted(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return 1.0f;
}

/// The god never wants to give to a player or to talk to a player
float NeverWanted(RuleContext& /*context*/, computer_mind::Mind& /*mind*/, uint16_t /*node*/)
{
	return 0.0f;
}

/// The rival that the god most wants to attack, multiplied by what the god thinks of the rival, is the node's object.
/// How much the god wants to defeat it, not more than 1.
float DefeatPlayer(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto players = PlayersInOrder(true);
	const auto god = std::ranges::find(players, context.player, &KnownPlayer::name);
	if (god == players.end())
	{
		return 0.0f;
	}
	const auto& registry = Entities();
	const auto* attitudes = registry.TryGet<const ComputerAttitudes>(god->entity);
	const float nearestAbode = player_relations::NearestAbode(HandCoords(context.state.hand.hand), AbodesOf(context.player));
	const auto godPlace = FixedPlace(*god);
	const auto rivals = RivalsOf(context.player, true);
	std::vector<float> values;
	for (const auto& rival : rivals)
	{
		const auto godSide = SideOf(*god, rival.name, godPlace);
		const auto rivalSide = SideOf(rival, context.player, FixedPlace(rival));
		const float desire = player_relations::DesireToAttack(godSide, rivalSide, nearestAbode);
		const float attitude =
		    attitudes != nullptr ? attitudes->kept.at(static_cast<size_t>(rival.name)) : ComputerAttitudes::k_StartValue;
		values.push_back(attitude * desire);
	}
	const auto chosen = player_relations::ChooseRival(values);
	if (!chosen.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = rivals[*chosen].entity;
	return player_relations::DefeatIntensity(values[*chosen]);
}

/// The first rival of the god is the node's object, and the god fully wants it. The rule compares no miracles and no
/// creatures.
float FirstRival(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto rivals = RivalsOf(context.player, false);
	if (rivals.empty())
	{
		return 0.0f;
	}
	mind.nodes[node].object = rivals.front().entity;
	return 1.0f;
}

/// The first rival of the god is the node's object, but the god never wants it. The rule compares the share of
/// worshippers of the god with the same share, so the difference is always 0.
float RivalWithMoreWorshippers(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto rivals = RivalsOf(context.player, false);
	if (!rivals.empty())
	{
		mind.nodes[node].object = rivals.front().entity;
	}
	return 0.0f;
}

} // namespace

void computer_world::RegisterAttitudesRules(RuleTable& table)
{
	AddRule(table, MindDomain::Attitudes, NodeKind::TopDefeatPlayer, {.intensity = AlwaysWanted});
	AddRule(table, MindDomain::Attitudes, NodeKind::NeedGiveToPlayer, {.intensity = NeverWanted});
	AddRule(table, MindDomain::Attitudes, NodeKind::NeedTalktoPlayer, {.intensity = NeverWanted});
	AddRule(table, MindDomain::Attitudes, NodeKind::NeedDefeatPlayer, {.intensity = DefeatPlayer});
	AddRule(table, MindDomain::Attitudes, NodeKind::NeedPlayerWithMoreSpells, {.intensity = FirstRival});
	AddRule(table, MindDomain::Attitudes, NodeKind::NeedPlayerWithBetterCreature, {.intensity = FirstRival});
	AddRule(table, MindDomain::Attitudes, NodeKind::NeedPlayerWithMoreWorshippers, {.intensity = RivalWithMoreWorshippers});
}
