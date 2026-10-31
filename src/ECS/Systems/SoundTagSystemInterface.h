/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The sounds objects keep up and that sound at points (components::SoundTag)
class SoundTagSystemInterface
{
public:
	virtual ~SoundTagSystemInterface() = default;
	/// Once a game turn: every switched on object's tag whose sound isn't playing starts it again, if the camera is
	/// within the sound's reach of it. A delayed point's sound plays once it has had time to reach the camera, if the
	/// camera is within its reach, and a point's tag goes once its sound has finished. An attached tag whose object
	/// is gone, or has no position, is removed as DetachObjectSound removes it.
	virtual void ProcessTurn(const glm::vec3& camera) = 0;
	/// Switches an object's tag on or off; off stops its sound at once
	virtual void SetActive(entt::entity entity, bool active) = 0;
	/// Sounds once at a point: at once, or, when delayed, once it has travelled to the camera at the speed of sound.
	/// Only this computer hears the sound, so it makes no entity of the world.
	virtual void CreatePointSound(entt::id_type sound, const glm::vec3& position, bool delayed) = 0;
	/// Attaches a sound to an object, on, from the object's position, and returns the new tag. The object keeps all
	/// the sounds it has: an object can keep up many sounds, also the same sound more than one time. When loops is
	/// false, each start plays the sound one time.
	virtual entt::entity AttachObjectSound(entt::entity entity, entt::id_type sound, bool loops) = 0;
	/// Removes all the tags of a sound from an object: the tags attached to it and its own tag. The tags of other
	/// sounds stay. A looping sound that plays continues to the end of its loop where it is, and does not start
	/// again. A sound that plays one time continues to its end.
	virtual void DetachObjectSound(entt::entity entity, entt::id_type sound) = 0;
	/// A saved game has loaded. The sounds of points and the sounds attached to objects do not come back. An object's
	/// own tag stays, with no sound playing, so that the next game turn starts its sound again. The load can attach a
	/// sound to an object again: it then gets the tag that the sound had before the load. The other tags of attached
	/// sounds go when the load calls ForgetUnclaimedSounds.
	virtual void ForgetSavedSounds() {}
	/// The tags of attached sounds that the load did not attach again go
	virtual void ForgetUnclaimedSounds() {}
};

} // namespace openblack::ecs::systems
