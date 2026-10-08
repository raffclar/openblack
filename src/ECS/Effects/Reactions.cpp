/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Reactions.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ReactionsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MapShield.h"

using namespace openblack;
using namespace openblack::ecs::effects;
using openblack::ecs::components::ReactionRecords;

namespace
{
/// The game's reactions; stops with a message when there are none (before the game or after it has gone)
ecs::systems::ReactionsSystemInterface& ReactionList()
{
	if (!Locator::reactionsSystem::has_value())
	{
		std::fputs("ecs::effects::reactions: no reactions in the locator (Locator::reactionsSystem)\n", stderr);
		std::abort();
	}
	return Locator::reactionsSystem::value();
}

reactions::Reaction* FindMutable(uint32_t id)
{
	return ReactionList().Find(id);
}

/// The cell's unsigned high words inside the map (the island's cells per side,
/// as ecs::sea_cells; out without a land)
bool InMap(const map_coords::MapCoords& coords)
{
	return Locator::terrainSystem::has_value() &&
	       map_coords::InBounds(coords, Locator::terrainSystem::value().GetCellsPerSide());
}

glm::vec2 PosOf(entt::entity entity)
{
	const auto& position = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(entity).position;
	return {position.x, position.z};
}

/// The reaction's position = its initiator's, as MapCoords in metres (x, z and the altitude above the land in y).
/// Most initiators are objects with a Transform (a world point); a Spell has no Transform in openblack and keeps its
/// own position (components::Spell), and a spell IS the initiator of the shield reactions (REACTION 13 / 35 / 36,
/// Magic/Spells/SpellShield) and of the spell's own reaction. False: no position at all
bool MapPosOf(entt::entity entity, glm::vec3& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(entity); transform != nullptr)
	{
		out = magic::ToMap(transform->position);
		return true;
	}
	if (const auto* spell = registry.TryGet<const ecs::components::Spell>(entity); spell != nullptr)
	{
		out = spell->position;
		return true;
	}
	return false;
}

/// The shut-down of the reactions about to be removed (their ids, in list order): not available any more, then each
/// class's followers (the head follower stops reacting and sets its state while there is one), before the caller
/// erases them
void ShutDown(const std::vector<uint32_t>& ids)
{
	for (const auto id : ids)
	{
		if (auto* entry = FindMutable(id); entry != nullptr)
		{
			entry->available = false;
		}
		for (const auto handler : ReactionList().ShutDownHandlers())
		{
			if (handler != nullptr)
			{
				handler(id);
			}
		}
	}
}

/// The ids of the reactions that `remove` selects, in list order
template <typename Predicate>
std::vector<uint32_t> Matching(const Predicate& remove)
{
	std::vector<uint32_t> ids;
	for (const auto& reaction : ReactionList().List())
	{
		if (remove(reaction))
		{
			ids.push_back(reaction.id);
		}
	}
	return ids;
}

/// The ids just shut down go (the handlers may have removed or added reactions meanwhile)
void EraseIds(const std::vector<uint32_t>& ids)
{
	ReactionList().Erase(ids);
}

/// The Living class of an object of a cell's list (-1: not a Living)
int ClassOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		return static_cast<int>(reactions::LivingClass::Villager);
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		return static_cast<int>(reactions::LivingClass::Animal);
	}
	return -1; // no creature yet
}
} // namespace

void reactions::SetLivingReactionHandler(LivingClass living, LivingReactionHandler handler)
{
	ReactionList().SetReactionHandler(living, handler);
}

void reactions::SetLivingShutDownHandler(LivingClass living, LivingShutDownHandler handler)
{
	ReactionList().SetShutDownHandler(living, handler);
}

uint32_t reactions::CreateReaction(entt::entity initiator, openblack::Reaction type, PlayerNames player, bool stamp)
{
	// (type & 0xFF) == -1 never holds for the byte, but the callers check reactionType != -1 first
	if (type == openblack::Reaction::None)
	{
		return 0;
	}
	Reaction reaction {
	    .initiator = initiator,
	    .type = type,
	    .player = player,
	    .turnCreated = stamp ? game_clock::Turn() : 0,
	};
	// radius = whetherReactionGrows ? 1 : maxReactionDistance
	if (Locator::infoConstants::has_value())
	{
		const auto& table = Locator::infoConstants::value().reaction;
		if (const auto index = static_cast<size_t>(type); index < table.size())
		{
			reaction.radius = table[index].whetherReactionGrows != 0 ? 1.0f : table[index].maxReactionDistance;
		}
	}
	reaction.id = ReactionList().Add(reaction);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Reaction {}: type {} by entity {}, radius {:.1f}", reaction.id,
	                    static_cast<int>(type), static_cast<uint32_t>(initiator), reaction.radius);
	// spread from its position over the spiral size of its radius
	SpreadReaction(reaction.id);
	return reaction.id;
}

