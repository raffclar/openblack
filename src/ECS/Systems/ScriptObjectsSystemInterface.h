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

#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{

/// One taken place of the scripts' object table, as the debugging tools see it
struct ScriptObjectPlace
{
	uint16_t place {0};
	entt::entity object {entt::null};
	bool createdByScript {false};
	/// How many script references hold it
	uint8_t references {0};
};

/// The objects the scripts hold, the references they keep to them, and which of them a script controls
class ScriptObjectsSystemInterface
{
public:
	virtual ~ScriptObjectsSystemInterface() = default;

	/// An object a native made (or only found) for a script takes its place in the table; false when the table is full
	virtual bool Register(entt::entity object, bool createdByScript) = 0;
	/// Before each native runs: whether it takes control of the objects it is given
	virtual void EnterNative(uint32_t native) = 0;
	/// An object a native is given: a native that takes control takes control of it; none (null) when the object has gone
	/// or is no longer to be dealt with
	virtual entt::entity Fetch(entt::entity object) = 0;
	/// A script variable takes or lets go of an object
	virtual void AddReference(entt::entity object) = 0;
	virtual void RemoveReference(entt::entity object) = 0;
	/// A script lets go of its control of an object, which goes back into the game
	virtual void ReleaseFromScript(entt::entity object) = 0;
	/// What a script held as one object it now holds as another
	virtual void Replace(entt::entity from, entt::entity to) = 0;
	/// The scripts' program starts again: every object a script made goes, every other one it held is let go back into
	/// the game, and every place is free again
	virtual void Reset() = 0;
	/// After the scripts' turn: every place no script reference holds any more is freed. Its object, if still there, is no
	/// longer in a script and is let go: a flock or dance a script made goes, others leave it; a marker, a timer or a
	/// highlight a script made goes; anything else goes back into the game
	virtual void ReleaseUnreferenced() = 0;
	/// Every member leaves a script's flock or dance, letting go of the reference it kept; a member the script controls
	/// waits for the script. False when the object is no flock or dance
	virtual bool Disband(entt::entity container) = 0;
	/// The taken places, in order
	[[nodiscard]] virtual std::vector<ScriptObjectPlace> Places() const = 0;
	/// How many times an object found no free place since the scripts' program last started
	[[nodiscard]] virtual uint32_t TimesFull() const = 0;
	/// A new land: its objects have gone already, so nothing is let go back into the game; each place forgets its
	/// object but keeps its count of references
	virtual void ClearForNewLand() = 0;
};

} // namespace openblack::ecs::systems
