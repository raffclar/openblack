/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's dance with the villagers of a town: the world's side of each stage that its mind orders, and what its
// mind sees of the dance

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <bit>
#include <optional>

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureDance.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/DanceMoves.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerCreatureDance.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_mind::DanceOrder;

namespace
{
ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The creature's dance record, made the first time it needs one
CreatureDance& DanceRecordOf(entt::entity creature)
{
	auto& registry = Entities();
	if (auto* record = registry.TryGet<CreatureDance>(creature))
	{
		return *record;
	}
	return registry.Assign<CreatureDance>(creature);
}

/// The creature's dance, while it is there
entt::entity DanceOf(const CreatureDance& record)
{
	return ecs::dances::IsDance(Entities(), record.dance) ? record.dance : entt::null;
}

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

/// The dance goes, and its dancers decide what to do
void ClearDance(CreatureDance& record)
{
	const auto dance = DanceOf(record);
	if (dance != entt::null && Locator::danceSystem::has_value())
	{
		Locator::danceSystem::value().Destroy(dance);
	}
	record.dance = entt::null;
	record.lastPlaces.clear();
}

/// Calls the free grown up people of a villager's town to the dance; whether enough came
bool Summon([[maybe_unused]] entt::entity creature, CreatureDance& record, std::optional<uint32_t> villager, uint32_t fewest)
{
	auto& registry = Entities();
	const auto person = villager.has_value() ? static_cast<entt::entity>(*villager) : entt::null;
	if (person == entt::null || !registry.Valid(person) || !registry.AllOf<Villager>(person))
	{
		return false;
	}
	const auto town = registry.Get<const Villager>(person).town;
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return false;
	}
	const auto order = ecs::villager_creature_dance::SummonOrder(town);
	std::vector<uint32_t> free;
	free.reserve(order.size());
	std::ranges::transform(order, std::back_inserter(free), [](entt::entity entity) { return entt::to_integral(entity); });
	const auto* stats = registry.TryGet<const TownStats>(town);
	const auto dance = DanceOf(record);
	const auto joined = creature_dance::Summon(free, fewest, stats != nullptr ? stats->adults : 0, [dance](uint32_t id) {
		return dance != entt::null &&
		       ecs::villager_creature_dance::StartWorshippingCreature(static_cast<entt::entity>(id), dance);
	});
	if (!joined.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stops its dance: too few villagers to dance with",
		                    entt::to_integral(creature));
		return false;
	}
	// The list holds the last called first
	for (const auto id : *joined)
	{
		record.dancers.insert(record.dancers.begin(), static_cast<entt::entity>(id));
	}
	return true;
}

/// Sends each called villager to its place; whether it sent any
bool SendToPlaces(CreatureDance& record)
{
	const auto dance = DanceOf(record);
	bool sent = false;
	for (const auto villager : record.dancers)
	{
		if (Entities().Valid(villager) && ecs::villager_creature_dance::SendToPlace(villager, dance))
		{
			sent = true;
		}
	}
	return sent;
}
} // namespace

std::optional<glm::vec2> CreatureMindSystem::DanceAreaFor(entt::entity creature, glm::vec2 villager)
{
	const auto& registry = Entities();
	const auto width = creature_dance::AreaWidth(object_measures::TwoDRadius(registry, creature));
	return mind_detail::ClearAreaFor(registry, creature, villager, width, false);
}

void CreatureMindSystem::SituateDance(entt::entity creature, const creature_plan_actions::Executor& executor,
                                      glm::vec2 villager, creature_plan_actions::Situation& situation)
{
	// The creature notes the dance as it starts to plan it, whether or not it finds a place for it
	NoteImpressiveDance(creature, executor.danceType);
	situation.danceArea = DanceAreaFor(creature, villager);
	if (Locator::infoConstants::has_value() && executor.danceType < Locator::infoConstants::value().dance.size())
	{
		const auto& row = Locator::infoConstants::value().dance.at(executor.danceType);
		situation.fewestDancers = row.minimumDancers;
		situation.danceBeats = row.totalNumBeats;
	}
}

void CreatureMindSystem::NoteImpressiveDance(entt::entity creature, uint32_t type)
{
	auto& record = DanceRecordOf(creature);
	record.lastImpressiveTurn = Turn();
	record.lastImpressiveType = type;
}

bool CreatureMindSystem::MayDanceToImpress(entt::entity creature)
{
	const auto* record = Entities().TryGet<const CreatureDance>(creature);
	return creature_dance::MayDanceToImpress(record != nullptr ? record->lastImpressiveType : 0,
	                                         record != nullptr ? record->lastImpressiveTurn : 0, Turn());
}