void reactions::SpreadReaction(uint32_t id)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::entitiesMap::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* found = Find(id);
	glm::vec3 initiator(0.0f);
	if (found == nullptr || !ecs::IsAvailable(found->initiator) || !MapPosOf(found->initiator, initiator))
	{
		return;
	}
	const Reaction reaction = *found;
	// outside the turn (the map script at load, the debug hooks) the cells are rebuilt first; inside it they are the
	// turn's (the game turn rebuilds them before the Living)
	if (!ReactionList().InTurn())
	{
		Locator::entitiesMap::value().Rebuild();
	}
	const glm::vec2 at(initiator.x, initiator.z); // MapPosOf: a Spell initiator has no Transform
	const int cells = map_coords::CellSpiralSize(reaction.radius);
	const auto atCoords = map_coords::FromMetres(at);
	auto coords = atCoords;
	map_coords::Spiral spiral; // dir = count = 1
	for (int i = 0; i < cells; ++i)
	{
		// the distance in metres from the reaction to the cell: the cell is kept while radius >= d
		if (InMap(coords) && gutils::GetDistanceInMetres(atCoords, coords) <= reaction.radius)
		{
			// the cell's mobile list, from its head (ecs::map_cells): every Living of it,
			// whatever its class, in that order
			const auto livings = ecs::map_cells::MobileInCell(map_coords::Cell(coords));
			for (const auto entity : livings)
			{
				// the handlers may create or remove reactions: this one is looked up again for each Living
				const auto* current = Find(reaction.id);
				if (current == nullptr || entity == reaction.initiator || !registry.Valid(entity) ||
				    !registry.AllOf<components::Transform>(entity))
				{
					continue;
				}
				const int living = ClassOf(entity);
				const auto handler = living < 0 ? nullptr : ReactionList().ReactionHandler(static_cast<LivingClass>(living));
				if (handler == nullptr)
				{
					continue;
				}
				// (after the class's own test, before the distance) a Living under a shield the
				// reaction's source is not definitely inside ignores it, villagers and animals alike
				if (magic::map_shield::IsReactionBlockedByShield(
				        magic::ToMap(registry.Get<const components::Transform>(entity).position), initiator))
				{
					continue;
				}
				const glm::vec2 p = PosOf(entity);
				const float d = (std::abs(p.y - at.y) + std::abs(p.x - at.x)) * 0.5f;
				const Reaction copy = *current;
				handler(entity, copy, d);
			}
		}
		map_coords::AddCells(coords, spiral.Next()); // one cell along the spiral: the fraction stays
	}
}

void reactions::RemoveAllReactionsInitiatedByObject(entt::entity initiator)
{
	// the shut-down of each one the object started (the shut-down handlers, then erased)
	const auto ids = Matching([initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
	ShutDown(ids);
	EraseIds(ids);
}

void reactions::SetAvailable(entt::entity initiator, bool available)
{
	for (auto& reaction : ReactionList().List())
	{
		if (reaction.initiator == initiator)
		{
			reaction.available = available;
		}
	}
}

void reactions::SetUnavailableInHand(entt::entity initiator)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& table = Locator::infoConstants::value().reaction;
	for (auto& reaction : ReactionList().List())
	{
		// the reaction info of its type: whetherReactionFinishesIfInitiatorInHand
		const auto type = static_cast<size_t>(reaction.type);
		if (reaction.initiator == initiator && type < table.size() && table[type].whetherReactionFinishesIfInitiatorInHand != 0)
		{
			reaction.available = false;
		}
	}
}

void reactions::RemoveAllReactionsOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type)
{
	// the shut-down of each one of that type the object started
	const auto ids = Matching(
	    [initiator, type](const Reaction& reaction) { return reaction.initiator == initiator && reaction.type == type; });
	ShutDown(ids);
	EraseIds(ids);
}

