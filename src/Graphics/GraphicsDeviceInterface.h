/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string_view>

#include <glm/ext/vector_uint2_sized.hpp>

#include "GraphicsHandle.h"
#include "Texture2D.h"
#include "VertexBuffer.h"

namespace bgfx
{
struct Caps;
struct Memory;
} // namespace bgfx

namespace openblack::graphics
{

/// The graphics device that makes, changes and deletes the textures and buffers of the world, and gives the memory that
/// goes into them. The game has one device. The dedicated server has no device, because its resources keep only their
/// data (see UploadMode). The parts of the world that draw nothing use the device only through this interface. Thus the
/// server does not link the graphics library.
class GraphicsDeviceInterface
{
public:
	GraphicsDeviceInterface() = default;
	GraphicsDeviceInterface(const GraphicsDeviceInterface&) = delete;
	GraphicsDeviceInterface& operator=(const GraphicsDeviceInterface&) = delete;
	GraphicsDeviceInterface(GraphicsDeviceInterface&&) = delete;
	GraphicsDeviceInterface& operator=(GraphicsDeviceInterface&&) = delete;
	virtual ~GraphicsDeviceInterface() = default;

	/// Device memory with a copy of size bytes from data. The device owns the memory and frees it after it uses it.
	[[nodiscard]] virtual const bgfx::Memory* Copy(const void* data, uint32_t size) const = 0;
	/// Device memory of size bytes. The caller fills it before it gives the memory to the device.
	[[nodiscard]] virtual const bgfx::Memory* Alloc(uint32_t size) const = 0;
	/// Device memory that only refers to size bytes at data. The data must stay until the device has used it.
	[[nodiscard]] virtual const bgfx::Memory* Reference(const void* data, uint32_t size) const = 0;
	/// What the device can do, for example the texture formats that it has
	[[nodiscard]] virtual const bgfx::Caps* GetCaps() const = 0;
	/// Ends the frame of the device, so that the device does the work that it has
	virtual void Frame() const = 0;

	/// Makes a texture with no mipmaps. A texture with no texels can get its texels later.
	[[nodiscard]] virtual TextureHandle CreateTexture2D(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format,
	                                                    uint64_t flags, const bgfx::Memory* texels) const = 0;
	/// Replaces a rectangle of the texels of one layer, in a texture made with no texels
	virtual void UpdateTexture2D(TextureHandle texture, uint16_t layer, glm::u16vec2 origin, glm::u16vec2 size,
	                             const bgfx::Memory* texels) const = 0;
	/// Asks for the texels of one layer of a texture. The texels are in the data after the frames that the graphics
	/// library needs to read them.
	virtual void ReadTexture(TextureHandle texture, uint16_t layer, void* texels) const = 0;
	virtual void SetName(TextureHandle texture, std::string_view name) const = 0;
	virtual void Destroy(TextureHandle texture) const = 0;

	/// Makes a vertex buffer whose vertices have the attributes of the declaration
	[[nodiscard]] virtual VertexBufferHandle CreateVertexBuffer(const bgfx::Memory* vertices, const VertexDecl& decl) const = 0;
	/// Makes a vertex buffer whose vertices can change later
	[[nodiscard]] virtual DynamicVertexBufferHandle CreateDynamicVertexBuffer(const bgfx::Memory* vertices,
	                                                                          const VertexDecl& decl) const = 0;
	/// Makes a vertex layout from the declaration. A draw reads a buffer with the layout.
	[[nodiscard]] virtual VertexLayoutHandle CreateVertexLayout(const VertexDecl& decl) const = 0;
	/// Replaces the vertices of a vertex buffer, from the first vertex on
	virtual void Update(DynamicVertexBufferHandle buffer, uint32_t firstVertex, const bgfx::Memory* vertices) const = 0;
	virtual void SetName(VertexBufferHandle buffer, std::string_view name) const = 0;
	virtual void Destroy(VertexBufferHandle buffer) const = 0;
	virtual void Destroy(DynamicVertexBufferHandle buffer) const = 0;
	virtual void Destroy(VertexLayoutHandle layout) const = 0;

	/// Makes an index buffer, with indices of 32 bits when wide, or else of 16 bits
	[[nodiscard]] virtual IndexBufferHandle CreateIndexBuffer(const bgfx::Memory* indices, bool wide) const = 0;
	/// Makes an index buffer in a part of a large buffer, which the device shares between many small buffers
	[[nodiscard]] virtual DynamicIndexBufferHandle CreateSharedIndexBuffer(const bgfx::Memory* indices, bool wide) const = 0;
	virtual void SetName(IndexBufferHandle buffer, std::string_view name) const = 0;
	virtual void Destroy(IndexBufferHandle buffer) const = 0;
	virtual void Destroy(DynamicIndexBufferHandle buffer) const = 0;
};

} // namespace openblack::graphics
