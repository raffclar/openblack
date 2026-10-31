/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "3D/TempleToggles.h"
#include "ECS/Reflection/BinaryArchive.h"

namespace openblack::temple
{

/// The part of the save that tells about the rooms of the temple
struct RoomSave
{
	/// The main room opened the other rooms
	bool roomsOpened {false};
	/// What the map in the pool of the main room shows, one value for each button
	std::array<bool, TempleToggles::k_Count> shown {};
};

/// Writes the state of the rooms. The temple is the temple of this computer. Only its frames change this state, and no
/// game turn reads it. Thus all of it is a presentation value, and the tools that compare the synced state ignore it.
inline void SaveRooms(ecs::reflection::BinaryWriter& out, const RoomSave& rooms)
{
	out.BeginPresentation();
	ecs::reflection::Save(out, rooms.roomsOpened);
	for (const bool each : rooms.shown)
	{
		ecs::reflection::Save(out, each);
	}
	out.EndPresentation();
}

} // namespace openblack::temple
