/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildingSiteRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::building_site;

namespace
{
/// The game's turn as a float, and the step between a site's places
constexpr float k_Turn = 6.2831855f;
constexpr float k_PlaceStep = 0.049087387f;
/// One place's share of a turn and the places in a turn, as the game finds a place from an angle
constexpr float k_InverseTurn = 0.15915494f;
constexpr float k_PlacesPerTurn = 128.0f;
constexpr float k_PlaceShare = 0.0078125f;
/// Beyond three turns either way an angle isn't wrapped
constexpr float k_ThreeTurns = 18.849556f;
constexpr float k_HalfTurn = 3.1415927f;
/// A builder's places in this length of the building's outline
constexpr float k_StrokeOutline = 2.0f;
/// How far a builder's first place may lie either side of the way to it
constexpr float k_QuarterTurn = 1.5707964f;
constexpr float k_EighthTurn = 0.78539819f;
/// Evil land makes a stroke use up to this much more wood
constexpr float k_AlignmentWood = 0.2f;
constexpr float k_MostWoodMultiplier = 1.2f;
/// A temple's piles start a seventh of a turn apart, less this
constexpr float k_SeventhTurn = 0.89759791f;
constexpr float k_PileAngleOffset = 1.1423974f;
/// Each place is put this much farther out from the model's middle
constexpr float k_PlaceOutset = 1.0f;
/// The tiny amount the game adds to a villager's room and load before dividing them
constexpr float k_Tiny = 1e-5f;

/// Whether a point lies between two others in x and in z, ends included
bool Between(glm::vec3 a, glm::vec3 b, glm::vec3 point)
{
	const auto within = [](float from, float to, float value) {
		return from < to ? (value >= from && value <= to) : (value >= to && value <= from);
	};
	return within(a.x, b.x, point.x) && within(a.z, b.z, point.z);
}

/// Where the line through a point along a direction crosses the upright plane through a centre that holds a direction
/// on the ground, if it isn't parallel to it
std::optional<glm::vec3> CrossPlane(glm::vec3 point, glm::vec3 direction, glm::vec3 centre, glm::vec3 normal)
{
	const float denominator = normal.z * direction.z + direction.x * normal.x;
	if (denominator == 0.0f)
	{
		return std::nullopt;
	}
	const float t = ((centre.x - point.x) * normal.x + (centre.z - point.z) * normal.z) / denominator;
	return point + direction * t;
}

glm::vec3 Transform(const glm::mat4& model, glm::vec3 point)
{
	return glm::vec3(model * glm::vec4(point, 1.0f));
}
} // namespace

int32_t building_site::BuildersNeeded(const BuildersNeededInputs& in)
{
	if (in.built && (in.repaired || in.repairDesire == 0.0f))
	{
		return 0;
	}
	return in.maxBuilders - in.workers;
}

float building_site::DesireForVillagers(int32_t needed, int32_t maxBuilders, float forcedDesire)
{
	const float desire = static_cast<float>(needed) / static_cast<float>(maxBuilders) + forcedDesire;
	if (desire < 0.0f)
	{
		return 0.0f;
	}
	return desire > 1.0f ? 1.0f : desire;
}

float building_site::WoodValue(float scale, uint32_t woodValue, float tribalPower)
{
	return static_cast<float>(woodValue) * scale / tribalPower;
}

float building_site::WoodNeededToBuild(float builtOrLife, float woodValue, uint32_t woodAtSite)
{
	return (1.0f - builtOrLife) * woodValue - static_cast<float>(woodAtSite);
}

