/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/fwd.hpp>

namespace openblack
{

namespace graphics
{
class FrameBuffer;
class Mesh;
} // namespace graphics

class OceanInterface
{
public:
	virtual ~OceanInterface() = default;

	[[nodiscard]] virtual const graphics::FrameBuffer& GetReflectionFramebuffer() const noexcept = 0;
	/// The sea's screen rows need a render target of the screen's size: recreates it when the size changes
	virtual void ResizeReflectionFramebuffer(uint16_t width, uint16_t height) = 0;
	/// The level-0 world quad of +-70000
	[[nodiscard]] virtual graphics::Mesh& GetMesh() const noexcept = 0;
	/// A full-screen quad in clip space, for the screen rows of the other levels
	[[nodiscard]] virtual graphics::Mesh& GetScreenMesh() const noexcept = 0;
	[[nodiscard]] virtual entt::id_type GetDiffuseTexture() const noexcept = 0;
	[[nodiscard]] virtual entt::id_type GetAlphaTexture() const noexcept = 0;
};
} // namespace openblack
