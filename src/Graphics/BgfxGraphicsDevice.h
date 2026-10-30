/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "GraphicsDeviceInterface.h"

namespace openblack::graphics
{

/// The graphics device of the game, on the graphics library
class BgfxGraphicsDevice final: public GraphicsDeviceInterface
{
public:
	[[nodiscard]] const bgfx::Memory* Copy(const void* data, uint32_t size) const override;
	[[nodiscard]] const bgfx::Memory* Alloc(uint32_t size) const override;
	[[nodiscard]] const bgfx::Memory* Reference(const void* data, uint32_t size) const override;
	[[nodiscard]] const bgfx::Caps* GetCaps() const override;
	void Frame() const override;

	[[nodiscard]] TextureHandle CreateTexture2D(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format,
	                                            uint64_t flags, const bgfx::Memory* texels) const override;
	void ReadTexture(TextureHandle texture, uint16_t layer, void* texels) const override;
	void UpdateTexture2D(TextureHandle texture, uint16_t layer, glm::u16vec2 origin, glm::u16vec2 size,
	                     const bgfx::Memory* texels) const override;
	void SetName(TextureHandle texture, std::string_view name) const override;
	void Destroy(TextureHandle texture) const override;

	[[nodiscard]] VertexBufferHandle CreateVertexBuffer(const bgfx::Memory* vertices, const VertexDecl& decl) const override;
	[[nodiscard]] DynamicVertexBufferHandle CreateDynamicVertexBuffer(const bgfx::Memory* vertices,
	                                                                  const VertexDecl& decl) const override;
	[[nodiscard]] VertexLayoutHandle CreateVertexLayout(const VertexDecl& decl) const override;
	void Update(DynamicVertexBufferHandle buffer, uint32_t firstVertex, const bgfx::Memory* vertices) const override;
	void SetName(VertexBufferHandle buffer, std::string_view name) const override;
	void Destroy(VertexBufferHandle buffer) const override;
	void Destroy(DynamicVertexBufferHandle buffer) const override;
	void Destroy(VertexLayoutHandle layout) const override;

	[[nodiscard]] IndexBufferHandle CreateIndexBuffer(const bgfx::Memory* indices, bool wide) const override;
	[[nodiscard]] DynamicIndexBufferHandle CreateSharedIndexBuffer(const bgfx::Memory* indices, bool wide) const override;
	void SetName(IndexBufferHandle buffer, std::string_view name) const override;
	void Destroy(IndexBufferHandle buffer) const override;
	void Destroy(DynamicIndexBufferHandle buffer) const override;
};

} // namespace openblack::graphics
