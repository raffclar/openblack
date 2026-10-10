/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IntroRules.h"

#include <glm/vec4.hpp>

#include "Common/ModelInverseSquareRoot.h"

using namespace openblack::ecs;
namespace light = openblack::ecs::intro_rules::light;

float intro_rules::PutDownYaw()
{
	// Each step rounded to single precision as the game works it out
	const float degrees = 15.0f * std::bit_cast<float>(0x3C8EFA35u);
	return std::bit_cast<float>(0xBF490FDBu) - degrees;
}

int32_t intro_rules::AdvanceClip(int32_t time, uint32_t milliseconds, int32_t playTime, bool looping, bool& cameRound)
{
	const auto moved = static_cast<int32_t>(static_cast<uint32_t>(time) + milliseconds);
	int32_t result = moved;
	if (looping)
	{
		result = playTime > 0 ? moved % playTime : 0;
	}
	else if (moved > playTime - 1)
	{
		result = playTime - 1;
	}
	cameRound = result < time;
	return result;
}

namespace
{
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);
constexpr float k_SizeStep = std::bit_cast<float>(0x3D4CCCCDu);
constexpr float k_SizeBase = 3.0f;
/// A sprite is never drawn smaller than this
constexpr float k_SmallestSize = std::bit_cast<float>(0x38D1B717u);
constexpr float k_Flicker = 2.0f;
/// The streak starts this far behind the start
constexpr float k_StreakBack = -6.0f;
constexpr float k_StreakSize = 350.0f;
constexpr float k_StreakHeight = std::bit_cast<float>(0x3D4CCCCDu);
constexpr float k_StreakAngle = std::bit_cast<float>(0xBEC90FDBu);
constexpr float k_GlowSize = 80.0f;
constexpr uint32_t k_GlowArgb = 0x28FFFFFFu;
constexpr uint32_t k_StreakArgb = 0x0EFFFFFFu;
constexpr uint8_t k_FrontCell = 48;
constexpr uint8_t k_StreakCell = 49;
constexpr uint8_t k_TrailCell = 50;
/// The front sprites, then the trailing ones, then the glow and the streak
constexpr size_t k_FrontSprites = 6;
constexpr size_t k_Glow = 18;
constexpr size_t k_Streak = 19;
/// Only the first five flicker
constexpr size_t k_Flickering = 5;

float AtLeastSmallest(float size)
{
	return size < k_SmallestSize ? k_SmallestSize : size;
}

/// Sprite i's size: smaller further back, the trailing ones twice as large
float SizeOf(size_t i)
{
	const auto steps = static_cast<float>((19 - static_cast<int32_t>(i)) * 15);
	const float size = steps * k_SizeStep + k_SizeBase;
	return i < k_FrontSprites ? size : size + size;
}

glm::vec3 Along(const glm::vec3& from, const glm::vec3& way, float distance)
{
	return {way.x * distance + from.x, way.y * distance + from.y, way.z * distance + from.z};
}
} // namespace

light::Light light::Make(const glm::vec3& target, const glm::vec3& way, const Random& random)
{
	Light light;
	const float inverse =
	    gutils::ModelInverseSquareRoot((way.z * way.z + way.y * way.y) + way.x * way.x); // the game sums z, y, x
	light.way = {inverse * way.x, inverse * way.y, inverse * way.z};
	light.start = {target.x + light.way.x * k_Start, target.y + light.way.y * k_Start, target.z + light.way.z * k_Start};
	light.head = light.start;
	for (size_t i = 0; i < k_Sprites; ++i)
	{
		auto& sprite = light.sprites.at(i);
		sprite.position = Along(light.start, light.way, static_cast<float>(-static_cast<int32_t>(i)) * k_Spacing);
		sprite.cell = i < k_FrontSprites ? k_FrontCell : k_TrailCell;
		sprite.halfWidth = AtLeastSmallest(SizeOf(i));
		sprite.angle = random(0.0f, k_TwoPi);
		sprite.argb = (static_cast<uint32_t>((19 - static_cast<int32_t>(i)) * 255 / 20) << 24u) | 0xFFFFFFu;
	}
	// A soft glow at the head
	auto& glow = light.sprites.at(k_Glow);
	glow.argb = k_GlowArgb;
	glow.halfWidth = k_GlowSize;
	glow.position = light.start;
	// And a thin faint streak across it, tilted
	auto& streak = light.sprites.at(k_Streak);
	streak.position = Along(light.start, light.way, k_StreakBack);
	streak.angle = k_StreakAngle;
	streak.argb = k_StreakArgb;
	streak.cell = k_StreakCell;
	streak.halfWidth = k_StreakSize;
	streak.heightFactor = k_StreakHeight;
	return light;
}