bool building_site::ShouldFetchWood(const ShouldFetchInputs& in)
{
	if (in.distanceToBuilding < k_NearBuilding)
	{
		return in.woodAtSite == 0;
	}
	float total = static_cast<float>(in.woodAtSite);
	for (const auto wood : in.workersWood)
	{
		total = total + static_cast<float>(wood);
	}
	if (!(total < in.woodNeeded))
	{
		return false;
	}
	const auto builders = static_cast<int32_t>(in.workersWood.size()) + 1;
	float stocked = static_cast<float>(in.woodHeld) / static_cast<float>(in.maxWoodCarried) +
	                total / static_cast<float>(builders * in.woodPerBuilderWanted);
	if (stocked >= 1.0f)
	{
		stocked = 1.0f;
	}
	const float siteScore = gutils::GetDistanceModifier(in.distanceToBuilding, k_FetchDistanceScale) * stocked;
	const float storeScore = gutils::GetDistanceModifier(in.distanceToDropOff, k_FetchDistanceScale) * (1.0f - stocked);
	return siteScore < storeScore;
}

int32_t building_site::WoodPerStroke(float woodUsedPerBuildCycle, float landAlignment)
{
	float multiplier = 1.0f - landAlignment * k_AlignmentWood;
	if (multiplier < 1.0f)
	{
		multiplier = 1.0f;
	}
	else if (multiplier > k_MostWoodMultiplier)
	{
		multiplier = k_MostWoodMultiplier;
	}
	return static_cast<int32_t>(woodUsedPerBuildCycle * multiplier);
}

uint32_t building_site::PlaceAt(float angle)
{
	if (angle < -k_ThreeTurns)
	{
		return 0;
	}
	if (angle > k_ThreeTurns)
	{
		angle = k_HalfTurn;
	}
	else
	{
		while (angle < 0.0f)
		{
			angle = angle + k_Turn;
		}
		while (angle > k_Turn)
		{
			angle = angle - k_Turn;
		}
		if (angle == 0.0f)
		{
			return 0;
		}
	}
	return static_cast<uint32_t>(static_cast<int32_t>(angle * k_InverseTurn * k_PlacesPerTurn)) & 0x7Fu;
}

uint32_t building_site::NextPlace(uint32_t place, float buildingRadius, const FloatRandom& floatRandom,
                                  const IntRandom& intRandom)
{
	const float perOutline = k_StrokeOutline / (buildingRadius * k_Turn * k_PlaceShare);
	const auto step = static_cast<int32_t>(floatRandom(perOutline * 0.5f) + perOutline);
	const int32_t direction = intRandom(2) != 0 ? 1 : -1;
	auto next = static_cast<int32_t>(place) + direction * step;
	// Wrapped once round, as the game does
	if (next >= static_cast<int32_t>(k_Places))
	{
		next -= static_cast<int32_t>(k_Places);
	}
	else if (next < 0)
	{
		next += static_cast<int32_t>(k_Places);
	}
	return static_cast<uint32_t>(next);
}

float building_site::FirstPlaceAngle(float angleToBuilder, const FloatRandom& floatRandom)
{
	return floatRandom(k_QuarterTurn) - k_EighthTurn + angleToBuilder;
}

