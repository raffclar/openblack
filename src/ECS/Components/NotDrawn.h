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

/// An entity whose model is not drawn although it keeps its Mesh (its size, map cells and type still come from it): a
/// building under construction at exactly 0 % (the building's draw draws nothing when its percent for drawing is 0).
/// RenderingSystem leaves it out of every model pass and of the static shadows (abodes::CastsShadowOnTexture), but not
/// out of the landscape footprints (footprintOnlyDrawDescs: the footprint stays on for an unbuilt building).
/// (pending) feature_build still hides by taking the Mesh away; it should use this tag
struct NotDrawn
{
};

} // namespace openblack::ecs::components
