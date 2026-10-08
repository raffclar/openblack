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

/// An object's "needs sorting" flag, set by someone else than its mesh: the hand's press sets it on the held object
/// (the old value kept by the hand and given back when it throws the object). The whole object then goes to the
/// Z-sorter instead of being drawn at
/// once, as for a mesh with the flag 0x200. RenderingSystem gives such an entity its own instance range
/// (RenderContext::sortedOpaqueDrawDescs) and the renderer queues it in the main view
struct NeedsSorting
{
};

} // namespace openblack::ecs::components