std::optional<creature_mind::Senses::DanceView> CreatureMindSystem::DanceViewOf(entt::entity creature,
                                                                                const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::Dance)
	{
		return std::nullopt;
	}
	auto& registry = Entities();
	auto* record = registry.TryGet<CreatureDance>(creature);
	creature_mind::Senses::DanceView view;
	const auto dance = record != nullptr ? DanceOf(*record) : entt::null;
	if (dance == entt::null)
	{
		return view;
	}
	view.exists = true;
	// A dancer that stands somewhere other than in the last turn moves
	std::vector<CreatureDance::Place> places;
	for (const auto dancer : ecs::dances::Dancers(registry, dance))
	{
		const auto* at = registry.TryGet<const Transform>(dancer);
		if (at == nullptr)
		{
			continue;
		}
		const auto last = std::ranges::find(record->lastPlaces, dancer, &CreatureDance::Place::dancer);
		if (last != record->lastPlaces.end() && glm::xz(last->position) != glm::xz(at->position))
		{
			view.dancersMoving = true;
		}
		places.push_back({.dancer = dancer, .position = at->position});
	}
	record->lastPlaces = std::move(places);
	const auto* dancer = registry.TryGet<const Dancer>(creature);
	const auto& data = registry.Get<const Dance>(dance);
	if (dancer == nullptr || dancer->dance != dance || dancer->group >= data.groups.all.size() ||
	    !Locator::danceSystem::has_value())
	{
		return view;
	}
	const auto& group = data.groups.all[dancer->group];
	const auto found = std::ranges::find(group.dancers, creature);
	if (found == group.dancers.end())
	{
		return view;
	}
	const auto slot = static_cast<std::size_t>(found - group.dancers.begin());
	const auto position = glm::xz(registry.Get<const Transform>(creature).position);
	view.inGroup = true;
	view.action = group.move.action;
	view.first = group.move.first;
	view.second = std::bit_cast<float>(group.move.second);
	view.danceAngle = data.angle;
	view.groupCentre = map_coords::ToMetres(ecs::dance_moves::GroupCentre(data, dancer->group));
	view.place =
	    map_coords::ToMetres(ecs::dance_moves::SlotPosition(data, dancer->group, slot, Locator::danceSystem::value().Shapes()));
	view.placeDistance = glm::distance(position, view.place);
	if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature))
	{
		view.heading = locomotion->heading;
	}
	return view;
}

void CreatureMindSystem::CarryOutDance(entt::entity creature, creature_mind::IdleMind& idle, const DanceOrder& order)
{
	auto& registry = Entities();
	auto& record = DanceRecordOf(creature);
	switch (order.kind)
	{
	case DanceOrder::Kind::Clear:
		ClearDance(record);
		break;
	case DanceOrder::Kind::Set:
	{
		const bool known = Locator::infoConstants::has_value() && order.type < Locator::infoConstants::value().dance.size();
		if (known && Locator::danceSystem::has_value())
		{
			const auto height = registry.Get<const Transform>(creature).position.y;
			record.dance = Locator::danceSystem::value().Create(
			    static_cast<DanceInfo>(order.type), glm::vec3(order.point.x, height, order.point.y), creature, 0, false);
		}
		creature_mind::DanceOrderDone(idle, known);
		break;
	}
	case DanceOrder::Kind::Summon:
		creature_mind::DanceOrderDone(idle, Summon(creature, record, order.villager, order.count));
		break;
	case DanceOrder::Kind::StopLooking:
		creature_mind::DanceOrderDone(idle, true);
		break;
	case DanceOrder::Kind::SendToPlaces:
		creature_mind::DanceOrderDone(idle, SendToPlaces(record));
		break;
	case DanceOrder::Kind::Start:
		if (const auto dance = DanceOf(record); dance != entt::null)
		{
			auto& data = registry.Get<Dance>(dance);
			data.state = Dance::State::Dancing;
			data.startTurn = Turn();
			data.clock = 0.0f;
		}
		creature_mind::DanceOrderDone(idle, true);
		break;
	case DanceOrder::Kind::TellToDance:
	{
		const auto villager = order.villager.has_value() ? static_cast<entt::entity>(*order.villager) : entt::null;
		if (villager == entt::null || !registry.Valid(villager) || !registry.AllOf<Villager>(villager))
		{
			creature_mind::DanceOrderDone(idle, false);
			break;
		}
		for (const auto dancer : record.dancers)
		{
			if (registry.Valid(dancer))
			{
				ecs::villager_creature_dance::TellToDance(dancer);
			}
		}
		creature_mind::DanceOrderDone(idle, true);
		break;
	}
	case DanceOrder::Kind::Dance:
		// The creature joins its own dance, in a group that takes a creature
		if (const auto dance = DanceOf(record); dance != entt::null && !registry.AllOf<Dancer>(creature))
		{
			static_cast<void>(
			    ecs::dances::AddDancer(registry, dance, creature, DanceGroup::k_AnySex, creature_dance::k_CreatureDanceType));
		}
		break;
	case DanceOrder::Kind::Stop:
		if (const auto dance = DanceOf(record); dance != entt::null)
		{
			registry.Get<Dance>(dance).state = Dance::State::Stopped;
		}
		break;
	case DanceOrder::Kind::WaitUntilStill:
		break;
	}
}

void CreatureMindSystem::KeepDanceWithAgenda(entt::entity creature, const creature_mind::IdleMind& idle)
{
	auto& registry = Entities();
	auto* record = registry.TryGet<CreatureDance>(creature);
	const auto dance = record != nullptr ? DanceOf(*record) : entt::null;
	if (dance == entt::null || registry.Get<const Dance>(dance).state != Dance::State::Dancing)
	{
		return;
	}
	// The dance goes on while the agenda tells the people to dance, or while the creature dances
	if (idle.step < idle.agenda.size() && idle.agenda[idle.step].kind == creature_mind::Step::Kind::Dance)
	{
		const auto stage = idle.agenda[idle.step].dance.kind;
		if (stage == DanceOrder::Kind::TellToDance || stage == DanceOrder::Kind::Dance)
		{
			return;
		}
	}
	registry.Get<Dance>(dance).state = Dance::State::Stopped;
}
