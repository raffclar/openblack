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

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A sound that an object keeps up, or that sounds one time at a point (see SoundTagSystemInterface).
///
/// An object can keep up many sounds at the same time. Each sound has its own tag and its own voice, thus two tags
/// do not replace each other. A tag that an object owns from its start is on the object. A tag that a script or the
/// game attaches later is an entity of its own, which names the object in owner.
///
/// An object's tag repeats while it is on: when its sound does not play and the camera is in the sound's range, the
/// tag starts it again each game turn. When the tag goes off, its sound stops immediately. A point's tag sounds one
/// time. If it has a delay, it waits until the sound can travel to the camera. It goes when its sound is finished.
struct SoundTag
{
	entt::id_type sound;
	/// The distance above the object's position where it sounds from
	glm::vec3 offset;
	bool active;
	/// The emitter that plays it, if one plays it
	entt::entity emitter {entt::null};
	/// True for a point's sound, false for an object's sound
	bool point {false};
	/// True while a point's sound travels to the camera
	bool delayed {false};
	/// Game turns since the tag was made
	uint16_t turns {0};
	/// The object an attached tag sounds from. Null for a point or for a tag on its object.
	entt::entity owner {entt::null};
	/// True when the sound loops until it is released. False when each start plays the sound one time, and the next
	/// turn starts it again when it is finished.
	bool loops {true};
};

} // namespace openblack::ecs::components
