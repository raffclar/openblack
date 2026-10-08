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

#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureTemplates.h"

// The live sample buffer (GestureSystem): the mouse samples and their keypoints, found online as the samples arrive.
// Wiki: docs/bw1-notes/magic.md (gestures).

namespace openblack::magic::gestures
{
/// A buffer sample
struct Sample
{
	float sx {0.0f};        ///< mouse x, pixels
	float sy {0.0f};        ///< (0)
	float sz {0.0f};        ///< mouse y, pixels (down)
	float turn {0.0f};      ///< the signed turn at this corner
	uint32_t direction {0}; ///< octant of the outgoing segment
	glm::vec3 world {0.0f}; ///< the land point under the cursor
	uint32_t flags {0};     ///< k_Start, k_Corner, k_Anchor, k_End or 0
	float heading {0.0f};   ///< Atan2Positive of the outgoing segment

	static constexpr uint32_t k_Start = 1;
	static constexpr uint32_t k_Corner = 2;
	static constexpr uint32_t k_Anchor = 4; ///< the newest sample after a corner was found
	static constexpr uint32_t k_End = 8;    ///< the newest sample
};

/// {minX, minZ, maxX, maxZ} in pixels
struct BoundingBox
{
	float minX {0.0f};
	float minZ {0.0f};
	float maxX {0.0f};
	float maxZ {0.0f};
};

/// pi/8 * 3/4: the smallest turn that makes a corner
constexpr float k_CornerTurn = 0.29452431f;

/// wrap(b - a): > pi -> -2pi, <= -pi -> +2pi
[[nodiscard]] float WrapDifference(float a, float b);
/// atan2(z, x) in [0, 2pi)
[[nodiscard]] float Atan2Positive(float x, float z);
/// to the nearest integer, an exact .5 goes down (truncate, +1 only above .5)
[[nodiscard]] int RoundHalfDown(float v);
/// the heading's octant, +pi/2 (0 = up on the screen), rounded with RoundHalfDown (an exact .5 rounds down)
[[nodiscard]] uint32_t Octant(float heading);

class GestureSystem
{
public:
	static constexpr uint8_t k_Size = 80;
	/// 70 samples on the same pixel wipe the buffer
	static constexpr uint32_t k_StationaryLimit = 70;

	/// a new sample at the head (mouse in pixels); then its keypoint work (ProcessNewSample)
	void AddSample(const glm::vec3& world, glm::ivec2 mouse);
	/// a sample that reuses the newest one's land point (the cursor is off the land); nothing when empty
	void AddSampleAtLastWorld(glm::ivec2 mouse);
	/// count = head = stationary = 0 and the samples zeroed
	void Clear();

	[[nodiscard]] uint8_t Count() const { return _count; }
	[[nodiscard]] uint8_t Head() const { return _head; }
	/// The physical slot of logical sample i (0 = the oldest): (head - count + i + 80) % 80, or 0 for i > count
	[[nodiscard]] uint8_t Physical(int i) const;
	[[nodiscard]] Sample& At(int i) { return _samples[Physical(i)]; }
	[[nodiscard]] const Sample& At(int i) const { return _samples[Physical(i)]; }

	/// the box of samples s..b-1 (from the first one; samples at (0,0,0) are skipped after it)
	[[nodiscard]] BoundingBox Box(int s, int b) const;
	/// the raw sample indices of keypoints number `start` and `end` (keypoints: flags & 0xB, or the last)
	void KeypointIndices(int start, int end, int& first, int& last) const;

private:
	/// flags the new sample and looks for a corner before it
	void ProcessNewSample(int i);
	/// the last j in [1, i-1] with flags, else 0
	[[nodiscard]] int PrevNonZero(int i) const;
	/// the last j in [1, i-1] that is a corner, else 0 (the start)
	[[nodiscard]] int PrevCorner(int i) const;
	/// the last j in [1, i-1] with flags, or with a non-zero position far from sample i, else 0
	[[nodiscard]] int PrevAnchor(int i) const;
	/// the sharpest corner between the previous keypoint k and sample i (or k itself), else 0
	[[nodiscard]] int FindCorner(int k, int i) const;
	/// 0 = keep c as a new corner, 1 = merged into the previous one (or the start)
	[[nodiscard]] int MergeOrReject(int c, int i);
	/// whether segment a->b of delta (dx, dz) is long enough
	[[nodiscard]] bool LongEnough(int a, int b, float dx, float dz) const;
	/// the outgoing heading, octant and turn of the corner before i
	void UpdateHeading(int i);
	/// wrap(Atan2P(c - b) - Atan2P(b - a)) on the screen positions
	[[nodiscard]] static float Turn(const Sample& a, const Sample& b, const Sample& c);
	/// 4 pixels or more apart on either axis
	[[nodiscard]] static bool Far(const Sample& a, const Sample& b);

	std::array<Sample, k_Size> _samples {};
	uint8_t _count {0};
	uint32_t _stationary {0};
	uint8_t _head {0};
};
} // namespace openblack::magic::gestures
