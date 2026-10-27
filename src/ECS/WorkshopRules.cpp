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
#include <bit>

#include "3D/MapCoords.h"
#include "Common/X87Angle.h"
#include "Common/X87Matrix.h"
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

glm::mat4 workshop::BuildingMatrix(const glm::vec3& angles, bool canLean, float scale, const glm::vec3& position)
{
	glm::mat4 building {canLean ? x87::GameTurnMatrixScaled(angles.x, angles.y, angles.z, scale)
	                            : x87::GameUprightTurnScaled(angles.y, scale)};
	building[3] = glm::vec4(position, 1.0f);
	return building;
}

workshop::SpecialPlace workshop::SpecialPlaceOf(const glm::mat4& point, const glm::mat4& building)
{
	// The point's matrix moves through the building's matrix in the order of the 3D engine
	const auto world = x87::Product(point, building);
	return {.coords = map_coords::FromMetres(glm::vec2(world[3].x, world[3].z)),
	        .angles = x87::GameTurnMatrixAngles(glm::mat3(world))};
}

glm::vec3 workshop::SpecialPointOf(const glm::mat4& point, const glm::mat4& building)
{
	return glm::vec3(x87::Product(point, building)[3]);
}

float workshop::ScaleOfValue(uint32_t value)
{
	return (static_cast<float>(value) * k_ScalePerValue) + k_ScaleOfNoValue;
}

uint32_t workshop::ValueOfScale(float scale)
{
	const float value = ((scale - k_ValueScaleOffset) * k_ValuePerScale) + k_ValueRounding;
	return static_cast<uint32_t>(map_coords::FtoL(value));
}

float workshop::WoodWant(uint32_t wood, uint32_t scaffoldWood)
{
	// 0.0001 as the game keeps it
	constexpr float k_Small = std::bit_cast<float>(0x38D1B717u);
	const float share = (static_cast<float>(wood) + k_Small) / (static_cast<float>(scaffoldWood) + k_Small);
	return 1.0f - std::min(share, 1.0f);
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

bool workshop::IsWoodPile(const Registry& registry, entt::entity pile)
{
	if (pile == entt::null)
	{
		return false;
	}
	bool found = false;
	registry.Each<const components::Workshop>(
	    [pile, &found](const components::Workshop& data) { found = found || data.woodPile == pile; });
	return found;
}

bool workshop::CanJoin(const JoinFacts& facts)
{
	// The sum and the most are words: a row with no most lets any sum through, as in the game
	const uint32_t most = facts.football ? facts.mostForCombining : static_cast<uint32_t>(facts.mostForCombining) - 1u;
	if (facts.targetInPhysics || facts.heldValue + facts.targetValue > most)
	{
		return false;
	}
	if (facts.heldScriptBuilding || facts.targetScriptBuilding)
	{
		return false;
	}
	return !facts.targetOnSite || facts.targetSiteBuilt == 0.0f;
}

workshop::JoinedTown workshop::TownOfJoined(const JoinedTownFacts& facts)
{
	if (!facts.heldHasTown)
	{
		return JoinedTown::Target;
	}
	if (!facts.targetHasTown)
	{
		return JoinedTown::Held;
	}
	if (facts.heldValue == facts.targetValue)
	{
		return facts.holderOwnsHeldTown ? JoinedTown::Held : JoinedTown::Target;
	}
	return facts.heldValue > facts.targetValue ? JoinedTown::Held : JoinedTown::Target;
}

} // namespace openblack::ecs
