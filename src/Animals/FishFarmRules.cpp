/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarmRules.h"

#include <algorithm>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"

namespace openblack::fish_farm
{
namespace
{
/// The kinds of food a farm's table can name, of which any makes it hold food
constexpr uint32_t k_FoodKinds = 3u;
} // namespace

float Full(const Type& type)
{
	return (type.foodType & k_FoodKinds) != 0 ? type.foodValue : 0.0f;
}

float Grow(float fish, uint32_t turn, const Type& type)
{
	if (type.turnsPerFish != 0 && turn % type.turnsPerFish == 0)
	{
		fish += 1.0f;
	}
	if (fish < 0.0f)
	{
		return 0.0f;
	}
	const float full = Full(type);
	return full < fish ? full : fish;
}

uint32_t Take(float& fish, uint32_t wanted)
{
	if (static_cast<float>(wanted) <= fish)
	{
		fish -= static_cast<float>(wanted);
		return wanted;
	}
	const auto taken = static_cast<uint32_t>(fish);
	fish = 0.0f;
	return taken;
}

uint32_t FirstHandful(uint32_t initialScoop, const Type& type)
{
	const float full = Full(type);
	return static_cast<uint32_t>(full <= static_cast<float>(initialScoop) ? full : static_cast<float>(initialScoop));
}

uint32_t ScoopWanted(uint32_t ramp, const Type& type, uint32_t held, uint32_t maxPickedUp)
{
	const float full = Full(type);
	auto wanted = static_cast<uint32_t>(full <= static_cast<float>(ramp) ? full : static_cast<float>(ramp));
	if (maxPickedUp != 0)
	{
		const auto room = static_cast<uint32_t>(std::max<int64_t>(int64_t {maxPickedUp} - int64_t {held}, 0));
		wanted = std::min(wanted, room);
	}
	return wanted;
}

uint32_t DesireToBeFished(size_t fishermen, uint32_t maxFishermen)
{
	// The farm is always taken as full, so only its fishermen count
	constexpr float k_PercentFull = 1.0f;
	float lacking = static_cast<float>(fishermen) / static_cast<float>(maxFishermen);
	if (!(lacking < 1.0f))
	{
		lacking = 1.0f;
	}
	return static_cast<uint32_t>(map_coords::FtoL(k_PercentFull * (1.0f - lacking)));
}

std::optional<size_t> BestFarm(std::span<const Candidate> farms, uint32_t maxFishermen)
{
	std::optional<size_t> best;
	float bestScore = 0.0f;
	for (size_t i = 0; i < farms.size(); ++i)
	{
		const auto& farm = farms[i];
		const float score = static_cast<float>(DesireToBeFished(farm.fishermen, maxFishermen)) *
		                    gutils::GetDistanceModifier(farm.distance, k_FishermanReach);
		if (score > bestScore)
		{
			bestScore = score;
			best = i;
		}
	}
	return best;
}

glm::ivec2 FishingSpot(glm::ivec2 farm, glm::vec2 offset)
{
	// Each axis goes out to metres, moves, and comes back, truncated
	const auto axis = [](int32_t fixed, float metres) {
		const double moved =
		    (static_cast<double>(fixed) * 10.0 * (1.0 / 65536.0) + static_cast<double>(metres)) * 65536.0 / 10.0;
		return static_cast<int32_t>(moved);
	};
	return {axis(farm.x, offset.x), axis(farm.y, offset.y)};
}

namespace
{
/// The room a villager has left for food, as the game keeps it: a 16-bit count
[[nodiscard]] float Room(uint32_t capacity, uint32_t held)
{
	return static_cast<float>(static_cast<int16_t>(static_cast<uint16_t>(capacity - held)));
}
} // namespace

int32_t Catch(uint32_t capacity, uint32_t held, uint32_t season, float tribalPower)
{
	const float share = static_cast<float>(capacity) * k_CatchShare;
	const float wanted = share * k_SeasonCatch.at(std::min<size_t>(season, k_SeasonCatch.size() - 1));
	const float room = Room(capacity, held);
	const float caught = (wanted <= room ? wanted : room) * tribalPower;
	return map_coords::FtoL(caught);
}

bool TakesCatchToStore(uint32_t capacity, uint32_t held, uint32_t caught)
{
	const float room = Room(capacity, held);
	return room < static_cast<float>(caught) || room == 0.0f;
}

} // namespace openblack::fish_farm
