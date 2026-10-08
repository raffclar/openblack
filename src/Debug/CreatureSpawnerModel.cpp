/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpawnerModel.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>

#include <glm/geometric.hpp>

namespace openblack::debug::creature_spawner
{

namespace
{
// Indexed by species, from the cow
constexpr std::array<std::string_view, k_SpeciesCount> k_SpeciesNames {
    "Cow",        "Tiger", "Leopard", "Wolf", "Lion",     "Horse", "Tortoise", "Zebra",     "Brown Bear",
    "Polar Bear", "Sheep", "Chimp",   "Ogre", "Mandrill", "Rhino", "Gorilla",  "Giant Ape",
};

constexpr std::array<std::string_view, static_cast<size_t>(PlayerNames::_COUNT)> k_OwnerNames {
    "Player One", "Player Two", "Player Three", "Player Four", "Player Five", "Player Six", "Player Seven", "Neutral",
};

/// Marks are kept this far from the skin's edges
constexpr int k_SkinMargin = 16;
constexpr int k_SkinSize = 256;
} // namespace

std::string_view SpeciesName(CreatureType species)
{
	const auto index = static_cast<size_t>(species);
	return index >= 1 && index <= k_SpeciesNames.size() ? k_SpeciesNames.at(index - 1) : "Unknown";
}

CreatureType SpeciesAt(size_t index)
{
	return static_cast<CreatureType>(index + 1);
}

std::string_view OwnerName(PlayerNames owner)
{
	const auto index = static_cast<size_t>(owner);
	return index < k_OwnerNames.size() ? k_OwnerNames.at(index) : "Nobody";
}

glm::vec3 DrawnScale(const std::optional<glm::vec3>& poseScale, const glm::vec3& ownScale)
{
	return poseScale.value_or(ownScale);
}

int32_t ClampPhase(int phase, int32_t lastPhase)
{
	return std::clamp<int32_t>(phase, 0, lastPhase);
}

std::string KnownLeashes(const std::bitset<creature_leash::k_Types.size()>& known)
{
	std::string text;
	for (size_t i = 0; i < creature_leash::k_Types.size(); ++i)
	{
		if (known.test(i))
		{
			text += text.empty() ? "" : ", ";
			text += creature_leash::Name(creature_leash::k_Types.at(i));
		}
	}
	return text.empty() ? "none" : text;
}

std::vector<creature_desires::Desire> DesiresByStrength(const creature_desires::Desires& desires)
{
	std::vector<creature_desires::Desire> order;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto& state = desires.desires.at(i);
		if (state.activated && (!state.sources.empty() || state.value > 0.0f))
		{
			order.push_back(static_cast<creature_desires::Desire>(i));
		}
	}
	std::ranges::stable_sort(order, std::greater {},
	                         [&desires](creature_desires::Desire desire) { return desires[desire].value; });
	return order;
}

PlayerCreatureSummary Summarise(const PlayerCreatureParts& parts, size_t maxDesires)
{
	PlayerCreatureSummary summary {
	    .entity = parts.entity,
	    .species = parts.species,
	    .position = parts.position,
	    .size = parts.size,
	    .drawnScale = DrawnScale(parts.poseScale, parts.ownScale),
	    .alignment = parts.alignment,
	    .developmentPhase = parts.developmentPhase,
	    .home = parts.home,
	    .wornLeash = parts.wornLeash,
	    .knownLeashes = KnownLeashes(parts.knownLeashes),
	    .leashable = parts.leashable,
	    .desires = {},
	    .plan = parts.plan,
	    .needs = std::nullopt,
	};
	if (parts.desires != nullptr)
	{
		summary.desires = DesiresByStrength(*parts.desires);
		if (summary.desires.size() > maxDesires)
		{
			summary.desires.resize(maxDesires);
		}
	}
	if (parts.needs != nullptr)
	{
		summary.needs = *parts.needs;
	}
	return summary;
}

glm::vec3 PointAround(const glm::vec3& position, const glm::mat3& rotation, float angle, float distance)
{
	// A creature looks down its -z axis
	auto ahead = rotation * glm::vec3(std::sin(angle), 0.0f, -std::cos(angle));
	ahead.y = 0.0f;
	const auto length = glm::length(ahead);
	ahead = length > 0.0f ? ahead / length : glm::vec3(0.0f, 0.0f, -1.0f);
	return position + (ahead * distance);
}

glm::vec3 OnGround(const glm::vec3& point, float groundHeight)
{
	return {point.x, groundHeight, point.z};
}

float RandomFacingDegrees(RandomEngine& random)
{
	return std::uniform_real_distribution(0.0f, 360.0f)(random);
}

uint8_t RandomSkinTexel(RandomEngine& random)
{
	return static_cast<uint8_t>(std::uniform_int_distribution<int>(k_SkinMargin, k_SkinSize - 1 - k_SkinMargin)(random));
}

size_t RandomIndex(RandomEngine& random, size_t count)
{
	return std::uniform_int_distribution<size_t>(0, count - 1)(random);
}

std::string Narrow(std::u16string_view text)
{
	std::string narrow;
	narrow.reserve(text.size());
	std::ranges::transform(text, std::back_inserter(narrow), [](char16_t c) { return c < 0x80 ? static_cast<char>(c) : '?'; });
	return narrow;
}

bool IsMindFileName(std::string_view fileName)
{
	return !fileName.empty() && !fileName.starts_with("Physique");
}

} // namespace openblack::debug::creature_spawner
