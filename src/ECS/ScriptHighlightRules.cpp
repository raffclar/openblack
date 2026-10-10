/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHighlightRules.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::script_highlights
{

namespace
{
/// The beat's step, in radians a millisecond of a turn
constexpr float k_PulseSpeed = 5.0f;
constexpr float k_Milliseconds = 0.001f;
/// A whole turn, and its inverse, as the game's single precision constants
constexpr float k_TwoPi = 6.2831855f;
constexpr float k_InverseTwoPi = 0.159154937f;
constexpr float k_QuarterTurn = 1.5707964f;
/// Half a turn a second of the frames' game time, in radians a millisecond
constexpr float k_SpinPerMillisecond = 0.0031415927f;
/// The camera distances a scroll is drawn larger across, and the inverse of the furthest
constexpr uint32_t k_NearestDistance = 10;
constexpr uint32_t k_FurthestDistance = 30;
constexpr float k_InverseFurthest = 0.0333333351f;
/// The glow is twice the model's reach across, never quite none, and stands 1.4 of the reach before its middle
constexpr float k_GlowSizeOfReach = 2.0f;
constexpr float k_SmallestGlow = 0.0001f;
constexpr float k_GlowOutOfReach = 1.4f;
/// The beat's least share, and how much more it rises
constexpr float k_PulseLeast = 0.4f;
constexpr float k_PulseRise = 0.6f;
/// The silver beam's colour; the others' colour, and their alpha a whole beat
constexpr uint32_t k_SilverBeam = 0x14b4dcff;
constexpr uint32_t k_YellowBeam = 0x00ffff00;
constexpr uint32_t k_YellowBeamAlpha = 0x50;
/// The help system's events of a tap with and without a challenge or tip
constexpr uint32_t k_TapEventWithText = 0x22;
constexpr uint32_t k_TapEventWithoutText = 0x23;
} // namespace

void StepPulse(Pulse& pulse, float millisecondsPerTurn)
{
	pulse.phase += millisecondsPerTurn * k_PulseSpeed * k_Milliseconds;
	if (pulse.phase > k_TwoPi)
	{
		pulse.phase -= k_TwoPi;
	}
	pulse.previous = pulse.level;
	pulse.level = (1.0f - std::cos(pulse.phase)) * 0.5f;
}

float PulseShare(const Pulse& pulse, float turnFraction)
{
	const float blended = std::clamp(((pulse.level - pulse.previous) * turnFraction) + pulse.previous, 0.0f, 1.0f);
	return (blended * k_PulseRise) + k_PulseLeast;
}

uint32_t BeamColour(HighlightInfo kind, float pulseShare)
{
	if (kind == HighlightInfo::Silver)
	{
		return k_SilverBeam;
	}
	// The beat is truncated to a whole number before it is scaled, so the beam only shows at the top of the beat
	const auto alpha = static_cast<uint8_t>(static_cast<uint32_t>(static_cast<int32_t>(pulseShare)) * k_YellowBeamAlpha);
	return (static_cast<uint32_t>(alpha) << 24u) | k_YellowBeam;
}

float WrapAngle(float radians)
{
	const auto turns = static_cast<int32_t>(radians * k_InverseTwoPi);
	return radians - (static_cast<float>(turns) * k_TwoPi);
}

float Spin(float yAngle, float frameMilliseconds)
{
	return WrapAngle(yAngle + (frameMilliseconds * k_SpinPerMillisecond));
}

float FacingAngle(glm::vec3 camera, glm::vec3 at)
{
	return WrapAngle(std::atan2(camera.z - at.z, camera.x - at.x) + k_QuarterTurn);
}

float DrawnScale(HighlightInfo kind, float scale, float cameraDistance)
{
	if (IsTipSign(kind))
	{
		return scale;
	}
	const auto distance =
	    std::clamp(static_cast<uint32_t>(std::max(cameraDistance, 0.0f)), k_NearestDistance, k_FurthestDistance);
	return static_cast<float>(distance) * scale * k_InverseFurthest;
}

float GlowHalfSize(float radius)
{
	return std::max(k_GlowSizeOfReach * radius, k_SmallestGlow);
}

glm::vec3 GlowPosition(glm::vec3 centre, glm::vec3 camera, float radius)
{
	const auto towards = camera - centre;
	const float length = glm::length(towards);
	if (length <= 0.0f)
	{
		return centre;
	}
	return centre + (towards / length * radius * k_GlowOutOfReach);
}

uint8_t GlowAlpha(HighlightInfo kind)
{
	switch (kind)
	{
	case HighlightInfo::DidYouKnowSign:
		return 0x32;
	case HighlightInfo::Silver:
		return 0x96;
	case HighlightInfo::Gold:
		return 0x64;
	case HighlightInfo::Scroll:
		break;
	}
	return 0;
}

Sparks MakeSparks(const std::function<float(float)>& randomFloat, const std::function<uint32_t(uint32_t)>& random)
{
	constexpr int32_t k_StartApart = k_SparkLifeMilliseconds / static_cast<int32_t>(k_Sparks);
	Sparks made;
	for (size_t i = 0; i < k_Sparks; ++i)
	{
		made.sparks.at(i) = {.ageMilliseconds = -static_cast<int32_t>(i) * k_StartApart,
		                     .angle = randomFloat(glm::two_pi<float>())};
	}
	made.firstPicture = random(32);
	return made;
}

void StepSparks(Sparks& sparks, uint32_t gameMilliseconds, const std::function<float(float)>& randomFloat)
{
	for (auto& spark : sparks.sparks)
	{
		spark.ageMilliseconds += static_cast<int32_t>(gameMilliseconds);
		if (spark.ageMilliseconds > k_SparkLifeMilliseconds)
		{
			spark.ageMilliseconds -= k_SparkLifeMilliseconds;
			spark.angle = randomFloat(glm::two_pi<float>());
		}
	}
}

std::optional<SparkLook> LookOf(const Sparks& sparks, size_t index, glm::vec3 from)
{
	if (index >= k_Sparks || sparks.sparks.at(index).ageMilliseconds < 0)
	{
		return std::nullopt;
	}
	constexpr float k_Rise = 20.0f;
	constexpr float k_StartHalfSize = 2.0f;
	constexpr float k_EndHalfSize = 1.2f;
	constexpr float k_FadeInPerMillisecond = 0.0002f;
	constexpr float k_FadeOutPerMillisecond = 1.0f / 3000.0f;
	constexpr int32_t k_FadeOutFrom = 5000;
	constexpr float k_LifeShareSquared = 1.5625e-08f;
	constexpr int32_t k_PictureMilliseconds = 50;
	const auto& spark = sparks.sparks.at(index);
	const int32_t age = spark.ageMilliseconds;
	// Its share of its life, squared; the game works this through at more than a float's precision
	const double share = static_cast<double>(age * age) * static_cast<double>(k_LifeShareSquared);
	const double fade = age < k_FadeOutFrom
	                        ? static_cast<double>(age) * static_cast<double>(k_FadeInPerMillisecond)
	                        : 1.0 - static_cast<double>(age - k_FadeOutFrom) * static_cast<double>(k_FadeOutPerMillisecond);
	return SparkLook {
	    .position = {from.x, static_cast<float>(static_cast<double>(from.y) + static_cast<double>(k_Rise) * share), from.z},
	    .halfSize = static_cast<float>(static_cast<double>(k_EndHalfSize - k_StartHalfSize) * share +
	                                   static_cast<double>(k_StartHalfSize)),
	    .alpha = static_cast<uint8_t>(static_cast<int32_t>(255.0 * fade)),
	    .picture =
	        (static_cast<uint32_t>(age / k_PictureMilliseconds) + static_cast<uint32_t>(index) + sparks.firstPicture) & 31u,
	    .angle = spark.angle,
	};
}

TapOutcome Tap(HighlightInfo kind, uint32_t scriptId, bool byThisPlayer)
{
	TapOutcome outcome;
	if (byThisPlayer)
	{
		outcome.helpEvent = scriptId != 0 ? k_TapEventWithText : k_TapEventWithoutText;
	}
	if (scriptId == 0)
	{
		return outcome;
	}
	outcome.starts = true;
	if (IsTipSign(kind))
	{
		outcome.signSound = byThisPlayer;
		outcome.showsTip = true;
	}
	else
	{
		outcome.replaysChallenge = true;
	}
	return outcome;
}

bool TipsRead::Has(uint32_t text, uint32_t category) const
{
	return category < k_Categories && std::ranges::find(_texts.at(category), text) != _texts.at(category).end();
}

void TipsRead::Add(uint32_t text, uint32_t category)
{
	if (category >= k_Categories)
	{
		return;
	}
	auto& texts = _texts.at(category);
	if (texts.size() >= k_MostInCategory || std::ranges::find(texts, text) != texts.end())
	{
		return;
	}
	texts.push_back(text);
}

bool TipsRead::Empty() const
{
	return std::ranges::all_of(_texts, [](const auto& texts) { return texts.empty(); });
}

void TipsRead::Clear()
{
	for (auto& texts : _texts)
	{
		texts.clear();
	}
}

float HeightOnThings(glm::vec2 inCell, float radius, std::span<const ThingBelow> things)
{
	float highest = 0.0f;
	for (const auto& thing : things)
	{
		if (thing.livingOrMoving || thing.top <= highest)
		{
			continue;
		}
		const auto apart = inCell - thing.inCell;
		if (glm::dot(apart, apart) < (thing.radius * thing.radius) + (radius * radius))
		{
			highest = thing.top;
		}
	}
	return highest;
}

glm::vec2 InCell(glm::vec2 point)
{
	constexpr float k_FixedToMetres = map_coords::k_CellSize / static_cast<float>(map_coords::k_FixedPerCell);
	const auto within = [](float metres) {
		return static_cast<float>(static_cast<uint32_t>(map_coords::ToFixed(metres)) & 0xffffu) * k_FixedToMetres;
	};
	return {within(point.x), within(point.y)};
}

} // namespace openblack::ecs::script_highlights
