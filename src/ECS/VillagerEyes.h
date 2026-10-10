/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>
#include <cstdint>

#include <algorithm>
#include <array>
#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/HighDetailRules.h"

/// The eyes of the villagers a script draws in high detail, such as the opening's family: an eyeball and two lids for
/// each eye, fixed to the head of the detailed model, blinking now and then and looking a little from side to side
namespace openblack::ecs::villager_eyes
{

using high_detail_rules::Face;

enum class Side : uint8_t
{
	Right,
	Left,
};

/// The models each villager's eyes are made of. The one eyeball is drawn for both eyes.
enum class Piece : uint8_t
{
	RightUpperLid,
	RightLowerLid,
	LeftUpperLid,
	LeftLowerLid,
	Eyeball,
};
constexpr size_t k_PieceCount = 5;

/// A piece's file in the game's misc folder: the boy's eyes are a little larger than the man's and the woman's
[[nodiscard]] constexpr std::string_view PieceFile(Face face, Piece piece)
{
	constexpr std::array<std::string_view, k_PieceCount> k_Grown {
	    "Eyes/r_paupe_up.l3d", "Eyes/r_paupe_down.l3d", "Eyes/l_paupe_up.l3d", "Eyes/l_paupe_down.l3d", "Eyes/eye_ball.l3d",
	};
	constexpr std::array<std::string_view, k_PieceCount> k_Child {
	    "Eyes/r_paupe_up2.l3d",   "Eyes/r_paupe_down2.l3d", "Eyes/l_paupe_up2.l3d",
	    "Eyes/l_paupe_down2.l3d", "Eyes/eye_ball2.l3d",
	};
	return (face == Face::Boy ? k_Child : k_Grown).at(static_cast<size_t>(piece));
}

/// How long the lids take to close or open again, in milliseconds
constexpr int32_t k_LidMoveTime = 50;
/// How long the lids stay shut in a villager's first blink, in milliseconds
constexpr int32_t k_FirstClosedTime = 200;

/// Where a villager is in its blinking
struct Blink
{
	bool blinking {false};
	/// Milliseconds until the next blink, counted while it isn't blinking
	int32_t untilNext {0};
	/// Milliseconds into the blink
	int32_t into {0};
	/// Milliseconds the lids stay shut this blink
	int32_t closedFor {k_FirstClosedTime};
};

/// A villager's own eyes
struct Eyes
{
	Blink blink;
	/// How far both eyeballs are turned to the side, in radians
	float roll {0.0f};
};

/// What every high-detail villager's eyes share: the way they all turn towards, and how far every lid droops at
/// least, picked again every so often
struct Shared
{
	float rollTarget {0.0f};
	uint32_t droopTime {0};
	float droop {0.0f};
};

/// The most the eyeballs turn to either side, in radians
constexpr float k_MostRoll = 0.25f;
/// How fast they turn, in radians a millisecond, as its two factors
constexpr float k_RollSpeed = 0.7f;
constexpr float k_PerMillisecond = 0.001f;
/// How often the lids' least droop is picked again, in milliseconds counted by each villager drawn
constexpr uint32_t k_DroopTime = 300;
/// The most the lids droop at least, of closed
constexpr float k_MostDroop = 0.3f;
/// The share of closed the lids move by each millisecond of a blink's closing and opening
constexpr float k_LidMovePerMillisecond = 0.02f;

/// A draw of a random number between a and b, as the game draws them for its drawing
template <typename Random>
concept RandomRange = std::invocable<Random&, float, float>;

/// Moves a villager's eyes on by `elapsed` milliseconds of the game's clock while it is drawn: the blink and the turn
/// of its eyes. Gives how closed its lids are, 0 open to 1 shut.
template <RandomRange Random>
[[nodiscard]] float Step(Eyes& eyes, Shared& shared, uint32_t elapsed, Random&& random)
{
	auto& blink = eyes.blink;
	const auto passed = static_cast<int32_t>(elapsed);
	float closed = 0.0f;
	if (!blink.blinking)
	{
		blink.untilNext -= passed;
		if (blink.untilNext < 0)
		{
			// It starts closing on the next frame
			blink.blinking = true;
			blink.untilNext = static_cast<int32_t>(random(1000.0f, 5000.0f));
		}
	}
	else
	{
		const int32_t into = blink.into;
		const int32_t shut = blink.closedFor;
		if (into > shut + (2 * k_LidMoveTime))
		{
			// Open again: the next blink stays shut for another while
			blink.blinking = false;
			blink.closedFor = static_cast<int32_t>(random(100.0f, 200.0f));
			blink.into = 0;
		}
		else
		{
			if (into < k_LidMoveTime)
			{
				closed = static_cast<float>(into) * k_LidMovePerMillisecond;
			}
			else if (into < shut + k_LidMoveTime)
			{
				closed = 1.0f;
			}
			else
			{
				const float opening = static_cast<float>(into - shut - k_LidMoveTime) * k_LidMovePerMillisecond;
				closed = 1.0f - opening;
			}
			blink.into = into + passed;
		}
	}

	// Both eyeballs turn steadily towards where every villager's eyes turn, and the first to get there picks another way
	float step = static_cast<float>(passed) * k_RollSpeed;
	step = step * k_PerMillisecond;
	if (shared.rollTarget == eyes.roll)
	{
		shared.rollTarget = random(-k_MostRoll, k_MostRoll);
	}
	else if (eyes.roll <= shared.rollTarget)
	{
		eyes.roll = std::min(eyes.roll + step, shared.rollTarget);
	}
	else
	{
		eyes.roll = std::max(eyes.roll - step, shared.rollTarget);
	}

	// Every lid droops a little at least, by a share picked again every so often
	shared.droopTime += elapsed;
	if (shared.droopTime > k_DroopTime)
	{
		shared.droopTime = 0;
		shared.droop = random(0.0f, k_MostDroop);
	}
	return std::max(closed, shared.droop);
}

/// Where one eye is drawn this frame
struct DrawnEye
{
	glm::mat4 eyeball;
	glm::mat4 upperLid;
	glm::mat4 lowerLid;
	/// How its pieces are shaded by the sun, of 255, in place of the sun's light on each of their vertices: the
	/// villager's colour, as its body takes it from the land where it stands, times this over 256
	int32_t shade;
};

/// Where a villager's eyes are drawn this frame, right then left
struct DrawnEyes
{
	std::array<DrawnEye, 2> eyes;
	/// How closed the lids are, 0 open to 1 shut
	float closed;
	/// Where the villager stands, whose light and colour of the land the eyes take as its body does
	glm::vec3 standing;
};

/// An eye's frame in the world: its frame on the head's bone, turned about its own axes so that its pieces face out of
/// the head. `bone` is the bone's matrix in the world.
[[nodiscard]] glm::mat4 EyeFrame(const glm::mat4& bone, const glm::mat4& frame);

/// The eyeball's frame, turned to the side by `roll` about the eye's up axis
[[nodiscard]] glm::mat4 Rolled(const glm::mat4& eye, float roll);

/// A lid's frame, turned shut about the eye's side axis by `closed` (0 open to 1 shut): the upper lid comes down and
/// the lower lid up
[[nodiscard]] glm::mat4 Lid(const glm::mat4& eye, float closed, bool upper);

/// How an eye's pieces are shaded, of 255: by how the eye's face, a little off its front by angles of each face's own,
/// turns to the sun, from the shade of the dark side to whole. Every piece of the eye takes the one shade.
[[nodiscard]] int32_t Shade(Face face, Side side, const glm::mat4& eye);

/// How dark the side of an eye turned from the sun is, of 255
constexpr int32_t k_DarkSide = 90;

/// Where the eyeball's texture is read from for a face's eyes: each face has its own iris in the texture
[[nodiscard]] glm::vec2 IrisOffset(Face face);

/// The lids are drawn with the skin of the head, read from twice as far across its texture as they say
constexpr float k_LidTextureScale = 2.0f;

/// Where a villager's eyes are drawn this frame. `model` places the villager in the world, `bones` are the detailed
/// model's bones in its space as posed, and `frames` the eyes' frames on them, right then left.
[[nodiscard]] DrawnEyes Place(Face face, const glm::mat4& model, const std::array<glm::mat4, 2>& bones,
                              const std::array<glm::mat4, 2>& frames, float roll, float closed);

} // namespace openblack::ecs::villager_eyes
