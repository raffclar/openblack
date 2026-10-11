/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VolumeGlow.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

namespace volume_glow = openblack::particles::volume_glow;

namespace
{
constexpr uint32_t k_ByteMax = 255;
constexpr uint32_t k_White = 0xFFFFFF;
/// A corner of the inner edge starts to glow once it is seen this side on, and is fully lit at the far end
constexpr float k_EdgeStart = 0.57f;
constexpr float k_EdgeEnd = 2.0f;
} // namespace

float volume_glow::Strength(float power)
{
	if (!(power > 0.0f))
	{
		return 0.0f;
	}
	return power >= 1.0f ? 1.0f : power;
}

float volume_glow::Zoom(const Settings& settings, float power, float atomScale)
{
	if (settings.alphaFromStrength)
	{
		return settings.zoomMin;
	}
	return ((settings.zoomMax - settings.zoomMin) * Strength(power) + settings.zoomMin) * atomScale;
}

uint32_t volume_glow::Alpha(const Settings& settings, float power)
{
	const auto alpha = static_cast<uint32_t>(std::clamp(settings.alpha, 0, static_cast<int>(k_ByteMax)));
	if (!settings.alphaFromStrength)
	{
		return alpha;
	}
	// Whole steps, rounded down
	return static_cast<uint32_t>(static_cast<float>(alpha) * Strength(power));
}

uint32_t volume_glow::Argb(const Settings& settings, uint32_t alpha, uint8_t collectionAlpha, std::optional<uint32_t> playerRgb)
{
	const uint32_t a = (static_cast<uint32_t>(collectionAlpha) * alpha) / k_ByteMax;
	const uint32_t rgb = settings.usePlayerColour && playerRgb.has_value() ? (*playerRgb & k_White) : k_White;
	return (a << 24u) | rgb;
}

glm::vec2 volume_glow::UvOffset(glm::vec2 animSpeed, uint64_t milliseconds)
{
	const float seconds = static_cast<float>(milliseconds) * 0.001f;
	return {std::fmod(animSpeed.x * seconds, 1.0f), std::fmod(animSpeed.y * seconds, 1.0f)};
}

uint64_t volume_glow::GameMilliseconds(uint32_t turn, float turnFraction)
{
	constexpr uint64_t k_TurnMilliseconds = 100;
	constexpr float k_MostIntoATurn = 99.0f;
	const float into = std::clamp(std::floor(turnFraction * static_cast<float>(k_TurnMilliseconds)), 0.0f, k_MostIntoATurn);
	return static_cast<uint64_t>(turn) * k_TurnMilliseconds + static_cast<uint64_t>(into);
}

uint64_t volume_glow::SinceStarted(uint64_t now, uint64_t startedAt)
{
	return now > startedAt ? now - startedAt : 0;
}

glm::vec3 volume_glow::ViewNormal(glm::vec3 normal, const std::array<glm::vec3, 3>& axes, glm::vec2 projectionScale)
{
	const glm::vec3 widen {projectionScale, 1.0f};
	glm::vec3 out {0.0f};
	for (size_t i = 0; i < axes.size(); ++i)
	{
		const auto axis = widen * axes.at(i);
		const float length = glm::length(axis);
		out += length > 0.0f ? axis * (normal[static_cast<glm::length_t>(i)] / length) : glm::vec3(0.0f);
	}
	return out;
}

uint32_t volume_glow::EdgeLevel(glm::vec2 viewNormal, bool boned)
{
	const float sideOn = boned ? std::abs(viewNormal.x) + std::abs(viewNormal.y)
	                           : (viewNormal.x * viewNormal.x + viewNormal.y * viewNormal.y) / std::numbers::sqrt2_v<float>;
	const float level = (sideOn - k_EdgeStart) * (1.0f / (k_EdgeEnd - k_EdgeStart));
	if (!(level > 0.0f))
	{
		return 0;
	}
	const float scaled = std::nearbyint(level * static_cast<float>(k_ByteMax));
	return scaled >= static_cast<float>(k_ByteMax) ? k_ByteMax : static_cast<uint32_t>(scaled);
}

uint32_t volume_glow::InnerArgb(uint32_t argb, uint32_t level)
{
	if (level >= k_ByteMax)
	{
		return argb;
	}
	const auto channel = [argb, level](uint32_t shift) { return ((((argb >> shift) & 0xFFu) * level) >> 8u) << shift; };
	return (argb & 0xFF000000u) | channel(16) | channel(8) | channel(0);
}

glm::vec2 volume_glow::TextureCoordinates(float viewNormalY, glm::vec2 offset, glm::vec2 fraction, bool outer)
{
	return {viewNormalY * fraction.x + offset.x, outer ? offset.y - fraction.y : offset.y};
}

std::vector<uint32_t> volume_glow::ShellTriangles(std::span<const uint16_t> triangles)
{
	std::vector<uint32_t> out;
	out.reserve(triangles.size() / 3 * 18);
	for (size_t t = 0; t + 2 < triangles.size(); t += 3)
	{
		const uint32_t a = 2u * triangles[t];
		const uint32_t b = 2u * triangles[t + 1];
		const uint32_t c = 2u * triangles[t + 2];
		// Each edge in turn, two triangles between its corners on the model and pushed out
		out.insert(out.end(), {a, a + 1, b + 1, b + 1, b, a});
		out.insert(out.end(), {b, b + 1, c + 1, c + 1, c, b});
		out.insert(out.end(), {c, c + 1, a + 1, a + 1, a, c});
	}
	return out;
}
