/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Cumulonimbus.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <iterator>
#include <numbers>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

using namespace openblack::graphics;

namespace
{

uint32_t Mix(uint32_t value)
{
	value ^= value >> 16u;
	value *= 0x7feb352du;
	value ^= value >> 15u;
	value *= 0x846ca68bu;
	value ^= value >> 16u;
	return value;
}

} // namespace

float cumulonimbus::RandomOf(uint32_t seed, uint32_t index)
{
	return static_cast<float>(Mix(seed ^ Mix(index + 0x2c1b3c6du)) >> 8u) / static_cast<float>(1u << 24u);
}

uint32_t cumulonimbus::CellCount(float outerRadius)
{
	const auto more = static_cast<uint32_t>(std::max(outerRadius, 0.0f) / k_RadiusForEachCell);
	return std::clamp(1u + more, 1u, k_MostCellsInStorm);
}

float cumulonimbus::HeightAt(float life)
{
	const float t = std::clamp(life / k_GrowthShare, 0.0f, 1.0f);
	// Fast at the start and slow near the top, as the updraft reaches the tropopause and spreads out
	return 1.0f - (1.0f - t) * (1.0f - t);
}

float cumulonimbus::FadeAt(float life)
{
	const float t = std::clamp((1.0f - life) / k_DecayShare, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

cumulonimbus::Cells cumulonimbus::CellsFor(std::span<const StormSite> storms, float seconds)
{
	// Only a storm with rain builds thunderstorm cells. A storm of snow or of mild weather has none.
	std::vector<StormSite> strongest;
	std::ranges::transform(storms, std::back_inserter(strongest), [](StormSite storm) {
		storm.strength = std::clamp(storm.strength, 0.0f, 1.0f) * std::clamp(storm.rain * k_RainForCells, 0.0f, 1.0f);
		return storm;
	});
	std::erase_if(strongest, [](const StormSite& storm) { return storm.strength <= k_FaintestStorm; });
	std::ranges::stable_sort(strongest, std::greater {}, &StormSite::strength);
	Cells result;
	for (const auto& storm : strongest)
	{
		const uint32_t count = CellCount(storm.outerRadius);
		// A storm that grows lifts its cells to their full height before it is at its full strength
		const float tallness = std::clamp(storm.strength * 1.5f, 0.0f, 1.0f);
		const float presence = std::clamp(storm.strength * 2.0f, 0.0f, 1.0f);
		for (uint32_t slot = 0; slot < count && result.count < k_MaxCells; ++slot)
		{
			Cell cell {.serial = storm.serial, .centre = storm.centre, .radius = k_TowerRadius, .rain = storm.rain};
			if (count == 1)
			{
				// A small storm keeps one cell at its middle
				cell.height = tallness;
				cell.fade = presence;
			}
			else
			{
				// Each cell starts at its own part of the life, so that the cells of a storm take turns
				const float life =
				    seconds / k_LifeSeconds + static_cast<float>(slot) / static_cast<float>(count) + RandomOf(storm.serial, 7);
				const float cycle = std::floor(life);
				const float part = life - cycle;
				const uint32_t seed = Mix(storm.serial * 0x9e3779b9u ^ Mix(slot + 0x51u)) ^ static_cast<uint32_t>(cycle);
				const float angle = RandomOf(seed, 1) * 2.0f * std::numbers::pi_v<float>;
				const float distance = std::sqrt(RandomOf(seed, 2)) * k_CellReach * storm.outerRadius;
				cell.centre += glm::vec2 {std::cos(angle), std::sin(angle)} * distance;
				cell.height = HeightAt(part) * tallness;
				cell.fade = FadeAt(part) * presence;
			}
			result.cells.at(result.count++) = cell;
		}
	}
	return result;
}

std::optional<glm::vec2> cumulonimbus::StrikePlace(std::span<const Cell> cells, uint32_t serial, glm::vec2 wanted,
                                                   const std::array<float, 3>& randoms)
{
	const auto mature = [](const Cell& cell) { return cell.height >= k_MatureHeight && cell.fade > 0.3f; };
	const auto weight = [&](const Cell& cell) {
		if (cell.serial != serial || !mature(cell))
		{
			return 0.0f;
		}
		const float away = glm::distance(cell.centre, wanted) / k_StrikeReach;
		return cell.height * cell.fade / (1.0f + away * away);
	};
	float total = 0.0f;
	for (const auto& cell : cells)
	{
		total += weight(cell);
	}
	const Cell* chosen = nullptr;
	if (total > 0.0f)
	{
		float pick = std::clamp(randoms[0], 0.0f, 1.0f) * total;
		for (const auto& cell : cells)
		{
			const float share = weight(cell);
			if (share <= 0.0f)
			{
				continue;
			}
			chosen = &cell;
			pick -= share;
			if (pick < 0.0f)
			{
				break;
			}
		}
	}
	else
	{
		// No cell is mature yet: the tallest cell of the storm strikes
		for (const auto& cell : cells)
		{
			if (cell.serial == serial && cell.height > 0.0f && (chosen == nullptr || cell.height > chosen->height))
			{
				chosen = &cell;
			}
		}
	}
	if (chosen == nullptr)
	{
		return std::nullopt;
	}
	const float angle = std::clamp(randoms[1], 0.0f, 1.0f) * 2.0f * std::numbers::pi_v<float>;
	const float distance = std::sqrt(std::clamp(randoms[2], 0.0f, 1.0f)) * k_CoreShare * chosen->radius;
	return chosen->centre + glm::vec2 {std::cos(angle), std::sin(angle)} * distance;
}

float cumulonimbus::AnvilGrowth(float height)
{
	const float t = std::clamp((height - 0.75f) / 0.25f, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

glm::vec4 cumulonimbus::TowerBound(const Cell& cell, glm::vec2 downwind)
{
	// The tower bulges out to 1.37 radii; its axis leans downwind with the height
	const glm::vec2 middle = cell.centre + downwind * (0.5f * k_Lean * cell.radius * cell.height * cell.height);
	return {middle, (1.45f + 0.5f * k_Lean) * cell.radius, k_Base + (k_Tropopause - k_Base) * cell.height + k_DomeHeight};
}

glm::vec4 cumulonimbus::AnvilBound(const Cell& cell, glm::vec2 downwind)
{
	const float grown = AnvilGrowth(cell.height);
	const float upwind = k_AnvilUpwind * cell.radius * (0.7f + 0.3f * grown);
	const float reach = k_AnvilDownwind * cell.radius * grown;
	const glm::vec2 middle =
	    cell.centre + downwind * (k_Lean * cell.radius * cell.height * cell.height + 0.5f * (reach - upwind));
	const float halfLength = 0.5f * (reach + upwind);
	const float halfWidth = k_AnvilWidth * cell.radius;
	const float top = k_Base + (k_Tropopause - k_Base) * cell.height;
	// The anvil's rough bottom hangs a little under its depth
	return {middle, grown > 0.0f ? std::sqrt(halfLength * halfLength + halfWidth * halfWidth) : 0.0f,
	        top - k_AnvilDepth - 300.0f};
}

std::array<glm::vec4, cumulonimbus::k_MaxCells * 4> cumulonimbus::Pack(std::span<const Cell> cells, glm::vec2 downwind)
{
	const float length = glm::length(downwind);
	const glm::vec2 direction = length > 1e-4f ? downwind / length : glm::vec2 {1.0f, 0.0f};
	std::array<glm::vec4, k_MaxCells * 4> packed {};
	const size_t count = std::min(cells.size(), k_MaxCells);
	for (size_t i = 0; i < count; ++i)
	{
		const auto& cell = cells[i];
		packed.at(i) = {cell.centre, cell.radius, cell.height};
		packed.at(k_MaxCells + i) = {direction, cell.fade, cell.rain};
		packed.at(2 * k_MaxCells + i) = TowerBound(cell, direction);
		packed.at(3 * k_MaxCells + i) = AnvilBound(cell, direction);
	}
	return packed;
}
