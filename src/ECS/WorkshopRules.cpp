/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorkshopRules.h"

#include <algorithm>

#include "3D/MapCoords.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

namespace
{
/// The game keeps these as floats: a scaffold of worth n has the scale n * 0.2 + 0.5
constexpr float k_ScalePerValue = 0.2f;
constexpr float k_ScaleOfNoValue = 0.5f;
/// The game reads the worth back from the scale with these floats
constexpr float k_ValueScaleOffset = 0.5f;
constexpr float k_ValuePerScale = 5.0f;
constexpr float k_ValueRounding = 0.5f;
} // namespace

float workshop::ScaleOfValue(uint32_t value)
{
	return (static_cast<float>(value) * k_ScalePerValue) + k_ScaleOfNoValue;
}

uint32_t workshop::ValueOfScale(float scale)
{
	const float value = ((scale - k_ValueScaleOffset) * k_ValuePerScale) + k_ValueRounding;
	return static_cast<uint32_t>(map_coords::FtoL(value));
}

int32_t workshop::MakingTurns(float timeToMake)
{
	return map_coords::FtoL(timeToMake);
}

workshop::TurnEvent workshop::NextEvent(int32_t& makingTurns, const TurnFacts& facts)
{
	if (!facts.functional)
	{
		return TurnEvent::None;
	}
	if (makingTurns == 0)
	{
		return facts.room != 0 && facts.wood >= facts.scaffoldWood ? TurnEvent::Starts : TurnEvent::None;
	}
	--makingTurns;
	return makingTurns == 0 ? TurnEvent::Finishes : TurnEvent::None;
}

uint8_t workshop::FirstFreePlace(const std::array<components::Workshop::Place, 3>& places)
{
	const auto free = std::ranges::find(places, components::Workshop::Place::Free);
	return free != places.end() ? static_cast<uint8_t>(free - places.begin()) : 0;
}

bool workshop::KeepsScaffold(uint32_t putDownTurn, uint32_t turn, uint32_t keepTurns, bool scriptBuilding)
{
	const uint32_t lain = putDownTurn == 0 ? 0 : turn - putDownTurn;
	return !(keepTurns <= lain && !scriptBuilding);
}

void workshop::AddScaffold(components::Workshop& workshop, entt::entity scaffold)
{
	if (scaffold != entt::null)
	{
		workshop.scaffolds.insert(workshop.scaffolds.begin(), scaffold);
	}
	++workshop.counted;
}

bool workshop::RemoveScaffold(components::Workshop& workshop, entt::entity scaffold, uint8_t place)
{
	if (std::ranges::find(workshop.scaffolds, scaffold) == workshop.scaffolds.end())
	{
		return false;
	}
	std::erase(workshop.scaffolds, scaffold);
	--workshop.counted;
	workshop.places.at(place % workshop.places.size()) = components::Workshop::Place::Free;
	return workshop.counted == 2;
}

bool workshop::StandsInYard(const Registry& registry, entt::entity scaffold)
{
	const auto* data = registry.Valid(scaffold) ? registry.TryGet<const components::Scaffold>(scaffold) : nullptr;
	if (data == nullptr || !registry.Valid(data->workshop))
	{
		return false;
	}
	const auto* owner = registry.TryGet<const components::Workshop>(data->workshop);
	return owner != nullptr && owner->places.at(data->place % owner->places.size()) == components::Workshop::Place::Holds;
}

} // namespace openblack::ecs
