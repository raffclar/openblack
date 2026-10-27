/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandGlow.h"

#include <cmath>

#include <algorithm>
#include <functional>

#include "Common/ModelInverseSquareRoot.h"
#include "Common/PortableMath.h"

using namespace openblack;

namespace
{
/// The milliseconds in a second, as the game scales its times
constexpr float k_SecondsPerMillisecond = 0.001f;

/// The child of a bone with the lowest number
std::optional<uint32_t> FirstChild(std::span<const uint32_t> parents, uint32_t bone)
{
	for (uint32_t child = 0; child < parents.size(); ++child)
	{
		if (child != bone && parents[child] == bone)
		{
			return child;
		}
	}
	return std::nullopt;
}

/// The next bone after a bone under the same parent
std::optional<uint32_t> NextSibling(std::span<const uint32_t> parents, uint32_t bone)
{
	for (auto sibling = bone + 1; sibling < parents.size(); ++sibling)
	{
		if (parents[sibling] == parents[bone])
		{
			return sibling;
		}
	}
	return std::nullopt;
}
} // namespace

void creature_hand_glow::Set(Glows& glows, Place place, float scale, float power)
{
	auto& glow = glows.at(static_cast<size_t>(place));
	if (power == 0.0f)
	{
		glow.reset();
		return;
	}
	if (!glow.has_value())
	{
		glow = Glow {};
	}
	glow->power = power;
	glow->scale = scale;
}

std::optional<creature_hand_glow::Place> creature_hand_glow::PlaceOf(int32_t number)
{
	if (number < 0 || number >= static_cast<int32_t>(k_PlaceCount))
	{
		return std::nullopt;
	}
	return static_cast<Place>(number);
}

bool creature_hand_glow::AnyActive(const Glows& glows)
{
	return std::ranges::any_of(glows, [](const auto& glow) { return glow.has_value() && glow->power > 0.0f; });
}

creature_hand_glow::Hum creature_hand_glow::SetCreedPower(Glows& glows, Place place, float scale, float power)
{
	const bool before = AnyActive(glows);
	Set(glows, place, scale, power);
	const bool after = AnyActive(glows);
	if (before == after)
	{
		return Hum::Unchanged;
	}
	return after ? Hum::Start : Hum::Stop;
}

creature_hand_glow::SavedPower creature_hand_glow::SavedPowerOf(const Glows& glows, Place place)
{
	const auto& glow = glows.at(static_cast<size_t>(place));
	if (!glow.has_value())
	{
		return {};
	}
	return {.scale = glow->scale, .power = glow->power};
}

creature_hand_glow::Hum creature_hand_glow::LoadCreedPowers(Glows& glows, std::span<const SavedPower, k_PlaceCount> powers)
{
	const bool before = AnyActive(glows);
	// The loaded creature is new, so it has no glows before it gets the saved powers
	glows = {};
	for (size_t place = 0; place < k_PlaceCount; ++place)
	{
		Set(glows, static_cast<Place>(place), powers[place].scale, powers[place].power);
	}
	const bool after = AnyActive(glows);
	if (before == after)
	{
		return Hum::Unchanged;
	}
	return after ? Hum::Start : Hum::Stop;
}

std::optional<creature_hand_glow::CreedEvent> creature_hand_glow::CreedEventOf(int32_t event)
{
	if (event < k_FirstCreedEvent || event > k_LastCreedEvent)
	{
		return std::nullopt;
	}
	const auto step = static_cast<size_t>(event - k_FirstCreedEvent);
	const float level = k_CreedEventLevels.at(step % k_CreedEventLevels.size());
	// The power is the level added to itself
	return CreedEvent {
	    .place = k_CreedEventPlaces.at(step / k_CreedEventLevels.size()),
	    .power = level + level,
	};
}

float creature_hand_glow::Zoom(const Glow& glow)
{
	return k_ZoomPerPower * glow.power;
}

void creature_hand_glow::Slide(Motion& motion, int32_t frameMs)
{
	const float seconds = static_cast<float>(frameMs) * k_SecondsPerMillisecond;
	const float across = seconds * k_SlideSpeed.x;
	const float up = seconds * k_SlideSpeed.y;
	motion.slide.x = std::fmod(across + motion.slide.x, 1.0f);
	motion.slide.y = std::fmod(up + motion.slide.y, 1.0f);
}

void creature_hand_glow::Turn(Motion& motion, int32_t gameMs)
{
	const float scaled = static_cast<float>(gameMs) * k_TurnSpeed;
	const float radians = scaled * k_SecondsPerMillisecond;
	motion.turn = radians + motion.turn;
}

std::vector<uint32_t> creature_hand_glow::BranchEnds(std::span<const uint32_t> parents, uint32_t bone)
{
	std::vector<uint32_t> ends;
	if (bone >= parents.size())
	{
		return ends;
	}
	// The bone and each bone after it under its parent: a bone with children gives the ends under its first child and
	// that child's later siblings, and a bone without children is an end
	const std::function<void(uint32_t)> walk = [&](uint32_t first) {
		for (std::optional<uint32_t> at = first; at.has_value(); at = NextSibling(parents, *at))
		{
			if (const auto child = FirstChild(parents, *at); child.has_value())
			{
				walk(*child);
			}
			else
			{
				ends.push_back(*at);
			}
		}
	};
	walk(bone);
	return ends;
}

glm::vec3 creature_hand_glow::Middle(std::span<const glm::vec3> points)
{
	glm::vec3 sum(0.0f);
	for (const auto& point : points)
	{
		sum.x = sum.x + point.x;
		sum.y = sum.y + point.y;
		sum.z = sum.z + point.z;
	}
	if (points.empty())
	{
		return sum;
	}
	const float share = 1.0f / static_cast<float>(points.size());
	return {share * sum.x, share * sum.y, share * sum.z};
}

glm::mat3 creature_hand_glow::UnitAxes(const glm::mat3& axes)
{
	glm::mat3 unit = axes;
	for (int i = 0; i < 3; ++i)
	{
		auto& axis = unit[i];
		const float x = axis.x * axis.x;
		const float y = axis.y * axis.y;
		const float z = axis.z * axis.z;
		const float xy = x + y;
		const float inverse = gutils::ModelInverseSquareRoot(xy + z);
		axis = {inverse * axis.x, inverse * axis.y, inverse * axis.z};
	}
	return unit;
}

glm::mat3 creature_hand_glow::CirclingAxes(float turn)
{
	// The sine and cosine keep the processor's wider precision until they are stored
	const auto c = static_cast<float>(portable_math::Cos(static_cast<double>(turn)));
	const auto s = static_cast<float>(portable_math::Sin(static_cast<double>(turn)));
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

glm::vec3 creature_hand_glow::CirclingPoint(const glm::vec3& bodyOrigin, float size)
{
	const float sized = k_CirclingHeight * size;
	const float height = sized * k_HeightPerSize;
	return {bodyOrigin.x, height + bodyOrigin.y, bodyOrigin.z};
}

glm::mat4 creature_hand_glow::ModelMatrix(const glm::mat3& unitAxes, const glm::vec3& point, float bodyScale, float glowScale)
{
	const float scale = k_BodyScale * bodyScale;
	glm::mat4 model(1.0f);
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			float value = unitAxes[i][j] * scale;
			if (i == 2)
			{
				value = value * k_BackAxis;
			}
			model[i][j] = glowScale * value;
		}
	}
	model[3] = glm::vec4(point, 1.0f);
	return model;
}
