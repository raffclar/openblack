/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsGround.h"

#include <cmath>

#include <limits>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr float k_CellSize = 10.0f;
/// The highest a cell of shallow water stands; above this it is dry land
constexpr uint8_t k_HighestWater = 3;

/// The game changes a place to whole map units before it reads the land. A part that is a NaN or an infinity then
/// becomes 0, so the land at the corner of the map answers.
glm::vec2 LandPlace(glm::vec2 xz)
{
	const auto part = [](float metres) { return std::isfinite(metres) ? metres : 0.0f; };
	return {part(xz.x), part(xz.y)};
}

/// The game's FPU change of a number to the nearest whole number, halves to even. A NaN, an infinity or a result out
/// of the 32-bit range gives the lowest 32-bit integer.
int32_t NearestInteger(float value)
{
	const double nearest = std::nearbyint(static_cast<double>(value));
	// NOLINTNEXTLINE(readability-simplify-boolean-expr): The negated test also takes a NaN value.
	if (!(nearest >= static_cast<double>(std::numeric_limits<int32_t>::min()) &&
	      nearest <= static_cast<double>(std::numeric_limits<int32_t>::max())))
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(nearest);
}
} // namespace

float PhysicsGround::HeightAt(glm::vec2 xz) const
{
	return _land.GetHeightAt(LandPlace(xz));
}

glm::vec3 PhysicsGround::NormalAt(glm::vec2 xz) const
{
	return _land.GetNormalAt(LandPlace(xz));
}

const lnd::LNDCell* PhysicsGround::CellAt(glm::ivec2 cell) const
{
	if (cell.x < 0 || cell.y < 0 || cell.x >= LandIslandInterface::k_MapCellsPerSide ||
	    cell.y >= LandIslandInterface::k_MapCellsPerSide)
	{
		return nullptr;
	}
	return _land.FindCell(glm::u16vec2(cell));
}

std::optional<uint8_t> PhysicsGround::Altitude(glm::ivec2 cell) const
{
	const auto* found = CellAt(cell);
	if (found == nullptr)
	{
		return std::nullopt;
	}
	return found->altitude;
}

bool PhysicsGround::IsLand(glm::vec2 xz) const
{
	const auto* cell = CellAt(glm::ivec2(glm::floor(xz / k_CellSize)));
	return cell != nullptr && cell->properties.hasWater == 0;
}

std::optional<uint8_t> PhysicsGround::CellAltitudeDown(glm::vec2 xz) const
{
	return Altitude(glm::ivec2(glm::floor(xz / k_CellSize)));
}

glm::ivec2 PhysicsGround::NearestCell(glm::vec2 xz) const
{
	// Scaled to cells by a tenth and rounded to the nearest, halves to even, as the game's rounding does
	constexpr float k_CellsPerMetre = 0.1f;
	return {NearestInteger(xz.x * k_CellsPerMetre), NearestInteger(xz.y * k_CellsPerMetre)};
}

std::optional<uint8_t> PhysicsGround::CellAltitudeNearest(glm::vec2 xz) const
{
	return Altitude(NearestCell(xz));
}

std::optional<float> PhysicsGround::LakeDepth(glm::ivec2 cell) const
{
	const auto* lakes = _land.GetLakeMap();
	if (lakes == nullptr)
	{
		return std::nullopt;
	}
	const auto lake = lakes->LakeOfCorner(cell.x, cell.y);
	const auto* found = lake.has_value() ? CellAt(cell) : nullptr;
	if (found == nullptr)
	{
		return std::nullopt;
	}
	return lakes->GetLakes()[*lake].level - LandIslandInterface::GetDrawnAltitude(_land.HeightUnitsOf(*found, cell.x, cell.y));
}

float PhysicsGround::WaterLevelAt(glm::vec2 xz) const
{
	return _land.GetWaterLevelAt(LandPlace(xz));
}

bool PhysicsGround::IsSeaCell(glm::vec2 xz) const
{
	const auto altitude = CellAltitudeNearest(xz);
	if (!altitude.has_value() || *altitude == 0)
	{
		return true;
	}
	// The water of a lake above the sea of a generated land counts as the sea does
	const auto depth = LakeDepth(NearestCell(xz));
	return depth.has_value() && *depth > 0.0f;
}

bool PhysicsGround::IsDryLand(glm::vec2 xz) const
{
	const auto altitude = CellAltitudeDown(xz);
	if (!altitude.has_value() || *altitude <= k_HighestWater)
	{
		return false;
	}
	const auto depth = LakeDepth(glm::ivec2(glm::floor(xz / k_CellSize)));
	return !depth.has_value() || *depth <= 0.0f;
}

bool PhysicsGround::IsDeepWater(glm::vec2 xz) const
{
	const auto altitude = CellAltitudeNearest(xz);
	if (!altitude.has_value() || *altitude < k_HighestWater)
	{
		return true;
	}
	// Deep in a lake: one altitude unit or more under its level, as the sea is deep below altitude 3
	const auto depth = LakeDepth(NearestCell(xz));
	return depth.has_value() && *depth >= LandIslandInterface::k_HeightUnit;
}
