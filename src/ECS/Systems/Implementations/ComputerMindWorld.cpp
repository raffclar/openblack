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
#include <chrono>
#include <optional>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ComputerMindCommon.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
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
	const auto start = _timeHand ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point {};
	const auto hand = _state.hand.hand;
	const bool ended = computer_hand::Process(_state.hand, _hand);
	computer_world::ReportHand(_player, hand, _state.hand.camera);
	if (_timeHand)
	{
		_handMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
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

std::optional<glm::vec3> GameMindWorld::MarkerPlace(entt::entity thing) const
{
	if (thing == entt::null || !Entities().Valid(thing) || !Entities().AllOf<ScriptMarker>(thing))
	{
		return std::nullopt;
	}
	const auto coords = CoordsOfThing(thing);
	if (!coords.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(map_coords::ToMetres(coords->x), _moves.LandHeight(*coords) + coords->altitude,
	                 map_coords::ToMetres(coords->z));
}

bool GameMindWorld::HeldByHelp() const
{
	return computer_world::HeldByHelp(_state);
}

bool computer_world::Available(entt::entity thing)
{
	auto& registry = Entities();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	// A villager on its way to dying is no longer available
	const auto* action = registry.AllOf<Villager>(thing) ? registry.TryGet<const LivingAction>(thing) : nullptr;
	return action == nullptr || !Locator::livingActionSystem::has_value() ||
	       Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Final) != VillagerStates::Dying;
}

bool computer_world::HeldByHelp(const ComputerPlayer& state)
{
	// The cinema bars of the help or of a land script hold the mind, but not while a script action or queue is due.
	// The bars are not the same on each computer of a network game (see docs/vanilla-bugs.md).
	const bool bars =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	return bars && state.mind.queue.empty() && !state.mind.scriptHand;
}

namespace
{

/// The things near the hand of the god in the game's world
class GameSeenWorld final: public computer_mind::SeenWorld
{
public:
	[[nodiscard]] std::vector<entt::entity> ThingsIn(glm::ivec2 cell) const override
	{
		return Locator::entitiesMap::value().GetAllInCell(cell);
	}

	[[nodiscard]] entt::entity SeenAs(entt::entity thing) const override
	{
		auto& registry = Entities();
		if (registry.AllOf<Tree>(thing))
		{
			const auto* member = registry.TryGet<const ForestMember>(thing);
			if (member != nullptr && Locator::forestSystem::has_value())
			{
				if (const auto forest = Locator::forestSystem::value().LandForestOf(member->forest))
				{
					return *forest;
				}
			}
			return thing;
		}
		return registry.AllOf<OneOffSpellSeed>(thing) ? thing : entt::null;
	}

	[[nodiscard]] bool Available(entt::entity thing) const override { return computer_world::Available(thing); }
};

} // namespace

void computer_world::SeeNearHand(ComputerPlayer& state, uint32_t turn)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const GameSeenWorld world;
	computer_mind::SeeNearHand(state.seen, state.hand.coords, turn, world);
}

void computer_world::Validate(ComputerPlayer& state)
{
	auto& registry = Entities();
	std::erase_if(state.casts, [&registry](entt::entity spell) { return !registry.Valid(spell); });
	// The god forgets the seen things that are not available, and the queued actions whose things are not available
	std::erase_if(state.seen, [](const computer_mind::SeenThing& old) { return !Available(old.thing); });
	std::erase_if(state.mind.queue, [](const computer_mind::QueuedAction& queued) {
		return (queued.first != entt::null && !Available(queued.first)) ||
		       (queued.second != entt::null && !Available(queued.second));
	});
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
