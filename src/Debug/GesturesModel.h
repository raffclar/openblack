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

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Magic/Gestures/GestureTemplates.h"

// What the Gestures window works out: the gestures' names, what a key point of the hand's path is, what the interface
// is looking for, which gestures the path matches, and a template drawn back as a stroke of mouse positions. Free of
// the game's state, so it is tested with made-up templates and paths.

namespace openblack::debug::gestures_window
{

/// The gesture's name as the scripts write it, or "?" past the last one
[[nodiscard]] std::string_view GestureName(magic::gestures::Gesture gesture);

/// What a sample of the hand's path is, from its flags
enum class KeyPoint : uint8_t
{
	None,
	Start,
	Corner,
	Anchor,
	End,
};
[[nodiscard]] KeyPoint KeyPointOf(uint32_t flags);

/// What the interface looks for a gesture for, from its entry in the interface's table; empty when it looks for none
[[nodiscard]] std::string LookingForName(int type);

/// A gesture the path matches now, and whether mirrored
struct Match
{
	magic::gestures::Gesture gesture;
	bool mirrored;
	uint8_t templateIndex;
};
/// Every gesture whose templates match the path's key points, in gesture order
[[nodiscard]] std::vector<Match> MatchesNow(const std::vector<magic::gestures::GestureData>& templates,
                                            const magic::gestures::GestureData& keyPoints, float screenRatio);

/// The first template of the gesture, or nullptr
[[nodiscard]] const magic::gestures::GestureData* FirstTemplate(std::span<const magic::gestures::GestureData> templates,
                                                                magic::gestures::Gesture gesture);

/// The template's key points on the screen, `size` pixels wide around `centre`; the template's heights were stretched
/// by the screen's width over its height when it was made, so they are shrunk back by `screenRatio`
[[nodiscard]] std::vector<glm::vec2> TemplateStroke(const magic::gestures::GestureData& gestureTemplate, float size,
                                                    glm::vec2 centre, float screenRatio);

/// A polyline as the mouse positions of a stroke: about fifty, at least five pixels apart, from its first point to its
/// last
[[nodiscard]] std::vector<glm::ivec2> MousePositions(std::span<const glm::vec2> points);

} // namespace openblack::debug::gestures_window
