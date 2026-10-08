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
#include <vector>

// The recogniser's keypoint lists and the templates of Data\Gestures.jty. Wiki: docs/bw1-notes/magic.md, "Gestures:
// the buffer and the recogniser".

namespace openblack::magic::gestures
{
/// GESTURE_TYPE as the recogniser stores it (a byte). 22 and 23 are not in openblack's GestureType (Creature Isle), but
/// Gestures.jty has templates for them.
using Gesture = uint8_t;
constexpr Gesture k_None = 0;
constexpr Gesture k_Spiral = 1;
constexpr Gesture k_InverseSpiral = 2;
constexpr Gesture k_Circle = 4;
constexpr Gesture k_Scribble = 5;
constexpr Gesture k_RShape = 14;
constexpr size_t k_GestureCount = 24; ///< One entry per gesture in the interface's per-gesture tables

constexpr size_t k_MaxSamples = 80;

/// One keypoint (20 bytes in the file): the screen position (x, y unused, z = screen y down), the signed turn there
/// (rad, positive = clockwise on the screen) and the octant of the segment that leaves it (0 up, 2 right, 4 down, 6 left)
struct KeySample
{
	float x {0.0f};
	float y {0.0f};
	float z {0.0f};
	float turn {0.0f};
	uint32_t direction {0};
};

/// A template, or the keypoints of the live buffer
struct GestureData
{
	std::array<KeySample, k_MaxSamples> samples {};
	uint8_t count {0};
	Gesture gesture {k_None};
	uint8_t positionMode {0}; ///< 2 in every template
	float aspect {0.0f};
	bool checkDirection {false};
	bool allowReverse {false}; ///< (really "allow mirrored")
	bool checkAspect {false};

	/// Clears every field
	void SetToZero();
	/// The next keypoint (no bounds check in the original; 80 at most here)
	void Append(const KeySample& sample);
};

/// Reads Gestures.jty: u32 count, then count records of 0x65C bytes (80 samples, then u32 count, gesture, positionMode
/// (low bytes), checkDirection, allowReverse, checkAspect, f32 aspect). False if the data is short.
bool LoadTemplates(const std::vector<uint8_t>& bytes, std::vector<GestureData>& out);

/// The game's list, read once from Data\Gestures.jty through the resource cache; empty if missing
[[nodiscard]] const std::vector<GestureData>& Templates();
} // namespace openblack::magic::gestures
