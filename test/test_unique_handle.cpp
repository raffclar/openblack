/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <utility>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>

#include "Graphics/UniqueHandle.h"
#include "support/BgfxShutdown.h"

using openblack::graphics::UniqueHandle;

TEST(UniqueHandle, OwnsMovesAndResets)
{
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const openblack::test::BgfxShutdown bgfxShutdown;

	UniqueHandle<bgfx::TextureHandle> empty;
	EXPECT_FALSE(empty.IsValid());
	empty.Reset(); // nothing to destroy

	UniqueHandle<bgfx::TextureHandle> texture(bgfx::createTexture2D(4, 4, false, 1, bgfx::TextureFormat::R8));
	ASSERT_TRUE(texture.IsValid());
	const auto handle = texture.Get();

	// a move takes the handle and leaves the source empty
	UniqueHandle<bgfx::TextureHandle> moved(std::move(texture));
	EXPECT_FALSE(texture.IsValid()); // NOLINT(bugprone-use-after-move): the moved-from state is what is tested
	EXPECT_EQ(moved.Get().idx, handle.idx);

	// Reset with a new handle holds the new one; Reset() leaves it empty
	moved.Reset(bgfx::createTexture2D(2, 2, false, 1, bgfx::TextureFormat::R8));
	EXPECT_TRUE(moved.IsValid());
	moved.Reset();
	EXPECT_FALSE(moved.IsValid());

	UniqueHandle<bgfx::DynamicVertexBufferHandle> buffer;
	bgfx::VertexLayout layout;
	layout.begin().add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float).end();
	buffer.Reset(bgfx::createDynamicVertexBuffer(4, layout));
	EXPECT_TRUE(buffer.IsValid());
	buffer.Reset(); // before the shutdown, as the owners do
}
