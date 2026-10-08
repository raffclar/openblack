/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "GestureBuffer.h"
#include "GestureTemplates.h"

// The ideal drawing of each gesture (Data\Symbols\PathSymbol<n>.cam) that the "recognised" sparkles settle on, and
// the arc-length paths they move along. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic::gestures
{
/// A polyline with its cumulative length (each point's xyz and the distance so far)
class Path
{
public:
	void Clear()
	{
		_points.clear();
		_distances.clear();
	}
	void Add(const glm::vec3& point);
	[[nodiscard]] size_t Size() const { return _points.size(); }
	[[nodiscard]] const glm::vec3& operator[](size_t i) const { return _points[i]; }
	[[nodiscard]] glm::vec3& operator[](size_t i) { return _points[i]; }
	/// The distances again (after the points moved)
	void Measure();
	/// The whole length
	[[nodiscard]] float Length() const { return _distances.empty() ? 0.0f : _distances.back(); }
	/// The point at t (0..1) of the length, linear between the points
	[[nodiscard]] glm::vec3 At(float t) const;

private:
	std::vector<glm::vec3> _points;
	std::vector<float> _distances;
};

/// Cached per gesture: PathSymbol<gesture>.cam, or CIRCLE's (4) when there is no
/// file. Its points (x + 100) / 200, (100 - z) / 200 in 0..1, resampled into as many points evenly along the length.
struct Shape
{
	std::vector<glm::vec3> points; ///< (x, 0, z)
	float length {0.0f};
	float aspect {1.0f}; ///< (maxX - minX) / (maxZ - minZ)
	float maxX {0.0f};
	float minX {0.0f};
	float maxZ {0.0f};
	float minZ {0.0f};
};
[[nodiscard]] const Shape& ShapeOf(Gesture gesture);
/// Parses a .cam file's points (u32 file size, u32 ?, u32 count, count x 24 bytes {x, y, z, 3 floats}); false if short
bool ParseShape(const std::vector<uint8_t>& bytes, Shape& shape);

/// The record of a recognised gesture handed to UR_GesturingRecognised
struct RecognisedGesture
{
	Path stroke; ///< The land points under the drawn stroke
	Path ideal;  ///< The gesture's shape laid on the land, resampled to as many points
	glm::vec3 handPosition {0.0f};
	bool fromInterface {true}; ///< Whether it has an interface status (a FakeGestureOnLandscape has none)
};
} // namespace openblack::magic::gestures