glm::vec3 light::HeadPosition(Light& light)
{
	float distance = static_cast<float>(light.elapsed) * k_Speed;
	if (!(distance <= k_Arrives))
	{
		light.arrived = true;
		distance = k_Arrives;
	}
	return Along(light.start, light.way, distance);
}

void light::Step(Light& light, uint32_t milliseconds, const Random& random, Frame& out)
{
	out.drawn.clear();
	out.throughEverything = false;
	out.cameraAt.reset();
	if (light.state == State::Gone)
	{
		return;
	}
	if (light.state == State::Falling)
	{
		light.head = HeadPosition(light);
		if (light.arrived)
		{
			light.state = State::Holding;
			light.timer = k_HoldMs;
		}
		for (size_t i = 0; i < k_Sprites; ++i)
		{
			auto& sprite = light.sprites.at(i);
			if (i == k_Streak)
			{
				sprite.angle = static_cast<float>(static_cast<int32_t>(milliseconds)) * k_Spin + sprite.angle;
			}
			sprite.position = light.head;
			if (i >= k_Glow)
			{
				continue;
			}
			sprite.position = Along(light.head, light.way, static_cast<float>(-static_cast<int32_t>(i)) * k_Spacing);
			// While the game stands still the trail keeps its turns and sizes
			if (milliseconds == 0)
			{
				continue;
			}
			sprite.angle = random(0.0f, k_TwoPi);
			if (i >= k_Flickering)
			{
				continue;
			}
			const float flicker = random(-k_Flicker, k_Flicker);
			const auto steps = static_cast<float>((19 - static_cast<int32_t>(i)) * 15);
			sprite.halfWidth = AtLeastSmallest((steps * k_SizeStep + flicker) + k_SizeBase);
		}
		out.throughEverything = light.elapsed > k_ThroughEverythingAfterMs;
		out.drawn.assign(light.sprites.begin(), light.sprites.end());
		light.elapsed = static_cast<int32_t>(static_cast<uint32_t>(light.elapsed) + milliseconds);
		// The chasing camera, behind and above where the head now is
		auto camera = HeadPosition(light);
		camera.x -= light.way.x * k_CameraBehind;
		camera.z -= light.way.z * k_CameraBehind;
		camera.y = (camera.y - light.way.y * k_CameraBehind) + k_CameraAbove;
		out.cameraAt = camera;
		return;
	}
	auto& flash = light.sprites.front();
	if (light.state == State::Holding)
	{
		light.timer = static_cast<int32_t>(static_cast<uint32_t>(light.timer) - milliseconds);
		if (light.timer < 0)
		{
			light.state = State::Fading;
			light.timer = k_FadeMs;
		}
	}
	if (light.state == State::Fading)
	{
		const float share = static_cast<float>(light.timer) / static_cast<float>(k_FadeMs);
		const float alpha = share * static_cast<float>(0xFF - k_FadeTo) + static_cast<float>(k_FadeTo);
		flash.argb = (static_cast<uint32_t>(static_cast<int32_t>(alpha)) << 24u) | 0xFFFFFFu;
		light.timer = static_cast<int32_t>(static_cast<uint32_t>(light.timer) - milliseconds);
		if (light.timer < 0)
		{
			light.state = State::Gone;
		}
	}
	// The front sprite drawn three times through everything, turned anew between the draws
	out.throughEverything = true;
	out.drawn.push_back(flash);
	flash.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(flash);
	flash.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(flash);
}

std::optional<glm::vec3> intro_rules::GripPoint(int32_t clipTime, std::span<const glm::mat4> bones, uint32_t bone,
                                                const glm::vec3& point, const glm::mat4& model)
{
	if (!(static_cast<float>(clipTime) <= k_HoldsUntilMs) || bone >= bones.size())
	{
		return std::nullopt;
	}
	const auto world = model * bones[bone] * glm::vec4(point, 1.0f);
	return glm::vec3(world.x, world.y - k_HeldBelowGrip, world.z);
}
