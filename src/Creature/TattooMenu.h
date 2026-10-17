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

#include <functional>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMorph.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct PhysiqueFileData;
}

/// The tattoo editor that the main menu opens from the Players page with its Edit Tattoo button. It shows a creature of
/// its own, which is not in the world: the body that the player's profile keeps in its physique file, or a stand-in
/// Giant Ape when there is no such file. The creature stands at the world's origin, at the size of 1, and is drawn over
/// the menu's dialog with a depth of its own, seen by the editor's own camera.
namespace openblack::tattoo_menu
{

/// The creature's size, whatever the size kept in the file
constexpr float k_Size = 1.0f;

/// The creature looks at the point under the pointer this far into the screen, along the camera's view. In the cave,
/// the creature looks at a point that is nearer (see cave_creature::k_PointerDepth).
constexpr float k_PointerDepth = 10.0f;

/// The creature that the editor shows
struct Shown
{
	CreatureType species {CreatureType::GiantApe};
	creature_morph::Morph morph;
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;

	bool operator==(const Shown&) const = default;
};

/// The stand-in: a Giant Ape half fat and half strong, neither evil nor good
[[nodiscard]] Shown StandIn();

/// How strong each species is of itself, which counts towards how strong its body looks
using SpeciesStrength = std::function<float(CreatureType)>;

/// The creature from the profile's physique file: its species, its shape from its alignment, fatness and strength,
/// and the marks on its skin. Without a file, or with a species that is not one of the game's, the stand-in.
[[nodiscard]] Shown FromPhysique(const std::optional<creaturemind::PhysiqueFileData>& physique,
                                 const SpeciesStrength& speciesStrength);

/// The Edit Tattoo button shows only while the profile has a physique file, the scripts let the temple show the
/// creature, and leaving the Players page does not start the game again
[[nodiscard]] bool ButtonShown(bool physiqueKept, bool creatureShown, bool restartPending);

/// The point that the creature looks at: under a pixel of the dialog's view, k_PointerDepth along the ray from the eye
/// through that pixel's point on the near plane. The view and projection are the editor's camera, and the screen is the
/// size of the view in pixels.
[[nodiscard]] std::optional<glm::vec3> PointerPoint(const glm::mat4& view, const glm::mat4& projection, glm::vec2 screen,
                                                    glm::vec2 pixel);

/// What is drawn of the creature, in the order that it is drawn. The body comes with its eyes.
enum class Layer : uint8_t
{
	Body,
	Hair,
};

/// The layers to draw, in order: the body first, and its hair after it, so that the hair blends over the body. The hair
/// is drawn only where the creature has hair of its own and the hair is shown.
[[nodiscard]] std::vector<Layer> DrawOrder(bool hasHair, bool hairShown);

} // namespace openblack::tattoo_menu
