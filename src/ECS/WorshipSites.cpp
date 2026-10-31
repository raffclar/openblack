/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSites.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>

#include "Common/GUtilsDistance.h"
#include "Common/PortableMath.h"
#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "Common/X87Matrix.h"

namespace openblack::ecs::worship_site
{

float PlaceFacing(float templeFacing, uint32_t place)
{
	return templeFacing + static_cast<float>(place) * k_PlaceSpacing;
}

glm::vec2 TurnedPoint(glm::vec2 spot, float facing, glm::vec3 point)
{
	const float cosine = portable_math::Cos(facing);
	const float sine = portable_math::Sin(facing);
	return spot + glm::vec2(cosine * point.x - sine * point.z, sine * point.x + cosine * point.z);
}

glm::vec2 CentreOnLand(glm::vec3 position, const glm::mat3& rotation, float scale)
{
	const auto centre = position + rotation * (k_CentrePoint * scale);
	return {centre.x, centre.z};
}

float EdgeGap(glm::vec2 centre, glm::vec2 thing, float thingRadius)
{
	return gutils::GetDistanceInMetres(centre, thing) - (k_EffectRadius + thingRadius);
}

glm::vec2 PlaceJudgePoint(glm::vec2 temple, float templeFacing, float templeScale, uint32_t place, glm::vec3 templePoint)
{
	// The place turns the temple's point by the angle between the place and the temple. The sine and cosine are floats.
	const float turn = PlaceFacing(templeFacing, place) - templeFacing;
	const float cosine = x87::RoundToFloat(x87::Cos(turn));
	const float sine = x87::RoundToFloat(x87::Sin(turn));
	glm::mat4 placeTurn {1.0f};
	placeTurn[0] = glm::vec4(cosine, 0.0f, sine, 0.0f);
	placeTurn[2] = glm::vec4(-sine, 0.0f, cosine, 0.0f);
	// The temple's matrix, with its turn, size and position on the land
	auto templeMatrix = glm::mat4(x87::GameUprightTurnScaled(templeFacing, templeScale));
	templeMatrix[3] = glm::vec4(temple.x, 0.0f, temple.y, 1.0f);
	const auto judged = x87::TransformPoint(x87::Product(placeTurn, templeMatrix), templePoint, x87::k_MeshPointOrder);
	return {judged.x, judged.z};
}

std::optional<uint32_t> NearestFreePlace(const std::array<bool, k_Places>& taken, glm::vec2 temple, float templeFacing,
                                         float templeScale, glm::vec3 templePoint, glm::vec2 spot)
{
	// Nearer than this or no place at all, the first of equally near places kept
	float nearest = 999999.0f;
	std::optional<uint32_t> best;
	for (uint32_t place = 0; place < k_Places; ++place)
	{
		if (taken.at(place))
		{
			continue;
		}
		const auto judged = PlaceJudgePoint(temple, templeFacing, templeScale, place, templePoint);
		const float distance = gutils::GetDistanceInMetres(spot, judged);
		if (distance < nearest)
		{
			nearest = distance;
			best = place;
		}
	}
	return best;
}

bool MayHaveSite(int32_t landNumber, bool stoppedByScript, uint32_t population)
{
	return landNumber != k_LandWithoutSites && !stoppedByScript && population != 0;
}

} // namespace openblack::ecs::worship_site
