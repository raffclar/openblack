/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <bgfx/bgfx.h>

namespace openblack::test
{
/// bgfx::shutdown when it goes. Made right after a successful bgfx::init, so a failed ASSERT cannot skip the shutdown;
/// made before the test's meshes and textures, so they go first
class BgfxShutdown
{
public:
	BgfxShutdown() = default;
	~BgfxShutdown() { bgfx::shutdown(); }
	BgfxShutdown(const BgfxShutdown&) = delete;
	BgfxShutdown& operator=(const BgfxShutdown&) = delete;
	BgfxShutdown(BgfxShutdown&&) = delete;
	BgfxShutdown& operator=(BgfxShutdown&&) = delete;
};
} // namespace openblack::test
