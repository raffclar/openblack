/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <numbers>
#include <span>
#include <string_view>
#include <vector>

#include "3D/FlatLand.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureLayers.h"
#include "TestbedScenarioRegistry.h"

// The shared values and helpers that the files of the testbed scenarios use to make their
// scenarios. Only the scenario files include this header.

namespace openblack::testbed_scenarios
{
using Kind = Command::Kind;
using creature_desires::Desire;
namespace animations = creature_layers::animations;

/// How far apart the creatures of a lineup stand
inline constexpr float k_RowSpacing = 32.0f;
/// The highest hour of the day
inline constexpr float k_HoursPerDay = 24.0f;
/// Offsets further from the middle of the map than this would be off it
inline constexpr float k_MaxOffset = 2400.0f;
/// A leash order can name a place off the map, as the player can, but no further than this from the middle
inline constexpr float k_MaxOrderOffset = 2.0f * k_MaxOffset;
/// A command that waits for its creature to be free first gives the last command this long to get it going
inline constexpr float k_SettleSeconds = 0.5f;
/// The last stage of growing up, and how many skills, miracles and deeds to copy the game's tables have
inline constexpr size_t k_LastPhase = 13;
inline constexpr size_t k_Skills = 6;
inline constexpr size_t k_Miracles = 42;
inline constexpr size_t k_Deeds = 46;
/// The game's spell seeds, and its last gesture (the square wave)
inline constexpr size_t k_SeedCount = 30;
inline constexpr size_t k_LastGesture = 23;
/// The seed of every benchmark's crowd, so that runs lay it out the same
inline constexpr uint32_t k_BenchmarkSeed = 2026;
/// The testbed's lake, from the middle of the map: the middle of its open water, half its width, and the width of the
/// shallows round it
inline constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
inline constexpr glm::vec2 k_LakeHalf = flat_land::k_LakeHalfExtent;
inline constexpr float k_ShallowsWidth = 20.0f;
/// The near and far shallows of the lake, as the testbed's camera looks north across it
inline constexpr float k_NearShallows = k_Lake.y - k_LakeHalf.y - (k_ShallowsWidth * 0.5f);
inline constexpr float k_FarShallows = k_Lake.y + k_LakeHalf.y + (k_ShallowsWidth * 0.5f);

/// The lake's open water, for the overview to keep in view
inline std::vector<glm::vec2> LakeInView()
{
	return {k_Lake - k_LakeHalf, k_Lake + k_LakeHalf};
}

inline Command Go(Kind kind, size_t creature, glm::vec2 point, float delay = 0.0f, bool wait = true)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .point = point};
}

inline Command Play(Kind kind, size_t creature, size_t animation, float delay, bool wait)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .value = animation};
}

inline Command Act(Kind kind, size_t creature, float delay = 0.0f, bool wait = false)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait};
}

inline Command AtHour(float hour, float delay)
{
	return {.kind = Kind::SetHour, .delaySeconds = delay, .hour = hour};
}

/// A command on one of the scenario's objects: picking it up, knocking it down, tying the leash to it
inline Command OnObject(Kind kind, size_t creature, size_t object, float delay = 0.0f, bool wait = true)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .object = object};
}

inline Command Stroke(size_t creature, creature_feedback::BodyPart part, float delay)
{
	return {.kind = Kind::HandStroke,
	        .creature = creature,
	        .delaySeconds = delay,
	        .waitUntilFree = true,
	        .bodyPart = static_cast<size_t>(part)};
}

inline Command Slap(size_t creature, float height, bool gentle, bool sweepsRight, float delay)
{
	return {.kind = Kind::HandSlap,
	        .creature = creature,
	        .delaySeconds = delay,
	        .waitUntilFree = true,
	        .slapHeight = height,
	        .gentle = gentle,
	        .sweepsRight = sweepsRight};
}

/// Pulling the face the creature's mind would for a feeling
inline Command Feel(size_t creature, creature_face::Cue cue, float delay)
{
	return {.kind = Kind::ShowFeeling, .creature = creature, .delaySeconds = delay, .value = static_cast<size_t>(cue)};
}

inline Command Leash(size_t creature, LeashType type, float delay)
{
	return {.kind = Kind::PutOnLeash, .creature = creature, .delaySeconds = delay, .leash = type};
}

/// A body wanting nothing: full, rested, watered, comfortable
inline NeedOverrides Content()
{
	return {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f};
}

/// Every desire gone but one, which is as strong as it gets
inline std::vector<DesireOverride> OnlyDesire(Desire wanted)
{
	std::vector<DesireOverride> overrides;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		overrides.push_back({.desire = desire, .fraction = desire == wanted ? 1.0f : 0.0f});
	}
	return overrides;
}

/// A creature kept content, so that its needs stay out of what the scenario shows
inline CreatureSetup Content(CreatureType species, glm::vec2 offset, float facing = 0.0f, std::string_view label = {})
{
	return {.label = label, .species = species, .offset = offset, .facingDegrees = facing, .needs = Content(), .hold = true};
}

/// A content creature whose mind is paused, so that it only does what the scenario tells it: its idle mind would
/// otherwise wander off or sit down in the middle of a walk
inline CreatureSetup Posed(CreatureType species, glm::vec2 offset, float facing = 0.0f, std::string_view label = {})
{
	auto creature = Content(species, offset, facing, label);
	creature.pauseMind = true;
	return creature;
}

/// How far east of the middle the item of a row stands, the first furthest west so that the testbed's camera,
/// looking north, sees the row in order from left to right
inline float RowX(size_t index, size_t count, float spacing = k_RowSpacing)
{
	return (static_cast<float>(index) - ((static_cast<float>(count) - 1.0f) * 0.5f)) * spacing;
}

/// Walking round the points in turn, each once it has arrived at the last
inline void WalkRound(std::vector<Command>& commands, size_t creature, std::span<const glm::vec2> points, bool run = false)
{
	for (const auto& point : points)
	{
		commands.push_back(Go(run ? Kind::RunTo : Kind::WalkTo, creature, point, 0.0f, true));
	}
}

inline std::vector<glm::vec2> Circle(glm::vec2 centre, float radius, size_t points)
{
	std::vector<glm::vec2> circle;
	for (size_t i = 0; i < points; ++i)
	{
		const auto angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(points);
		circle.emplace_back(centre + (radius * glm::vec2(portable_math::Cos(angle), portable_math::Sin(angle))));
	}
	return circle;
}

inline Command Fight(Kind kind, size_t creature, size_t value, float delay, float chargeMs = 0.0f)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .value = value, .chargeMs = chargeMs};
}
} // namespace openblack::testbed_scenarios
