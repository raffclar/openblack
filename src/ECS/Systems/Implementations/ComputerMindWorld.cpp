/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindWorld.h"

#include <cassert>

#include <algorithm>
#include <optional>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ComputerMindCommon.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_mind::NodeKind;

namespace
{

/// Makes the table: each part of the mind puts its own rules in it
RuleTable MakeRules()
{
	RuleTable table {};
	RegisterCoreRules(table);
	RegisterTownsRules(table);
	RegisterWorshipRules(table);
	RegisterMiraclesRules(table);
	RegisterCreatureCareRules(table);
	RegisterCreaturePlayRules(table);
	RegisterConquestRules(table);
	RegisterWarRules(table);
	RegisterDefenceRules(table);
	RegisterAttitudesRules(table);
	return table;
}

/// The rules of the kind of the node
const Rule& RuleOf(const computer_mind::Mind& mind, uint16_t node)
{
	return Rules().at(static_cast<size_t>(mind.nodes[node].kind));
}

/// The need of the node: the node itself or the first node above it that is a need. None above the needs.
std::optional<uint16_t> NeedOf(const computer_mind::Mind& mind, uint16_t node)
{
	for (std::optional<uint16_t> at = node; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (computer_mind::InfoOf(mind.nodes[*at].kind).layer == computer_mind::Layer::Need)
		{
			return at;
		}
	}
	return std::nullopt;
}

/// Writes the need that the god chose to the log. The check tools count these lines for each god.
void LogNeed(PlayerNames player, const computer_mind::Mind& mind, uint16_t node)
{
	const auto logger = spdlog::get("game");
	const auto need = NeedOf(mind, node);
	if (logger == nullptr || !need.has_value())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(logger, "Computer god {} chooses need {}", static_cast<int>(player),
	                   computer_mind::InfoOf(mind.nodes[*need].kind).name);
}

} // namespace

void computer_world::AddRule(RuleTable& table, [[maybe_unused]] computer_mind::MindDomain domain, NodeKind kind, Rule rule)
{
	// Only the owner part of a kind of node adds its rules, and each rule one time
	assert(computer_mind::DomainOf(kind) == domain);
	auto& row = table.at(static_cast<size_t>(kind));
	assert(rule.intensity == nullptr || row.intensity == nullptr);
	assert(rule.perform == nullptr || row.perform == nullptr);
	if (rule.intensity != nullptr)
	{
		row.intensity = rule.intensity;
	}
	if (rule.perform != nullptr)
	{
		row.perform = rule.perform;
	}
}

const RuleTable& computer_world::Rules()
{
	static const RuleTable k_Rules = MakeRules();
	return k_Rules;
}

GameMindWorld::GameMindWorld(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
    , _moves(player, state)
    , _objects(player, state)
    , _casts(player, state)
    , _creature(player, state)
    , _totem(player, state)
    , _hand {.moves = _moves, .objects = _objects, .casts = _casts, .creature = _creature, .totem = _totem}
    , _context {.player = player, .state = state, .hand = _hand}
{
}

float GameMindWorld::Intensity(computer_mind::Mind& mind, uint16_t node)
{
	// A node whose rule is not made yet is not wanted
	const auto rule = RuleOf(mind, node).intensity;
	return rule != nullptr ? rule(_context, mind, node) : 0.0f;
}

bool GameMindWorld::Perform(computer_mind::Mind& mind, uint16_t node)
{
	const auto rule = RuleOf(mind, node).perform;
	const bool started = rule != nullptr && rule(_context, mind, node);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Computer god {} action {} ({:.3f}) started {}", static_cast<int>(_player),
	                    computer_mind::InfoOf(mind.nodes[node].kind).name, mind.nodes[0].value, started);
	if (started)
	{
		LogNeed(_player, mind, node);
	}
	return started;
}

bool GameMindWorld::ActionBusy() const
{
	return computer_hand::Busy(_state.hand);
}

bool GameMindWorld::StepAction()
{
	// The god reports the place of its hand before the step, and its camera after it
	const auto hand = _state.hand.hand;
	const bool ended = computer_hand::Process(_state.hand, _hand);
	computer_world::ReportHand(_player, hand, _state.hand.camera);
	return ended;
}

void GameMindWorld::ClearAction()
{
	computer_hand::Clear(_state.hand);
}

void GameMindWorld::StartAction() {}

bool GameMindWorld::ActionLocked() const
{
	return _state.hand.locked;
}

void GameMindWorld::UnlockAction()
{
	_state.hand.locked = false;
}

bool GameMindWorld::ForceQueued()
{
	return false;
}

bool GameMindWorld::HeldByHelp() const
{
	return false;
}

void computer_world::Validate(ComputerPlayer& state)
{
	auto& registry = Entities();
	std::erase_if(state.casts, [&registry](entt::entity spell) { return !registry.Valid(spell); });
	// An action whose thing has gone stops
	const bool gone = std::ranges::any_of(state.hand.steps, [&registry](const computer_hand::Step& step) {
		return std::ranges::any_of(step.arguments, [&registry](const Argument& argument) {
			return argument.object != entt::null && !registry.Valid(argument.object);
		});
	});
	if (gone)
	{
		computer_hand::Clear(state.hand);
	}
}

std::optional<glm::vec3> computer_world::StartingHand(PlayerNames player)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return std::nullopt;
	}
	const auto coords = CoordsOfThing(Locator::worshipSiteSystem::value().TempleOf(player));
	if (!coords.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(map_coords::ToMetres(coords->x), coords->altitude, map_coords::ToMetres(coords->z));
}
