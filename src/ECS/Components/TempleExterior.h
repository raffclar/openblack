/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/TempleExteriorMorph.h"

namespace openblack::ecs::components
{

/// How a temple's outside looks: its player's alignment and share of influence, each from 0 to 1, where each is
/// heading, and what its mesh was last blended for. On the temple's heart from the moment the heart is made, built or
/// not (TempleExteriorSystemInterface)
struct TempleExterior
{
	TempleExteriorMorph::State look;
};

} // namespace openblack::ecs::components
