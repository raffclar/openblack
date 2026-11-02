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

#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

#include "3D/WaterRings.h"

/// The waterfall of the third land. No land script puts it on the land: the draw of the land makes it on the first
/// frame that draws the third land. Its water slides down it, a ring of spray starts at its foot every 0.7 seconds of
/// game time, and the sound of water that flows comes from its place.
namespace openblack::designed_waterfall
{

/// The land that has the waterfall
inline constexpr int32_t k_Land = 3;
/// The model, in the misc folder
inline constexpr std::string_view k_ModelFile = "waterfall3.l3d";
/// The place of the model, which stands at the height of the sea. Its sound comes from the same place.
inline constexpr glm::vec3 k_Position {3059.23f, 0.0f, 3145.33f};
/// The turn of the model about the vertical, in radians, and its scale
inline constexpr float k_Turn = 4.7f;
inline constexpr float k_Scale = 1.0f;
/// The importance of the model: its distance bands go twice as far as those of a thing with no importance
inline constexpr float k_Importance = 1.0f;
/// The interval between two rings of spray, in seconds of game time
inline constexpr float k_SprayInterval = 0.7f;

/// Whether the waterfall is on the land, as the frames that draw the land find it
struct Presence
{
	/// The land that the last frame that drew the land found, none before the first such frame
	std::optional<int32_t> land;
	/// The model is on the land
	bool model {false};
	/// The sound comes from the waterfall
	bool sound {false};
};

/// What a frame that draws the land changes about the waterfall
struct Changes
{
	bool removeModel {false};
	bool removeSound {false};
	bool makeModel {false};
	bool makeSound {false};
};

/// A frame draws land `land`. A land with another number removes the waterfall of the last land. The third land gets
/// the model and the sound when it has no model. `modelShown` is false when the frames lost the model as the land
/// cleared: the model then comes back with no sound.
[[nodiscard]] Changes Arrive(Presence& presence, int32_t land, bool modelShown);

/// The land clears: the sound goes with the things of the land, but the model stays
void LandCleared(Presence& presence);

/// The slide of the water's texture after `milliseconds` more of game time. The texture slides by half its height each
/// second. The slide stays between -1 and 0: it loses its whole part each step.
[[nodiscard]] float Slide(float slide, int32_t milliseconds);

/// The clock of the spray after `milliseconds` more of game time. True when a ring of spray starts: the clock is then
/// back at 0 and keeps no part of the time that went past the interval.
[[nodiscard]] bool StepSpray(float& clock, int32_t milliseconds);

/// The ring of spray at the waterfall's foot, in `landLight`, the land's brightest light of the moment, 0xRRGGBB, at half
/// its opacity. It grows to 30 metres across its middle and ages at 0.3 of the game's pace, so it lives 2.33 seconds.
[[nodiscard]] water_rings::Ring Spray(uint32_t landLight);

} // namespace openblack::designed_waterfall