Places building_site::OutlinePlaces(std::span<const Triangle> modelTriangles, const glm::mat4& model, glm::vec3 boxCentre,
                                    float reach)
{
	const auto origin = glm::vec3(model[3]);
	Places places;
	places.fill(origin);
	std::array<float, k_Places> farthest {};
	const auto centre = Transform(model, boxCentre);
	for (const auto& triangle : modelTriangles)
	{
		// Its two lowest corners, in the order the game picks them
		const auto& [a, b, c] = triangle.corners;
		const glm::vec3* lowest = nullptr;
		const glm::vec3* second = nullptr;
		if (a.y <= b.y)
		{
			if (c.y <= a.y)
			{
				lowest = &c;
				second = &a;
			}
			else
			{
				lowest = &a;
				second = c.y <= b.y ? &c : &b;
			}
		}
		else if (c.y <= b.y)
		{
			lowest = &c;
			second = &b;
		}
		else
		{
			lowest = &b;
			second = a.y <= c.y ? &a : &c;
		}
		if (!(lowest->y < k_FootHeight) || !(second->y < k_FootHeight))
		{
			continue;
		}
		const auto start = Transform(model, *lowest);
		const auto end = Transform(model, *second);
		const auto along = end - start;
		const float length = std::sqrt(glm::dot(along, along));
		const auto direction = along / length;
		for (size_t i = 0; i < k_Places; ++i)
		{
			const float angle = static_cast<float>(static_cast<int32_t>(i)) * k_PlaceStep;
			const auto ray = glm::vec3(std::cos(angle) * reach + centre.x, centre.y, std::sin(angle) * reach + centre.z);
			// The upright plane holding the ray
			auto flat = glm::vec3(ray.x - centre.x, 0.0f, ray.z - centre.z);
			flat /= std::sqrt(flat.x * flat.x + flat.z * flat.z);
			const auto normal = glm::vec3(-flat.z, 0.0f, flat.x);
			const auto cross = CrossPlane(start, direction, centre, normal);
			if (!cross.has_value() || !Between(start, end, *cross) || !Between(centre, ray, *cross))
			{
				continue;
			}
			const float dz = cross->z - centre.z;
			const float dx = cross->x - centre.x;
			const float distance = dz * dz + dx * dx;
			if (distance > farthest.at(i))
			{
				places.at(i) = *cross;
				farthest.at(i) = distance;
			}
		}
	}
	for (auto& place : places)
	{
		auto offset = place - centre;
		const float length = std::sqrt(glm::dot(offset, offset));
		if (offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f)
		{
			offset *= (length + k_PlaceOutset) / length;
		}
		place = centre + offset;
	}
	return places;
}

Places building_site::CirclePlaces(glm::vec3 centre, float reach, const std::function<float(glm::vec2)>& landHeight)
{
	Places places;
	float angle = 0.0f;
	for (auto& place : places)
	{
		place.x = std::sin(angle) * reach + centre.x;
		place.z = std::cos(angle) * reach + centre.z;
		place.y = landHeight({place.x, place.z});
		angle = angle + k_PlaceStep;
	}
	return places;
}

float building_site::TemplePileAngle(float heartYAngle, size_t pile)
{
	return heartYAngle + static_cast<float>(static_cast<int32_t>(pile)) * k_SeventhTurn - k_PileAngleOffset;
}

std::optional<size_t> building_site::BestSite(std::span<const SiteCandidate> sites, bool anyway)
{
	std::optional<size_t> best;
	float bestDistance = 99999.0f;
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto& site = sites[i];
		if (!site.wantsBuilders && !anyway)
		{
			continue;
		}
		const float distance = site.distanceToEdge * (site.desireForVillagers * 0.9f + 0.1f);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}

WoodSource building_site::DecideWoodSource(const WoodSourceInputs& in)
{
	float storeWeight = 0.0f;
	float forestWeight = 0.0f;
	if (!in.forBuilding)
	{
		storeWeight = 1.0f - (static_cast<float>(in.room) + k_Tiny) / (static_cast<float>(in.maxWoodCarried) + k_Tiny);
		forestWeight = 1.0f - storeWeight;
	}
	else
	{
		forestWeight = 0.5f;
		storeWeight = static_cast<uint32_t>(in.room) < in.storeWood ? 1.0f : 0.0f;
	}
	const float storeScore = gutils::GetDistanceModifier(in.distanceToStore, in.maxDistance) * storeWeight;
	float forestScore = 0.0f;
	if (in.distanceToForest.has_value())
	{
		forestScore = gutils::GetDistanceModifier(*in.distanceToForest, in.maxDistance) * forestWeight;
	}
	if (storeScore > forestScore)
	{
		return WoodSource::Store;
	}
	if (forestScore == 0.0f || !in.distanceToForest.has_value())
	{
		return WoodSource::None;
	}
	return in.forestHasBigForest ? WoodSource::BigForest : WoodSource::Forest;
}
