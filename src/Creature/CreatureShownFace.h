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

/// Whether the shown face stays on the body at a turn pose. It stays while the world did not pull or relax a face since
/// the last pose. `worldAtLastPose` is the face of the world after that pose, and `worldNow` is its face now.
[[nodiscard]] inline bool StaysBeforePose(const creature_layers::FaceLayer& worldAtLastPose,
                                          const creature_layers::FaceLayer& worldNow)
{
	return worldNow == worldAtLastPose;
}

/// Whether the shown face is still necessary after a pose: the shown face and the face of the world are different
[[nodiscard]] inline bool StaysAfterPose(const creature_layers::FaceLayer& shown, const creature_layers::FaceLayer& world)
{
	return !(shown == world);
}

} // namespace openblack::creature_shown_face
