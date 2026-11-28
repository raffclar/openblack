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

/// The thing casts no shadow into the land, also when its kind casts one
struct NoLandShadow
{
};

/// The model of the thing casts no shadow into the land. The game keeps this on the model, which it makes again after a
/// load. Thus a save does not keep it, and the thing casts again after a load. The beach temple puzzle puts this on its
/// three smaller rings when it starts.
struct ModelCastsNoLandShadow
{
};

} // namespace openblack::ecs::components