void reactions::Prune()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// (approximate) the original removes the reactions when the initiator is deleted; here at BeginTurn. So the
	// shut-down runs here (or where an owner calls RemoveAll*), not at the deletion: a slot holder whose log / pile
	// went mid-turn meets the reaction processing's first test (it stops reacting, no state reset) instead of the
	// shut-down's stop with a new state. An initiator on the dead list (ecs::IsAvailable) counts as gone
	const auto ids = Matching([](const Reaction& reaction) { return !ecs::IsAvailable(reaction.initiator); });
	ShutDown(ids);
	EraseIds(ids);
}

void reactions::Stamp(uint32_t reaction)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr)
	{
		entry->turnCreated = game_clock::Turn();
	}
}

void reactions::MarkStarted(uint32_t reaction, uint32_t turn)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr && entry->turnCreated == 0)
	{
		entry->turnCreated = turn;
	}
}

void reactions::SetRadius(uint32_t reaction, float radius)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr)
	{
		entry->radius = radius;
	}
}

void reactions::SetInitiator(uint32_t reaction, entt::entity initiator)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr)
	{
		entry->initiator = initiator;
	}
}

uint32_t reactions::GetReactionInitiatedBy(entt::entity initiator)
{
	const auto& list = ReactionList().List();
	const auto it = std::find_if(list.begin(), list.end(),
	                             [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
	return it == list.end() ? 0 : it->id;
}

uint32_t reactions::GetReactionOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type)
{
	const auto& list = ReactionList().List();
	const auto it = std::find_if(list.begin(), list.end(), [initiator, type](const Reaction& reaction) {
		return reaction.initiator == initiator && reaction.type == type && reaction.available;
	});
	return it == list.end() ? 0 : it->id;
}

const std::vector<reactions::Reaction>& reactions::All()
{
	return ReactionList().List();
}

const reactions::Reaction* reactions::Find(uint32_t id)
{
	return FindMutable(id);
}

bool reactions::IsAvailable(const Reaction* reaction)
{
	return reaction != nullptr && reaction->available && ecs::IsAvailable(reaction->initiator);
}

bool reactions::Records(entt::entity living, uint8_t type, uint32_t again, uint32_t now)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& memory = registry.AllOf<ReactionRecords>(living) ? registry.Get<ReactionRecords>(living)
	                                                       : registry.Assign<ReactionRecords>(living);
	for (uint8_t i = 0; i < memory.count;)
	{
		auto& record = memory.records[i];
		if (record.type == type)
		{
			if (now - record.turn > again)
			{
				record.turn = now;
				return true;
			}
			return false;
		}
		if (now - record.turn > 1800)
		{
			std::copy(memory.records.begin() + i + 1, memory.records.begin() + memory.count, memory.records.begin() + i);
			--memory.count;
			continue;
		}
		++i;
	}
	if (memory.count >= memory.records.size())
	{
		std::copy(memory.records.begin() + 1, memory.records.end(), memory.records.begin());
		--memory.count;
	}
	memory.records[memory.count++] = {type, now};
	return true;
}

uint32_t reactions::RecordTurn(entt::entity living, uint8_t type)
{
	const auto* memory = Locator::entitiesRegistry::value().TryGet<const ReactionRecords>(living);
	if (memory == nullptr)
	{
		return 0;
	}
	for (uint8_t i = 0; i < memory->count; ++i)
	{
		if (memory->records[i].type == type)
		{
			return memory->records[i].turn;
		}
	}
	return 0;
}

void reactions::RefreshRecord(entt::entity living, uint8_t type, uint32_t now)
{
	auto* memory = Locator::entitiesRegistry::value().TryGet<ReactionRecords>(living);
	if (memory == nullptr)
	{
		return;
	}
	for (uint8_t i = 0; i < memory->count; ++i)
	{
		if (memory->records[i].type == type)
		{
			memory->records[i].turn = now;
		}
	}
}

