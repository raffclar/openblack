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

#include <span>
#include <string_view>

#include <glm/ext/vector_uint2_sized.hpp>

#include "GraphicsHandle.h"
#include "Texture2D.h"
#include "VertexBuffer.h"

namespace openblack::graphics
{

/// Memory that the device gives for the texels, the vertices or the indices of a resource. Only the device knows its
/// layout: GraphicsDeviceInterface::Bytes gives its bytes. The device owns the memory and frees it after it uses it.
struct DeviceMemory;

/// How a draw reads a texture format
enum class TextureUse : uint8_t
{
	/// A 2D texture that the fragment shaders read
	Texture2D,
	/// A texture that the vertex shaders read
	VertexTexture,
};

/// The flags of a texture: how the device samples it. The values are the values of the graphics library (the client
/// checks them).
namespace texture_flags
{
constexpr uint64_t k_None = 0;
constexpr uint64_t k_MirrorU = 0x1;
constexpr uint64_t k_ClampU = 0x2;
constexpr uint64_t k_BorderU = 0x3;
constexpr uint64_t k_MirrorV = 0x4;
constexpr uint64_t k_ClampV = 0x8;
constexpr uint64_t k_BorderV = 0xC;
/// The nearest texel for the minification, the magnification and the mipmaps
constexpr uint64_t k_Point = 0x540;
} // namespace texture_flags

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
	[[nodiscard]] virtual const DeviceMemory* Copy(const void* data, uint32_t size) const = 0;
	/// Device memory of size bytes. The caller fills it before it gives the memory to the device.
	[[nodiscard]] virtual const DeviceMemory* Alloc(uint32_t size) const = 0;
	/// Device memory that only refers to size bytes at data. The data must stay until the device has used it.
	[[nodiscard]] virtual const DeviceMemory* Reference(const void* data, uint32_t size) const = 0;
	/// The bytes of device memory. Any thread can read and write them until the memory goes to the device.
	[[nodiscard]] virtual std::span<uint8_t> Bytes(const DeviceMemory* memory) const = 0;
	/// Whether the device has a texture format for a use
	[[nodiscard]] virtual bool HasTextureFormat(TextureFormat format, TextureUse use) const = 0;
	/// The largest width and height of a texture that the device can make
	[[nodiscard]] virtual uint32_t MaxTextureSize() const = 0;
	/// Ends the frame of the device, so that the device does the work that it has
	virtual void Frame() const = 0;

	/// Makes a texture with no mipmaps. A texture with no texels can get its texels later.
	[[nodiscard]] virtual TextureHandle CreateTexture2D(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format,
	                                                    uint64_t flags, const DeviceMemory* texels) const = 0;
	/// Replaces a rectangle of the texels of one layer, in a texture made with no texels
	virtual void UpdateTexture2D(TextureHandle texture, uint16_t layer, glm::u16vec2 origin, glm::u16vec2 size,
	                             const DeviceMemory* texels) const = 0;
	/// Asks for the texels of one layer of a texture. The texels are in the data after the frames that the graphics
	/// library needs to read them.
	virtual void ReadTexture(TextureHandle texture, uint16_t layer, void* texels) const = 0;
	virtual void SetName(TextureHandle texture, std::string_view name) const = 0;
	virtual void Destroy(TextureHandle texture) const = 0;

	/// Makes a vertex buffer whose vertices have the attributes of the declaration
	[[nodiscard]] virtual VertexBufferHandle CreateVertexBuffer(const DeviceMemory* vertices, const VertexDecl& decl) const = 0;
	/// Makes a vertex buffer whose vertices can change later
	[[nodiscard]] virtual DynamicVertexBufferHandle CreateDynamicVertexBuffer(const DeviceMemory* vertices,
	                                                                          const VertexDecl& decl) const = 0;
	/// Makes a vertex layout from the declaration. A draw reads a buffer with the layout.
	[[nodiscard]] virtual VertexLayoutHandle CreateVertexLayout(const VertexDecl& decl) const = 0;
	/// Replaces the vertices of a vertex buffer, from the first vertex on
	virtual void Update(DynamicVertexBufferHandle buffer, uint32_t firstVertex, const DeviceMemory* vertices) const = 0;
	virtual void SetName(VertexBufferHandle buffer, std::string_view name) const = 0;
	virtual void Destroy(VertexBufferHandle buffer) const = 0;
	virtual void Destroy(DynamicVertexBufferHandle buffer) const = 0;
	virtual void Destroy(VertexLayoutHandle layout) const = 0;

	/// Makes an index buffer, with indices of 32 bits when wide, or else of 16 bits
	[[nodiscard]] virtual IndexBufferHandle CreateIndexBuffer(const DeviceMemory* indices, bool wide) const = 0;
	/// Makes an index buffer in a part of a large buffer, which the device shares between many small buffers
	[[nodiscard]] virtual DynamicIndexBufferHandle CreateSharedIndexBuffer(const DeviceMemory* indices, bool wide) const = 0;
	virtual void SetName(IndexBufferHandle buffer, std::string_view name) const = 0;
	virtual void Destroy(IndexBufferHandle buffer) const = 0;
	virtual void Destroy(DynamicIndexBufferHandle buffer) const = 0;
};

} // namespace openblack::graphics
