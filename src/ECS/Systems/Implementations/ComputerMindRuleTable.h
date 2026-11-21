/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The table of the rules of the mind of a computer god. Each kind of node has one owner part (DomainOf), and the file
/// of that part puts the rules of its kinds in the table. The table lists the call of each part, so that a new rule
/// changes only the file of its part.
namespace openblack::ecs::systems::computer_world
{

/// A search for the best thing near a place, and the thing that it found
struct KnownSearch
{
	map_coords::MapCoords centre;
	float radius {0.0f};
	float (*score)(entt::entity thing) {nullptr};
	entt::entity found {entt::null};
};

/// What a rule works with: the god, its mind and hand, and the hand's parts in the game's world
struct RuleContext
{
	PlayerNames player;
	components::ComputerPlayer& state;
	const computer_hand::World& hand;
	/// The cells of the thing searches that are in the influence of the god or its allies, by cell. The influence does
	/// not change while the god works out its needs, so a search asks for each cell one time. The world clears it
	/// before each step of the hand and each start of an action.
	std::unordered_map<uint32_t, bool> searchCells {};
	/// The searches for things since the world last changed. The things and their scores do not change while the god
	/// works out its needs, so the same search finds the same thing again.
	std::vector<KnownSearch> searches {};
	/// The trees of the forests that the god saw. The god makes the lists at the first question about the trees of a
	/// forest since the world last changed.
	std::optional<ForestTreeLists> forestTrees {};
	/// The miracle icons of each town centre, by town, in the order of a walk of the icons. The god makes the lists at
	/// the first question about the icons of a town since the world last changed.
	std::optional<std::unordered_map<entt::entity, std::vector<entt::entity>>> townIcons {};
	/// The towns of each player in the order that the player got them. The god makes the lists at the first question
	/// about the towns since the world last changed.
	std::optional<std::unordered_map<PlayerNames, std::vector<entt::entity>>> towns {};

	/// Forget what the searches found, because the world can change after this
	void ForgetSearches()
	{
		searchCells.clear();
		searches.clear();
		forestTrees.reset();
		townIcons.reset();
		towns.reset();
	}
};

/// How much the node is wanted. The rule can set the node's object and amount.
using IntensityRule = float (*)(RuleContext& context, computer_mind::Mind& mind, uint16_t node);
/// The node starts its action on the hand. False when it cannot.
using PerformRule = bool (*)(RuleContext& context, computer_mind::Mind& mind, uint16_t node);

/// The rules of a kind of node. A kind with no intensity rule is not wanted, and a kind with no perform rule cannot start
/// an action.
struct Rule
{
	IntensityRule intensity {nullptr};
	PerformRule perform {nullptr};
};

using RuleTable = std::array<Rule, static_cast<size_t>(computer_mind::NodeKind::_Count)>;

/// The owner part puts a rule of one of its kinds of nodes in the table. Each kind gets each rule one time.
void AddRule(RuleTable& table, computer_mind::MindDomain domain, computer_mind::NodeKind kind, Rule rule);

/// The rules of each part, each in the file ComputerMind<Part>.cpp
void RegisterCoreRules(RuleTable& table);
void RegisterTownsRules(RuleTable& table);
void RegisterWorshipRules(RuleTable& table);
void RegisterMiraclesRules(RuleTable& table);
void RegisterCreatureCareRules(RuleTable& table);
void RegisterCreaturePlayRules(RuleTable& table);
/// The lessons of the creature, a part of the play with the creature, in the file ComputerMindCreatureTeach.cpp
void RegisterCreatureTeachRules(RuleTable& table);
void RegisterConquestRules(RuleTable& table);
void RegisterWarRules(RuleTable& table);
void RegisterDefenceRules(RuleTable& table);
void RegisterAttitudesRules(RuleTable& table);

/// The table of all the rules, made one time from the calls above
[[nodiscard]] const RuleTable& Rules();

} // namespace openblack::ecs::systems::computer_world
