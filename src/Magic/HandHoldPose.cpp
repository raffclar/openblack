/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandHoldPose.h"

#include <cmath>

#include <algorithm>
#include <numbers>
#include <optional>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include "Common/X87Angle.h"
#include "Common/X87Matrix.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::hand_hold;

namespace
{
/// A vector of unit length with its squares added in the game's order, kept as it is when all its parts are zero
template <x87::SumOrder Order>
glm::vec3 UnitInOrder(glm::vec3 v)
{
	return v != glm::vec3(0.0f) ? x87::Scaled<Order>(v) : v;
}
} // namespace

HoldType hand_hold::HoldTypeOf(bool ready, HoldType recorded)
{
	return ready ? recorded : HoldType::Magic;
}

std::optional<HandAnimation::Cycle> hand_hold::HoldCycle(HoldType hold)
{
	switch (hold)
	{
	case HoldType::Above:
		return HandAnimation::Cycle::HoldAbove;
	case HoldType::Magic:
		return HandAnimation::Cycle::Wiggle;
	case HoldType::Grain:
		return HandAnimation::Cycle::Horn;
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		return HandAnimation::Cycle::HoldSide;
	case HoldType::None:
	case HoldType::Fingers:
		break;
	}
	return std::nullopt;
}

uint32_t hand_hold::HoldTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize)
{
	const float handReach = k_StandardHandHeight * handSize;
	switch (hold)
	{
	case HoldType::Above:
	{
		const float share = std::min(reach / (handReach * k_AboveReachShare), 1.0f);
		return static_cast<uint32_t>(static_cast<float>(durationMs) * 0.5f * (1.0f - share));
	}
	case HoldType::Magic:
		return durationMs >> 1;
	case HoldType::Grain:
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
	{
		const float share = std::min(reach / handReach, 1.0f);
		return static_cast<uint32_t>(static_cast<float>(durationMs >> 1) * share);
	}
	case HoldType::None:
	case HoldType::Fingers:
		break;
	}
	return 0;
}

uint32_t hand_hold::TugTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize)
{
	if (hold != HoldType::Above)
	{
		return HoldTimeMs(hold, durationMs, reach, handSize);
	}
	// The pull scales the reach and the hand's height alike, so the share is the reach against the hand's height
	const float share = std::min(reach / (k_StandardHandHeight * handSize), 1.0f);
	return static_cast<uint32_t>(static_cast<float>(durationMs) * 0.5f * (1.0f - share));
}

float hand_hold::SeedHang(HoldType hold, float lowering, float height)
{
	switch (hold)
	{
	case HoldType::Above:
	case HoldType::Grain:
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		return lowering * height;
	case HoldType::None:
	case HoldType::Magic:
	case HoldType::Fingers:
		break;
	}
	return 0.0f;
}

float hand_hold::HoldLift(HoldType hold, float hang, float handSize)
{
	switch (hold)
	{
	case HoldType::Above:
		return k_AboveLift;
	case HoldType::Magic:
		return k_StandardHandHeight * handSize;
	case HoldType::Grain:
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		return std::max(hang, k_MinimumSideLift);
	case HoldType::None:
	case HoldType::Fingers:
		break;
	}
	return 0.0f;
}

glm::vec2 hand_hold::CursorSway(glm::vec2 cursorLag)
{
	return glm::clamp(cursorLag, -k_SwayLag, k_SwayLag) * (k_MaxSway / k_SwayLag);
}

float hand_hold::DistanceFromView(glm::vec3 camera, glm::vec3 hand, bool holding)
{
	// The game adds the squares in two orders: x, y, z while the hand holds a thing, and x, z, y when it holds nothing
	return holding ? x87::Distance<x87::SumOrder::Xyz>(camera, hand) : x87::Distance<x87::SumOrder::Xzy>(camera, hand);
}

glm::vec3 hand_hold::HeldUp(glm::vec3 camera, glm::vec3 hand, float roll, float pitch)
{
	// The line to the camera and the level line across it are made unit unless they have no length. The game turns
	// straight up by a matrix for each turn, multiplies the two matrices and moves straight up through the product.
	auto toCamera = camera - hand;
	if (toCamera != glm::vec3(0.0f))
	{
		toCamera = x87::Scaled<x87::SumOrder::Xyz>(toCamera);
	}
	auto across = glm::vec3(0.0f - toCamera.z, 0.0f, toCamera.x - 0.0f);
	if (across != glm::vec3(0.0f))
	{
		across = x87::Scaled<x87::SumOrder::Xzy>(across);
	}
	const auto turn = x87::Product(x87::TurnAbout(toCamera, roll), x87::TurnAbout(across, pitch));
	// The products for x are added for rows 1, 2, 0, and for y and z in the rows' order
	constexpr x87::AxisOrders k_UpOrders {{{1, 2, 0}, {0, 1, 2}, {0, 1, 2}}};
	return x87::TransformPoint(turn, glm::vec3(0.0f, 1.0f, 0.0f), k_UpOrders);
}

