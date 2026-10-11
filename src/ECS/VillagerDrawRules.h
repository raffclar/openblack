/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <numbers>
#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

/// How a villager is drawn between the game's turns: the game moves and turns it once a turn, and its drawing glides
/// after it every frame
namespace openblack::ecs::villager_draw
{

/// A villager walking with a clip that carries it along is drawn between where it stood when the last turn began and
/// where it stands now, by how far the game is through the turn after it. Any other is drawn where it stands.
[[nodiscard]] glm::vec3 DrawnPosition(const glm::vec3& turnStart, const glm::vec3& now, float turnFraction, bool glides);

/// Whether a villager's drawing glides between turns: in a state that moves it, with a clip that plays by the ground
/// it covers rather than by the clock
[[nodiscard]] constexpr bool Glides(bool movingState, bool clipPlayedByTime)
{
	return movingState && !clipPlayedByTime;
}

/// An angle brought back within a half turn either way, by one whole turn at most
[[nodiscard]] float Wrapped(float angle);

/// A villager's drawn heading turns towards the way it faces at 3 radians a second, quicker for a turn of more than a
/// quarter (up to four times as quick for a half turn), by the game's milliseconds since the last frame; within that
/// step it faces the way it does at once. Angles are the game's, round the vertical.
[[nodiscard]] float EasedHeading(float drawn, float facing, uint32_t gameMilliseconds);

/// A villager drawn in high detail turns its drawn body once more after its drawn heading, at 3.927 radians a second,
/// unless a script told it to turn at once
[[nodiscard]] float EasedDetailedHeading(float drawn, float heading, uint32_t gameMilliseconds, bool turnAtOnce);

/// A frame's drawn headings: the one turned after the way the villager faces, the high-detail body's after that, and
/// the one it is drawn with
struct DrawnHeadings
{
	float eased;
	std::optional<float> detailed;
	float drawn;
};

/// The drawn headings a frame on, from the last frame's (none at first: they start facing its way)
[[nodiscard]] DrawnHeadings StepHeadings(std::optional<float> eased, std::optional<float> detailed, float facing,
                                         uint32_t gameMilliseconds, bool highDetail, bool turnAtOnce);

/// The heading of a model turned only round the vertical, as a villager's way of facing is drawn; none for one tipped
/// over (as in the physics)
[[nodiscard]] std::optional<float> HeadingOf(const glm::mat3& rotation);

/// The model's turn for a heading, the inverse of HeadingOf
[[nodiscard]] glm::mat3 RotationOf(float heading);

/// The milliseconds over which a villager drawn in high detail changes from one clip to the next
inline constexpr int32_t k_ClipBlendMilliseconds = 300;

/// A high-detail villager's change from the clip it played to the one it plays, held where the old clip was
struct ClipBlend
{
	AnimId from {AnimId::Invalid};
	uint32_t fromPlace {0};
	/// The milliseconds left; none left ends the change
	int32_t remaining {0};
	/// How much of the old clip shows, from 1 down
	float weight {0.0f};
};

/// What a high-detail villager's drawing remembers of its clips from frame to frame
struct ClipBlendTrack
{
	AnimId lastClip {AnimId::Invalid};
	uint32_t lastPlace {0};
	ClipBlend blend;
};

/// A frame of a high-detail villager's drawing: a new clip starts a change from the last one, held where it was last
/// drawn, all of the old clip showing at first; each frame on the same clip the old one shows less, by the game's
/// milliseconds, until none is left
void StepClipBlend(ClipBlendTrack& track, AnimId clip, uint32_t place, uint32_t gameMilliseconds);

/// Whether the change between clips is shown this frame: while time is left of it, unless a script told the villager
/// to turn and change at once
[[nodiscard]] constexpr bool ShowsClipBlend(const ClipBlend& blend, bool turnAtOnce)
{
	return blend.remaining != 0 && !turnAtOnce && blend.from != AnimId::Invalid;
}

/// The pose of a villager changing clip: the new clip's bones with the old clip's mixed in by the old one's weight
void BlendPoses(std::span<glm::mat4> bones, std::span<const glm::mat4> old, float oldWeight);

} // namespace openblack::ecs::villager_draw