uint32_t reactions::Score(uint8_t type, bool reactsToType, uint32_t priority, float distance)
{
	const auto& reaction = Locator::infoConstants::value().reaction.at(type);
	if (!reactsToType || distance > reaction.maxReactionDistance)
	{
		return 0;
	}
	const float max = reaction.maxReactionDistance;
	// std::max(max, 0.0001f): openblack's divide-by-zero guard, not in the original
	const float score = static_cast<float>(priority) *
	                    (1.0f + 0.5f * reaction.howImportantIsDistance * (max - distance) / std::max(max, 0.0001f));
	return static_cast<uint32_t>(std::trunc(std::min(255.0f, score)));
}

void reactions::SetReactionDoneWhen(entt::entity living, uint8_t type, uint32_t now)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& memory = registry.AllOf<ReactionRecords>(living) ? registry.Get<ReactionRecords>(living)
	                                                       : registry.Assign<ReactionRecords>(living);
	// the type's record -> its turn = now
	for (uint8_t i = 0; i < memory.count; ++i)
	{
		if (memory.records[i].type == type)
		{
			memory.records[i].turn = now;
			return;
		}
	}
	// a new record {type, now}; 3 or more records -> the head (the oldest) dropped
	if (memory.count >= memory.records.size())
	{
		std::copy(memory.records.begin() + 1, memory.records.end(), memory.records.begin());
		--memory.count;
	}
	memory.records[memory.count++] = {type, now};
}

float reactions::DistanceToReactionCell(entt::entity living, const Reaction& reaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	glm::vec3 initiator(0.0f);
	if (!registry.Valid(reaction.initiator) || !MapPosOf(reaction.initiator, initiator) ||
	    !registry.AllOf<ecs::components::Transform>(living))
	{
		return 0.0f;
	}
	// the reaction's position, its cell (the high words), then the distance from the Living's MapCoords to the cell
	const auto at = map_coords::FromMetres(glm::vec2(initiator.x, initiator.z));
	const map_coords::JustMapXZ cell {map_coords::SignedCellOf(at.x), map_coords::SignedCellOf(at.z)};
	return gutils::GetDistanceInMetresToCell(map_coords::FromMetres(PosOf(living)), cell);
}

namespace
{
/// R = 10 + max; ((R - d) / R x distImp x 0.5 + 1) x turns, truncated, every step rounded to a float as in the
/// original
uint32_t StandardTurns(uint8_t type, float distance, uint32_t turns)
{
	const auto& info = Locator::infoConstants::value().reaction.at(type);
	const float r = 10.0f + info.maxReactionDistance;
	float f = (r - distance) / r;
	f = f * info.howImportantIsDistance;
	f = f * 0.5f;
	f = f + 1.0f;
	f = f * static_cast<float>(static_cast<int32_t>(turns));
	return static_cast<uint32_t>(static_cast<int32_t>(std::trunc(f)));
}
} // namespace

uint32_t reactions::StandardTurnsToReact(uint8_t type, float distance)
{
	// the info's numGameTurnsForNormalThingsToReact
	return StandardTurns(type, distance, Locator::infoConstants::value().reaction.at(type).numGameTurnsForNormalThingsToReact);
}

uint32_t reactions::StandardTurnsBeforeReactingAgain(uint8_t type, float distance)
{
	// the info's numGameTurnsForNormalThingsBeforeReactingAgain
	return StandardTurns(type, distance,
	                     Locator::infoConstants::value().reaction.at(type).numGameTurnsForNormalThingsBeforeReactingAgain);
}

bool reactions::SameTypeSwitch(uint8_t type)
{
	// the type table's flag: 10 REACT_TO_FIRE, 28, 35
	return type == 10 || type == 28 || type == 35;
}

bool reactions::MaySwitch(float currentScore, float newScore, float seconds, uint8_t currentType)
{
	constexpr uint8_t k_ReactToHandPickUp = 16;
	if (!(currentScore < newScore))
	{
		return false;
	}
	const float h = std::max(10.0f, currentScore / newScore * 20.0f - 10.0f);
	return !(seconds < h && (seconds < 1.0f || currentType != k_ReactToHandPickUp));
}

uint32_t reactions::Turn()
{
	return game_clock::Turn();
}

void reactions::BeginTurn()
{
	ReactionList().SetInTurn(true);
	Prune();
}

void reactions::EndTurn()
{
	ReactionList().SetInTurn(false);
}

void reactions::Clear()
{
	ReactionList().Clear();
}
