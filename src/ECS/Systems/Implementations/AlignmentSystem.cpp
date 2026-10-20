/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AlignmentSystem.h"

#include <algorithm>
#include <array>
#include <span>

#include "3D/CreatureBody.h"
#include "Creature/CreatureDeedAlignment.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/AreaEffect.h"
#include "MagicLiving.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The game moves the sky by a hundredth of a tenth of each millisecond of game time, from 0,
/// good, to 2, evil: a whole of -1 to 1 a second
constexpr float k_SkyTurnPerMillisecond = 0.001f;

/// The number of the kinds of thing in the alignment table
constexpr uint32_t k_AlignmentKinds = 14;

/// What the creature does on this turn, as the plan that it does tells
creature_deed_alignment::Deed DeedOf(const ecs::Registry& registry, entt::entity creature, const InfoConstants& info)
{
	creature_deed_alignment::Deed deed;
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value())
	{
		return deed;
	}
	const auto& plan = *mind->planner.current;
	const auto desire = static_cast<size_t>(plan.desire);
	if (desire < info.creatureInitialDesire.size())
	{
		deed.desireChange = info.creatureInitialDesire.at(desire).alignmentChange;
	}
	// A plan to eat a villager uses the weight of the kind of the villager in the first row of the alignment table
	if (plan.desire != creature_desires::Desire::Hunger || !plan.object.has_value())
	{
		return deed;
	}
	const auto food = static_cast<entt::entity>(*plan.object);
	if (!registry.Valid(food) || !registry.AllOf<Villager>(food))
	{
		return deed;
	}
	const auto* row = magic_living::InfoOf(food);
	if (row != nullptr && static_cast<uint32_t>(row->alignmentType) < k_AlignmentKinds && !info.alignment.empty())
	{
		deed.eatenVillagerWeight = magic::AlignmentWeight(info.alignment.front(), row->alignmentType);
	}
	return deed;
}
} // namespace

std::optional<entt::entity> AlignmentSystem::FindPlayer(PlayerNames player)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const Player>([player, &found](const entt::entity entity, const Player& component) {
		if (component.name == player)
		{
			found = entity;
		}
	});
	return found;
}

float AlignmentSystem::GetPlayerAlignment(PlayerNames player) const
{
	const auto entity = FindPlayer(player);
	if (!entity.has_value())
	{
		return 0.0f;
	}
	const auto* alignment = Locator::entitiesRegistry::value().TryGet<const Alignment>(*entity);
	return alignment != nullptr ? alignment->value : 0.0f;
}

void AlignmentSystem::SetPlayerAlignment(PlayerNames player, float alignment)
{
	if (const auto entity = FindPlayer(player); entity.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* component = registry.TryGet<Alignment>(*entity);
		if (component == nullptr)
		{
			component = &registry.Assign<Alignment>(*entity);
		}
		component->value = std::clamp(alignment, -1.0f, 1.0f);
	}
}

void AlignmentSystem::AddPendingAlignment(PlayerNames player, float change)
{
	if (const auto entity = FindPlayer(player); entity.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* component = registry.TryGet<Alignment>(*entity);
		if (component == nullptr)
		{
			component = &registry.Assign<Alignment>(*entity);
		}
		component->pending += change;
	}
}

float AlignmentSystem::GetPendingAlignment(PlayerNames player) const
{
	const auto entity = FindPlayer(player);
	if (!entity.has_value())
	{
		return 0.0f;
	}
	const auto* alignment = Locator::entitiesRegistry::value().TryGet<const Alignment>(*entity);
	return alignment != nullptr ? alignment->pending : 0.0f;
}

void AlignmentSystem::AddPlayerAlignment(PlayerNames player, float change)
{
	SetPlayerAlignment(player, alignment::AddedAlignment(GetPlayerAlignment(player), change));
}

void AlignmentSystem::UpdateTurn(const glm::vec3& eye)
{
	// What the players' and the creatures' deeds changed comes through: each turn the change waiting, held to a whole one,
	// moves the alignment by that share of the owner's change a turn, and the rest is gone
	if (Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value();
		const float change = info.player.maxAlignmentChangePerGameTurn;
		// The advisors of the player at this machine watch which way its alignment is drifting, by the change waiting
		// before it is held to a whole one
		if (Locator::guidanceSystem::has_value() && Locator::playerSystem::has_value())
		{
			const auto here = Locator::playerSystem::value().GetLocalPlayer();
			Locator::guidanceSystem::value().AlignmentTurn(GetPendingAlignment(here) * change);
		}
		auto& registry = Locator::entitiesRegistry::value();
		registry.Each<Alignment>([change](entt::entity, Alignment& alignment) {
			alignment.value = magic::StepPendingAlignment(alignment.value, alignment.pending, change);
		});
		registry.Each<Creature>([&info, &registry](entt::entity entity, Creature& creature) {
			const auto row = creature::InfoRow(creature.species);
			if (row < info.creature.size())
			{
				// The plan that the creature does adds its change before the turn moves the alignment
				const auto& species = info.creature.at(row);
				creature.pendingAlignment += creature_deed_alignment::TurnChange(
				    DeedOf(registry, entity, info), species.alignmentChangeScale, creature.alignment);
				creature.alignment = magic::StepPendingAlignment(creature.alignment, creature.pendingAlignment,
				                                                 info.creature.at(row).alignmentChangePerTurn);
			}
		});
	}

	// The camera takes the alignment of the most influential player at its eye
	std::array<alignment::PlayerAtPlace, static_cast<size_t>(PlayerNames::NEUTRAL)> players {};
	size_t count = 0;
	if (Locator::influenceSystem::has_value())
	{
		const auto& influence = Locator::influenceSystem::value();
		for (size_t i = 0; i < players.size(); ++i)
		{
			const auto name = static_cast<PlayerNames>(i);
			if (FindPlayer(name).has_value())
			{
				players.at(count++) = {
				    .influence = influence.PlayerInfluence(name, eye),
				    .alignment = GetPlayerAlignment(name),
				};
			}
		}
	}
	_camera = alignment::LandAlignment(alignment::MostInfluentialAlignment(std::span(players).first(count)));
}

void AlignmentSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	const float step = gameTime.count() * k_SkyTurnPerMillisecond;
	_sky = _sky < _camera ? std::min(_sky + step, _camera) : std::max(_sky - step, _camera);
}
