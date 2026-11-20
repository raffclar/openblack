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

/// The waterfall of the third land, and the ark and the dinosaur skeleton of the fourth land. No land script puts them on
/// the land: the draw of the land makes them on the first frame that draws their land. The waterfall's water slides down
/// it, a ring of spray starts at its foot every 0.7 seconds of game time, and the sound of water that flows comes from its
/// place. The ark has the same sound. The ark and the dinosaur go onto the shape of the land, and the dinosaur makes a
/// print of its model on the land.
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

/// The land that has the ark and the dinosaur skeleton
inline constexpr int32_t k_SceneryLand = 4;

/// A model of the fourth land, which stands on the land at its place
struct Scenery
{
	/// The model, in the misc folder
	std::string_view modelFile;
	/// The place of the model along x and z. It stands on the land under the place.
	float x {0.0f};
	float z {0.0f};
	/// The turn of the model about the vertical, in radians, and its scale
	float turn {0.0f};
	float scale {1.0f};
};

/// The ark. It has the place of the waterfall among the models, and the same sound.
inline constexpr Scenery k_Ark {.modelFile = "arche.l3d", .x = 3538.0f, .z = 2129.0f, .turn = 8.9728f, .scale = 1.1f};
/// The dinosaur skeleton. It makes a print of its model on the land.
inline constexpr Scenery k_Dinosaur {.modelFile = "dinosaur.l3d", .x = 2690.0f, .z = 2590.0f, .turn = 1.57f, .scale = 1.0f};
/// The importance of the ark and the dinosaur. They never fade out with the distance.
inline constexpr float k_SceneryImportance = 10.0f;

/// The model that has the place of the waterfall among the models on land `land`, none on a land with no such model
[[nodiscard]] std::optional<std::string_view> ModelFileOf(int32_t land);

/// Whether the models are on the land, as the frames that draw the land find it
struct Presence
{
	/// The land that the last frame that drew the land found, none before the first such frame
	std::optional<int32_t> land;
	/// The waterfall or the ark is on the land
	bool model {false};
	/// The sound comes from the waterfall or the ark
	bool sound {false};
	/// The dinosaur is on the land
	bool dinosaur {false};
	/// The print of the dinosaur is on the land
	bool print {false};
};

/// What a frame that draws the land changes about the models
struct Changes
{
	bool removeModel {false};
	bool removeSound {false};
	bool makeModel {false};
	bool makeSound {false};
	bool removeDinosaur {false};
	bool makeDinosaur {false};
	/// The dinosaur that the frame makes has its print
	bool makePrint {false};
};

/// What the frames have of the models now. The frames lose the models as the land clears.
struct Shown
{
	/// The waterfall or the ark
	bool model {false};
	bool dinosaur {false};
};

/// A frame draws land `land`. A land with another number removes the models of the last land. The third land gets the
/// waterfall and its sound when it has no model. The fourth land gets the ark with its sound, and the dinosaur with its
/// print, when it has no such model. A model that the frames lost as the land cleared comes back with no sound and no
/// print.
[[nodiscard]] Changes Arrive(Presence& presence, int32_t land, Shown shown);

/// The land clears, and the game opens the land again: the sound goes with the things of the land, and the print goes
/// with the land. The models stay.
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
