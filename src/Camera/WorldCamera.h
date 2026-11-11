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

#include <glm/vec3.hpp>

namespace openblack
{
class Camera;
class CameraModel;
class DefaultWorldCameraModel;
} // namespace openblack

/// The world's camera. While the player is in the temple, the temple has its own camera, and the world's camera continues
/// behind it: a flight goes on, and a mouse button of a camera action acts on it.
namespace openblack::world_camera
{

/// The model that the world's camera has as the player comes out of the temple
enum class Model : uint8_t
{
	/// The player's own camera of the world
	World,
	/// Another model, for example the camera that follows the creature
	Other,
};

/// A place for the camera: where it is and where it looks
struct Place
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// What the world's camera does as the player comes out of the temple
struct Leave
{
	/// The camera goes to this place at once, with no movement
	std::optional<Place> cutTo;
	/// A new camera of the player's own takes over at the place
	bool newWorldModel {false};
	/// The player's camera starts again from where it is
	bool startAgain {false};
};

/// Coming out of the temple. With a place picked on the map, the camera goes to the place at once, and the player's own
/// camera starts again there, or a new one takes over from another model. With no place picked, only the player's own
/// camera starts again; another model, as the camera that follows the creature, continues.
[[nodiscard]] constexpr Leave OnLeave(std::optional<Place> picked, Model model)
{
	return {.cutTo = picked, .newWorldModel = picked.has_value() && model == Model::Other, .startAgain = model == Model::World};
}

/// Whether the player's camera of the world takes no controls of the player in the next frame. It takes none in the
/// temple and none while a box of the front end shows. The frame that goes in or that opens the box still takes them.
/// The frame after the box goes takes them again.
[[nodiscard]] constexpr bool InputsOff(bool inTemple, bool frontEndBox)
{
	return inTemple || frontEndBox;
}

/// The player's own camera of the world in a model: the model itself, or the player's camera under the fight view. None
/// for another model, for example the camera that follows the creature.
[[nodiscard]] DefaultWorldCameraModel* PlayersWorldModel(CameraModel& model);

/// The camera that the world's camera model moves: the temple's camera of the world while the player is inside, else the
/// player's camera
[[nodiscard]] Camera& Get();
/// The camera of the world behind the temple, none while the player is outside
[[nodiscard]] Camera* BehindTemple();
/// At the end of the frame, after the camera keys: the player's camera of the world finds if it takes the controls of
/// the player in the next frame (see InputsOff). While a miracle's camera path has the camera, the player's camera keeps
/// its state.
void EndFrameControls(bool frontEndBox);

} // namespace openblack::world_camera
