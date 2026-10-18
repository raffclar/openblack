/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// EXPERIMENT (not for merge): the foam that builds up on the experiment's water and decays. The original game has no
// such foam.

#include <cstdint>

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics::underwater
{

/// The foam field is a square of texels on the water round the place that the camera looks at. Each texel keeps how
/// thick the foam is, how old it is and how much air it holds.
inline constexpr uint16_t k_FoamTexels = 512;
/// The size of one texel of the foam field on the water, in units
inline constexpr float k_FoamTexelSize = 1.5f;
/// The side of the foam field on the water, in units
inline constexpr float k_FoamWindow = static_cast<float>(k_FoamTexels) * k_FoamTexelSize;
/// The foam field moves on at most 60 times a second
inline constexpr float k_FoamStepSeconds = 1.0f / 60.0f;
/// The longest time that one step of the foam field covers. After a longer pause the foam field moves on by this much.
inline constexpr float k_FoamLongestStep = 0.1f;
/// The farthest the middle of the foam field goes from the point under the eye, in units
inline constexpr float k_FoamFarthestFocus = 250.0f;

/// The middle of the foam field for an eye that looks along `forward`: the point where the view meets the sea's
/// level, at most k_FoamFarthestFocus from the point under the eye. When the view does not go down to the water, it
/// is the point under the eye.
[[nodiscard]] glm::vec2 FoamFocus(glm::vec3 eye, glm::vec3 forward, float seaLevel);

/// The corner of the foam field with the lowest x and z, for its middle. The corner is always on a whole texel, so the
/// foam does not shimmer as the field moves.
[[nodiscard]] glm::vec2 FoamWindowOrigin(glm::vec2 focus);

/// One step of the foam field
struct FoamStep
{
	/// The time that the step covers, in seconds
	float seconds {0.0f};
	/// The corner of the field after the step, and before it
	glm::vec2 origin {0.0f};
	glm::vec2 previousOrigin {0.0f};
	/// True when the step starts the field again with no foam: the first step, a new land, or a jump of the camera
	bool clear {false};
	/// The target that the step writes, 0 or 1. The step reads the other one.
	uint8_t target {0};
};

/// When the foam field moves on, and where it is. It moves on at most k_FoamStepSeconds apart, and not at all while
/// the game's time stops.
class FoamClock
{
public:
	/// The step for this frame, or nothing when the field does not move on this frame. `seconds` is the water's time,
	/// `focus` the middle of the field (see FoamFocus) and `revision` the sea bed's revision.
	[[nodiscard]] std::optional<FoamStep> Advance(float seconds, glm::vec2 focus, uint32_t revision);
	/// The target that the last step wrote, and the corner of the field then
	[[nodiscard]] uint8_t Latest() const { return _target; }
	[[nodiscard]] glm::vec2 Origin() const { return _origin; }
	/// Whether a step has written the field yet
	[[nodiscard]] bool Ready() const { return _revision.has_value(); }

private:
	std::optional<float> _last;
	float _waiting {0.0f};
	glm::vec2 _origin {0.0f};
	std::optional<uint32_t> _revision;
	uint8_t _target {0};
};

/// How much of its first height the shore foam keeps (the foam where the waves break and run up the beach), 1 for the
/// full height. The foam of the crests, the white horses and the cliffs keeps its full height. Change it together with
/// UW_SHORE_FOAM_HEIGHT_SCALE in underwater_foam_shade.sh.
inline constexpr float k_ShoreFoamHeightScale = 2.0f / 3.0f;

/// The height in units that foam stands on the water for a thickness of 1, before any scale
inline constexpr float k_FoamFullHeight = 0.45f;

/// How high in units foam stands on the water where it has this thickness (0 or more). `shoreShare` is the part of the
/// foam that is shore foam, 0 to 1.
[[nodiscard]] constexpr float FoamStandHeight(float thickness, float shoreShare)
{
	const float share = shoreShare < 0.0f ? 0.0f : (shoreShare > 1.0f ? 1.0f : shoreShare);
	const float scale = 1.0f + (k_ShoreFoamHeightScale - 1.0f) * share;
	return (thickness > 0.0f ? thickness : 0.0f) * k_FoamFullHeight * scale;
}

/// The run of the waves up the beach in a calm sea, in height (see WaveState)
inline constexpr float k_CalmSwash = 0.25f;
/// The most that the waves stir the sea bed
inline constexpr float k_MostStir = 2.0f;

/// How strongly the waves stir the silt off the sea bed where they break, 0 to k_MostStir: 0.6 in a calm sea, more
/// with a longer run up the beach and in a storm. `swash` is the run of the waves up the beach, `storm` 0 to 1.
[[nodiscard]] float SedimentStir(float swash, float storm);

} // namespace openblack::graphics::underwater
