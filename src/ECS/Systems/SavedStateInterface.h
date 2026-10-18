/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::reflection
{
class BinaryWriter;
class BinaryReader;
} // namespace openblack::ecs::reflection

namespace openblack::ecs::systems
{

/// A system that keeps running state of its own beside the entities, such as the order in which it works through
/// its entities. The whole-world save keeps this state in a section with the name of the system (deviation 83).
///
/// The default keeps nothing, so that a fake of a system interface needs no change.
class SavedStateInterface
{
public:
	virtual ~SavedStateInterface() = default;

	/// Writes the running state of the system
	virtual void SaveState(reflection::BinaryWriter& /*out*/) const {}

	/// Reads the running state that SaveState wrote. The load calls it after the entities are back and the land has
	/// started again. Returns whether the state read.
	virtual bool LoadState(reflection::BinaryReader& /*in*/) { return true; }
};

} // namespace openblack::ecs::systems