std::optional<float> hand_hold::HeadingOfSight(glm::vec3 lineOfSight)
{
	constexpr float k_LeastLean = 0.01f;
	if (!(std::abs(lineOfSight.x) > k_LeastLean) && !(std::abs(lineOfSight.z) > k_LeastLean))
	{
		return std::nullopt;
	}
	return x87::RoundToFloat(x87::GameHeadingOf(lineOfSight.x, lineOfSight.z));
}

glm::vec3 hand_hold::HeldFacing(float heading)
{
	// Add the half turn as a float. Keep the sine and the cosine of the sum as floats.
	constexpr float k_HalfTurn = std::numbers::pi_v<float>;
	const float turn = heading + k_HalfTurn;
	return {x87::RoundToFloat(x87::Sin(turn)), 0.0f, -x87::RoundToFloat(x87::Cos(turn))};
}

glm::mat3 hand_hold::HeldBasis(glm::vec3 facing, glm::vec3 up)
{
	auto side = glm::cross(facing, up);
	if (side != glm::vec3(0.0f))
	{
		side = glm::normalize(side);
	}
	return {side, up, glm::cross(side, up)};
}

glm::mat3 hand_hold::SeedTurn(const glm::mat3& basis, float yRotate, bool rightHanded)
{
	auto x = basis[0];
	auto z = basis[2];
	if (rightHanded)
	{
		x = -x;
		z = -z;
	}
	// The cosine is stored as a float and the sine stays wide
	const float c = x87::RoundToFloat(x87::Cos(yRotate));
	const double s = x87::Sin(yRotate);
	const auto timesSine = [s](glm::vec3 v) { return glm::vec3(x87::Mul(s, v.x), x87::Mul(s, v.y), x87::Mul(s, v.z)); };
	const auto turnedX = x * c + timesSine(z);
	const auto turnedZ = z * c - timesSine(x);
	// The game adds the squares of the first axis z, x, y and of the other two z, y, x
	return {UnitInOrder<x87::SumOrder::Zxy>(turnedX), UnitInOrder<x87::SumOrder::Zyx>(basis[1]),
	        UnitInOrder<x87::SumOrder::Zyx>(turnedZ)};
}

void WorldPoseFade::Change()
{
	std::swap(_drawn, _kept);
	_time = 0.0f;
}

void WorldPoseFade::Apply(float seconds, const glm::mat4& model, std::vector<glm::mat4>& bones, bool poseIntoKept)
{
	// The bones of a hand with no size cannot come back from the world
	if (glm::determinant(glm::mat3(model)) == 0.0f)
	{
		return;
	}
	std::vector<glm::mat4> world(bones.size());
	std::ranges::transform(bones, world.begin(), [&model](const glm::mat4& bone) { return model * bone; });
	(poseIntoKept ? _kept : _drawn) = world;
	// A pose that the hand never drew before is the pose of the frame
	if (_drawn.size() != world.size())
	{
		_drawn = world;
	}
	if (_kept.size() != world.size())
	{
		_kept = world;
	}
	bool blended = false;
	if (_time.has_value())
	{
		*_time += seconds;
		if (*_time < k_FadeSeconds)
		{
			const float t = *_time / k_FadeSeconds;
			for (size_t bone = 0; bone < _drawn.size(); ++bone)
			{
				_drawn[bone] = (_drawn[bone] - _kept[bone]) * t + _kept[bone];
			}
			blended = true;
		}
		else
		{
			_time.reset();
		}
	}
	if (!blended && !poseIntoKept)
	{
		return;
	}
	const auto toModel = glm::inverse(model);
	for (size_t bone = 0; bone < bones.size(); ++bone)
	{
		bones[bone] = toModel * _drawn[bone];
	}
}

void HeldFade::Change()
{
	_from = _placed.value_or(_from);
	_time = 0.0f;
}

glm::vec3 HeldFade::Step(float seconds, glm::vec3 placed)
{
	// When the hand has no place yet, the blend starts at the place of this frame
	if (!_placed.has_value())
	{
		_from = placed;
	}
	_placed = placed;
	if (!_time.has_value())
	{
		return glm::vec3(0.0f);
	}
	*_time += seconds;
	if (*_time >= k_FadeSeconds)
	{
		_time.reset();
		return glm::vec3(0.0f);
	}
	const float t = *_time / k_FadeSeconds;
	return (placed - _from) * t + _from - placed;
}
