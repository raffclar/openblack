/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// An object with its own shadow holder updated every frame, outside the physics list: the launched boat (MissionaryBoat
/// mode 0) and the SuperVillagers' temporary shadow (updated by the landscape draw: the sun, the land only).
/// graphics::shadow_list makes its entry.
struct DynamicShadow
{
	/// it falls on objects too (the boat's does)
	bool onObjects {false};
	/// lit by the fixed sun instead of from 15000 above (the boat's is)
	bool useSun {false};
};

} // namespace openblack::ecs::components
