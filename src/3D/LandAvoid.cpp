/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandAvoid.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <numbers>
#include <string_view>
#include <vector>

#include <LNDFile.h>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/LandAvoidState.h"
#include "3D/LandIslandInterface.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "Locator.h"

namespace
{
/// Flood marks: 3 queued, 5 a 1 next to the flood
constexpr uint8_t k_Queued = 3;
constexpr uint8_t k_Walkable = 4;
constexpr uint8_t k_AvoidNextToFlood = 5;

/// The LandAvoid state (Locator::landAvoidSystem)
openblack::land_avoid::State& LandAvoidState()
{
	return openblack::Locator::landAvoidSystem::value().GetState();
}

uint8_t& Cell(int32_t x, int32_t z)
{
	auto& state = LandAvoidState();
	return state.avoid[static_cast<size_t>(z) * static_cast<size_t>(state.size) + static_cast<size_t>(x)];
}

bool InMap(int32_t x, int32_t z)
{
	auto& state = LandAvoidState();
	return x >= 0 && z >= 0 && x < state.size && z < state.size;
}

/// A 4-neighbour flood of the walkable (4) cells from the seed, which become 0; the 1 cells
/// next to them become 5. Then 1 and 4 -> 2 and 5 -> 1. Nothing at all when the seed is not a 4.
void FloodFillReachable(int32_t seedX, int32_t seedZ)
{
	auto& state = LandAvoidState();
	if (!InMap(seedX, seedZ) || Cell(seedX, seedZ) != k_Walkable)
	{
		return;
	}
	// the neighbours in the order of the stack tables: (-1, 0), (0, -1), (1, 0), (0, 1)
	constexpr std::array<std::array<int32_t, 2>, 4> k_Neighbours {{{-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
	// a 1 MB stack of (x, z) uint16 pairs; the cell taken is the first and the last one moves into its place
	std::vector<std::array<int32_t, 2>> stack;
	stack.reserve(0x100000 / 4);
	Cell(seedX, seedZ) = k_Queued;
	stack.push_back({seedX, seedZ});
	while (!stack.empty())
	{
		const auto [x, z] = stack.front();
		for (const auto& [dx, dz] : k_Neighbours)
		{
			const int32_t nx = x + dx;
			const int32_t nz = z + dz;
			if (!InMap(nx, nz))
			{
				continue;
			}
			auto& value = Cell(nx, nz);
			if (value == openblack::land_avoid::k_Avoid)
			{
				value = k_AvoidNextToFlood;
			}
			else if (value == k_Walkable)
			{
				stack.push_back({nx, nz});
				value = k_Queued;
			}
		}
		Cell(x, z) = openblack::land_avoid::k_Land;
		stack.front() = stack.back();
		stack.pop_back();
	}
	for (auto& value : state.avoid)
	{
		if (value == openblack::land_avoid::k_Avoid || value == k_Walkable)
		{
			value = openblack::land_avoid::k_Unreachable;
		}
		else if (value == k_AvoidNextToFlood)
		{
			value = openblack::land_avoid::k_Avoid;
		}
	}
}
} // namespace

namespace openblack::land_avoid
{

void Validate(const LandIslandInterface& island)
{
	auto& state = LandAvoidState();
	namespace sea = ecs::sea_cells;
	state.size = island.GetCellsPerSide();
	state.avoid.assign(static_cast<size_t>(state.size) * static_cast<size_t>(state.size), 0);

	// the altitude byte of a corner, 0 off the map or without a block
	const auto altitude = [&island](int32_t x, int32_t z) { return sea::AltitudeAt(island, {x, z}); };
	int32_t seedX = 0;
	int32_t seedZ = 0;
	for (int32_t z = 0; z < state.size; ++z)
	{
		int32_t runLength = 0;
		bool inRun = false;
		for (int32_t x = 0; x < state.size; ++x)
		{
			const std::array<uint16_t, 4> corners {altitude(x, z), altitude(x, z + 1), altitude(x + 1, z),
			                                       altitude(x + 1, z + 1)};
			float lowest = static_cast<float>(corners[0]) * 0.67f; // the altitude scale
			float highest = lowest;
			for (size_t i = 1; i < corners.size(); ++i)
			{
				const float height = static_cast<float>(corners[i]) * 0.67f;
				highest = std::max(highest, height);
				lowest = std::min(lowest, height);
			}
			const bool anyAboveSea = (corners[0] | corners[1] | corners[2] | corners[3]) != 0;
			if (highest - lowest <= 10.0f && anyAboveSea) // steeper than 10 -> 1
			{
				Cell(x, z) = k_Walkable;
				++runLength;
				inRun = true;
				continue;
			}
			// too steep, or all four corners at altitude 0 (deep sea)
			Cell(x, z) = k_Avoid;
			if (inRun)
			{
				// the seed is the last cell of the last run of walkable cells that a 1 ends (a run that
				// reaches the end of its row does not count: the run is reset at every row)
				if (runLength > 0)
				{
					seedX = x - 1;
					seedZ = z;
				}
				inRun = false;
				runLength = 0;
			}
		}
	}

	FloodFillReachable(seedX, seedZ);

	// a reached cell that is water (or off the map, or without a block) is walkable water
	for (int32_t z = 0; z < state.size; ++z)
	{
		for (int32_t x = 0; x < state.size; ++x)
		{
			if (Cell(x, z) != k_Land)
			{
				continue;
			}
			const auto* cell = sea::CellAt(island, {x, z});
			if (cell == nullptr || cell->properties.hasWater != 0)
			{
				Cell(x, z) = k_Water;
			}
		}
	}

	state.seed = {seedX, seedZ};
}

void Clear()
{
	auto& state = LandAvoidState();
	state.size = 0;
	state.avoid.clear();
}

uint8_t At(int32_t x, int32_t z)
{
	return InMap(x, z) ? Cell(x, z) : uint8_t {k_Unreachable};
}

bool IsPosValid(glm::vec3 position, float radius)
{
	const auto walkable = [](uint8_t value) { return value == k_Land || value == k_Water; };
	// the cell: x 0.1, truncated
	const auto cellX = static_cast<int32_t>(position.x * 0.1f);
	const auto cellZ = static_cast<int32_t>(position.z * 0.1f);
	if (!InMap(cellX, cellZ) || !walkable(Cell(cellX, cellZ)))
	{
		return false;
	}
	const float radiusSquared = radius * radius;
	for (int32_t z = cellZ - 1; z <= cellZ + 1; ++z)
	{
		for (int32_t x = cellX - 1; x <= cellX + 1; ++x)
		{
			if (!InMap(x, z) || walkable(Cell(x, z)))
			{
				continue;
			}
			// the cell's centre: 10 i + 5
			const float dx = static_cast<float>(x) * 10.0f + 5.0f - position.x;
			const float dz = static_cast<float>(z) * 10.0f + 5.0f - position.z;
			if (radiusSquared > dx * dx + dz * dz)
			{
				return false;
			}
		}
	}
	return true;
}

bool DumpPng(const std::filesystem::path& path)
{
	auto& state = LandAvoidState();
	if (state.size == 0)
	{
		return false;
	}
	std::vector<uint8_t> pixels(state.avoid.size() * 3);
	for (size_t i = 0; i < state.avoid.size(); ++i)
	{
		std::array<uint8_t, 3> colour;
		switch (state.avoid[i])
		{
		case k_Land:
			colour = {0, 170, 0};
			break;
		case k_Water:
			colour = {0, 90, 255};
			break;
		case k_Avoid:
			colour = {230, 0, 0};
			break;
		case k_Unreachable:
			colour = {128, 128, 128};
			break;
		default:
			colour = {255, 0, 255};
			break;
		}
		std::copy(colour.begin(), colour.end(), pixels.begin() + static_cast<std::ptrdiff_t>(i * 3));
	}
	return stbi_write_png(path.string().c_str(), state.size, state.size, 3, pixels.data(), state.size * 3) != 0;
}

std::optional<glm::vec2> NearestValid(glm::vec2 point, float radius, float maxDistance,
                                      const std::function<bool(glm::vec3, float)>& valid)
{
	constexpr int32_t k_Directions = 16;
	constexpr float k_Step = 0.5f;
	const auto at = [&valid, radius](glm::vec2 p) { return valid(glm::vec3(p.x, 0.0f, p.y), radius); };
	if (at(point))
	{
		return point;
	}
	for (int32_t step = 1; static_cast<float>(step) * k_Step <= maxDistance; ++step)
	{
		const float distance = static_cast<float>(step) * k_Step;
		for (int32_t i = 0; i < k_Directions; ++i)
		{
			const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(k_Directions);
			const glm::vec2 candidate = point + (distance * glm::vec2(std::cos(angle), std::sin(angle)));
			if (at(candidate))
			{
				return candidate;
			}
		}
	}
	return std::nullopt;
}

std::optional<glm::vec2> NearestValid(glm::vec2 point, float radius, float maxDistance)
{
	return NearestValid(point, radius, maxDistance, [](glm::vec3 p, float r) { return IsPosValid(p, r); });
}

void DumpIfRequested()
{
	auto& state = LandAvoidState();
	const char* env = std::getenv("OPENBLACK_DUMP_LAND_AVOID");
	if (env == nullptr || *env == '\0')
	{
		return;
	}
	const std::string_view value(env);
	const std::filesystem::path path = value == "1" ? std::filesystem::path("land_avoid.png") : std::filesystem::path(value);
	const bool written = DumpPng(path);
	std::array<size_t, 7> counts {};
	for (const auto v : state.avoid)
	{
		++counts[std::min<size_t>(v, counts.size() - 1)];
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "OPENBLACK_DUMP_LAND_AVOID: {} {}; seed ({}, {}), 0 land {}, 6 water {}, 1 avoid {}, 2 unreachable {}",
	                   path.generic_string(), written ? "written" : "NOT written", state.seed[0], state.seed[1], counts[k_Land],
	                   counts[k_Water], counts[k_Avoid], counts[k_Unreachable]);
}

} // namespace openblack::land_avoid
