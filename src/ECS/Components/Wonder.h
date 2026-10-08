/******************************************************************************
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

/// A wonder: an abode plus its power. One per wonder abode entity, assigned by ecs::wonders::Create (from
/// AbodeArchetype). ecs::wonders is its only writer.
struct Wonder
{
	/// Power: 0 when made; set at creation with the script's scale, or by a scaffold building it with the town's
	/// wonder power. Saved after the abode's ((not ported) saves)
	float power {0.0f};
};

} // namespace openblack::ecs::components
