/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VortexSystem.h"

#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/VortexArchetype.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/VortexRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::Vortex;

namespace
{
/// The map's cells run 0 to 511 along each side
bool OnMap(glm::ivec2 cell)
{
	return cell.x >= 0 && cell.y >= 0 && cell.x < LandIslandInterface::k_MapCellsPerSide &&
	       cell.y < LandIslandInterface::k_MapCellsPerSide;
}

/// Seconds since a vortex's state began, now
double SecondsInState(const Vortex& vortex)
{
	const auto& time = Locator::time::value();
	const auto turn = time.GetTurn();
	const auto turns = turn >= vortex.stateStartTurn ? turn - vortex.stateStartTurn : 0;
	return vortex::ElapsedSeconds(turns, time.GetTurnFraction(),
	                              static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
}
} // namespace

entt::entity VortexSystem::Create(glm::vec3 position, VortexType type, float altitude)
{
	const auto& tables = Locator::infoConstants::value().vortex;
	const auto row = static_cast<size_t>(type);
	if (row >= tables.size())
	{
		return entt::null;
	}
	const auto ground = Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	const glm::vec3 centre {position.x, ground + altitude, position.z};
	return archetypes::VortexArchetype::Create(centre, type, tables.at(row).initialState, Locator::time::value().GetTurn());
}

bool VortexSystem::StartFadeOut(entt::entity vortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return false;
	}
	auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return false;
	}
	component->state = VortexStateType::FadeOut;
	component->stateStartTurn = Locator::time::value().GetTurn();
	return true;
}

float VortexSystem::GetOpenness(entt::entity vortex) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return 0.0f;
	}
	const auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return 0.0f;
	}
	return vortex::Openness(component->state, static_cast<float>(SecondsInState(*component)));
}

void VortexSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> gone;
	bool levelled = false;
	registry.Each<Vortex>([&gone, &levelled](entt::entity entity, Vortex& vortex) {
		const auto seconds = SecondsInState(vortex);
		switch (vortex::Advance(vortex.state, seconds))
		{
		case vortex::Step::Open:
			vortex.state = VortexStateType::Active;
			break;
		case vortex::Step::Remove:
			vortex.state = VortexStateType::Inactive;
			gone.push_back(entity);
			return;
		case vortex::Step::Stay:
			break;
		}
		if (!vortex::LevelsGround(vortex.type))
		{
			return;
		}
		// The levelling only ever goes further
		const auto amount = vortex::LevelAmount(vortex.state, static_cast<float>(seconds));
		if (vortex.levelApplied < amount)
		{
			vortex.levelApplied = amount;
			LevelGround(vortex, amount);
			levelled = true;
		}
	});
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
	if (levelled)
	{
		Locator::terrainSystem::value().CommitAltitudeChanges();
	}
}

void VortexSystem::LevelGround(Vortex& vortex, float amount)
{
	auto& island = Locator::terrainSystem::value();
	const glm::vec2 centre {vortex.centre.x, vortex.centre.z};
	const auto centreCell = vortex::CentreCell(centre);
	if (vortex.groundHeights.empty())
	{
		// The square's heights as they are now, cells off the land counting as 0
		vortex.groundHeights.resize(vortex::k_LevelCellCount);
		for (size_t i = 0; i < vortex.groundHeights.size(); ++i)
		{
			const auto cell = vortex::SquareCell(centreCell, i);
			const auto* landCell = OnMap(cell) ? island.FindCell(glm::u16vec2(cell)) : nullptr;
			vortex.groundHeights[i] = landCell != nullptr ? landCell->altitude : 0;
		}
		vortex.groundAverage = vortex::AverageAltitude(vortex.groundHeights);
	}
	const auto heights = vortex::LevelSquare(centre, vortex.groundHeights, vortex.groundAverage, amount);
	for (size_t i = 0; i < heights.size(); ++i)
	{
		const auto cell = vortex::SquareCell(centreCell, i);
		if (OnMap(cell))
		{
			island.SetCellAltitude(glm::u16vec2(cell), heights.at(i));
		}
	}
}
