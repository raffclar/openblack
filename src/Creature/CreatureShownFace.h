/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureFace.h"
#include "Creature/CreatureLayers.h"

/// The face that a stroke of this computer's hand pulls. Only the computer of the hand shows it, as in the game: the
/// world does not have it.
namespace openblack::creature_shown_face
{

/// The shown face after a stroke pulls a face. It starts from the face that the body shows now.
[[nodiscard]] inline creature_layers::FaceLayer Pull(const creature_layers::FaceLayer& shown,
                                                     const creature_face::Request& face)
{
	return creature_layers::PullFace(shown, creature_face::AnimationOf(face.face), face.milliseconds, face.cue);
}

/// The face that the body shows: the shown face of this computer if there is one, else the face of the world
[[nodiscard]] inline const creature_layers::FaceLayer& Shown(const creature_layers::FaceLayer& world,
                                                             const creature_layers::FaceLayer* shown)
{
	return shown != nullptr ? *shown : world;
}

/// The shown face before a pose. A change to the face of the world also changes the shown face in the same way, and
/// then sets `expectedWorld` to the new face of the world. If the face of the world is not `expectedWorld`, the world
/// changed its face in a different way: the shown face becomes the face of the world, as the one face slot of the game
/// does.
[[nodiscard]] inline creature_layers::FaceLayer AfterWorldWrites(const creature_layers::FaceLayer& shown,
                                                                 const creature_layers::FaceLayer& expectedWorld,
                                                                 const creature_layers::FaceLayer& world)
{
	return world == expectedWorld ? shown : world;
}

/// Whether the shown face is still necessary after a pose: the shown face and the face of the world are different
[[nodiscard]] inline bool StaysAfterPose(const creature_layers::FaceLayer& shown, const creature_layers::FaceLayer& world)
{
	return !(shown == world);
}

} // namespace openblack::creature_shown_face
