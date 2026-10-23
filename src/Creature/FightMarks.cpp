/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightMarks.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include "Common/PortableRotations.h"
#include "Common/X87.h"

using namespace openblack::creature_fight::marks;

namespace
{
/// The colour's three ramps: dark red to red over the last third of the queue, red to yellow over the middle third,
/// yellow to white over the first, each ramp steepened to reach full strength across its third
constexpr double k_RedEnd = 0.33;
constexpr double k_GreenEnd = 0.66;
constexpr double k_RampScale = 3.0303030303030303;
constexpr double k_ByteMax = 255.0;
/// The bits laid over the red before the green and blue are shifted in: every sparkle is drawn with alpha 0xA0
constexpr uint32_t k_AlphaOverRed = 0xFFFFA000u;
constexpr float k_ChannelMax = 255.0f;

uint32_t Ramp(float share)
{
	// Each multiply by a double rounds to a float, and the result is cut towards nothing
	return static_cast<uint32_t>(openblack::x87::Mul(openblack::x87::Mul(share, k_RampScale), k_ByteMax));
}

} // namespace

bool MarkList::Add(const Mark& mark)
{
	if (Full())
	{
		return false;
	}
	_marks.at(_count) = mark;
	_marks.at(_count).chargeMs = 0.0f;
	_marks.at(_count).frame = 0;
	++_count;
	return true;
}

void MarkList::KeepNewest()
{
	if (_count > 1)
	{
		const auto frame = _marks.front().frame;
		_marks.front() = _marks.at(_count - 1);
		_marks.front().frame = frame;
		_count = 1;
	}
}

void MarkList::PopFront()
{
	if (_count == 0)
	{
		return;
	}
	std::shift_left(_marks.begin(), _marks.begin() + static_cast<std::ptrdiff_t>(_count), 1);
	--_count;
}

void MarkList::DropNewest()
{
	if (_count > 0)
	{
		--_count;
	}
}

void MarkList::Charge(float elapsedMs)
{
	if (_count == 0)
	{
		return;
	}
	auto& newest = _marks.at(_count - 1);
	newest.chargeMs = std::min(newest.chargeMs + elapsedMs, k_FullChargeMs);
}

void MarkList::Animate()
{
	for (auto& mark : std::span(_marks.data(), _count))
	{
		if (mark.anchor == Anchor::Opponent)
		{
			mark.frame = NextFrame(mark.frame, mark.chargeMs);
		}
	}
}

std::optional<size_t> openblack::creature_fight::marks::HandMark(std::span<const Mark> marks, bool pressHeld)
{
	if (!pressHeld)
	{
		return std::nullopt;
	}
	std::optional<size_t> last;
	for (size_t place = 0; place < marks.size(); ++place)
	{
		if (marks[place].anchor == Anchor::Opponent)
		{
			last = place;
		}
	}
	return last;
}

uint32_t openblack::creature_fight::marks::NextFrame(uint32_t frame, float chargeMs)
{
	// Whole milliseconds, as the game counted them
	const auto charge = static_cast<uint32_t>(std::clamp(chargeMs, 0.0f, k_FullChargeMs));
	return (frame + (charge * 8 / static_cast<uint32_t>(k_FullChargeMs))) % k_FrameCount;
}

uint32_t openblack::creature_fight::marks::ColourOf(size_t place, size_t capacity)
{
	// The two counts are exact, and the share rounds to a float
	const float share = openblack::x87::Div(static_cast<double>(static_cast<int32_t>(capacity) - static_cast<int32_t>(place)),
	                                        static_cast<double>(static_cast<int32_t>(capacity)));
	uint32_t red = 0xFF;
	uint32_t green = 0;
	uint32_t blue = 0;
	if (openblack::x87::Less(share, k_RedEnd))
	{
		red = Ramp(share);
	}
	else if (openblack::x87::Less(share, k_GreenEnd))
	{
		green = Ramp(openblack::x87::Sub(share, k_RedEnd));
	}
	else
	{
		green = 0xFF;
		blue = Ramp(openblack::x87::Sub(share, k_GreenEnd));
	}
	// The channels are laid side by side without being kept to a byte: a blue over 255 spills into the green
	return (((((red | k_AlphaOverRed) << 8u) | green) << 8u) | blue);
}

glm::vec4 openblack::creature_fight::marks::Rgba(uint32_t argb)
{
	return glm::vec4(static_cast<float>((argb >> 16u) & 0xFFu), static_cast<float>((argb >> 8u) & 0xFFu),
	                 static_cast<float>(argb & 0xFFu), static_cast<float>(argb >> 24u)) /
	       k_ChannelMax;
}

glm::vec3 openblack::creature_fight::marks::DrawnAt(const glm::vec3& at, const glm::vec3& eye)
{
	return at + ((eye - at) * k_TowardsCamera);
}

glm::vec4 openblack::creature_fight::marks::FrameUv(uint32_t frame)
{
	constexpr float k_Cell = 1.0f / static_cast<float>(k_FramesPerRow);
	const auto cell = frame % k_FrameCount;
	return {static_cast<float>(cell % k_FramesPerRow) * k_Cell, static_cast<float>(cell / k_FramesPerRow) * k_Cell, k_Cell,
	        k_Cell};
}

glm::vec3 openblack::creature_fight::marks::ToLocal(const glm::mat4& frame, const glm::vec3& point)
{
	return glm::vec3(glm::inverse(frame) * glm::vec4(point, 1.0f));
}

glm::vec3 openblack::creature_fight::marks::ToWorld(const glm::mat4& frame, const glm::vec3& local)
{
	return glm::vec3(frame * glm::vec4(local, 1.0f));
}

glm::mat4 openblack::creature_fight::marks::GroundFrame(const glm::vec3& position, GameHeading heading)
{
	// The frame turns as the drawn body does
	return openblack::portable_math::Rotate(glm::translate(glm::mat4(1.0f), position), heading.Yaw(),
	                                        glm::vec3(0.0f, 1.0f, 0.0f));
}

bool Pace::Advance(float realMs)
{
	_sinceStepMs += realMs;
	if (_sinceStepMs < k_StepMs)
	{
		return false;
	}
	// One step a frame at most: a long frame leaves no steps owed, only its share of the next
	_sinceStepMs = std::fmod(_sinceStepMs - k_StepMs, k_StepMs);
	return true;
}
