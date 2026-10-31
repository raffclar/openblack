/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BgfxGraphicsDevice.h"

#include <array>

#include <bgfx/bgfx.h>

#include "GraphicsHandleBgfx.h"

namespace openblack::graphics
{
namespace
{
constexpr std::array<bgfx::AttribType::Enum, 3> k_Types {
    bgfx::AttribType::Uint8,
    bgfx::AttribType::Int16,
    bgfx::AttribType::Float,
};
constexpr std::array<bgfx::Attrib::Enum, 18> k_Attributes {
    bgfx::Attrib::Enum::Position,  bgfx::Attrib::Enum::Normal,    bgfx::Attrib::Enum::Tangent,   bgfx::Attrib::Enum::Bitangent,
    bgfx::Attrib::Enum::Color0,    bgfx::Attrib::Enum::Color1,    bgfx::Attrib::Enum::Color2,    bgfx::Attrib::Enum::Color3,
    bgfx::Attrib::Enum::Indices,   bgfx::Attrib::Enum::Weight,    bgfx::Attrib::Enum::TexCoord0, bgfx::Attrib::Enum::TexCoord1,
    bgfx::Attrib::Enum::TexCoord2, bgfx::Attrib::Enum::TexCoord3, bgfx::Attrib::Enum::TexCoord4, bgfx::Attrib::Enum::TexCoord5,
    bgfx::Attrib::Enum::TexCoord6, bgfx::Attrib::Enum::TexCoord7,
};

/// The layout of the graphics library for the attributes of the declaration
bgfx::VertexLayout LayoutOf(const VertexDecl& decl)
{
	bgfx::VertexLayout layout;
	layout.begin();
	for (const auto& attribute : decl)
	{
		layout.add(k_Attributes.at(static_cast<size_t>(attribute.attribute)), attribute.num,
		           k_Types.at(static_cast<size_t>(attribute.type)), attribute.normalized, attribute.asInt);
	}
	layout.end();
	return layout;
}

uint16_t IndexFlags(bool wide)
{
	return wide ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE;
}

// The world gives the flags and the handles of the graphics library with its own constants
static_assert(texture_flags::k_None == BGFX_TEXTURE_NONE);
static_assert(texture_flags::k_MirrorU == BGFX_SAMPLER_U_MIRROR);
static_assert(texture_flags::k_ClampU == BGFX_SAMPLER_U_CLAMP);
static_assert(texture_flags::k_BorderU == BGFX_SAMPLER_U_BORDER);
static_assert(texture_flags::k_MirrorV == BGFX_SAMPLER_V_MIRROR);
static_assert(texture_flags::k_ClampV == BGFX_SAMPLER_V_CLAMP);
static_assert(texture_flags::k_BorderV == BGFX_SAMPLER_V_BORDER);
static_assert(texture_flags::k_Point == BGFX_SAMPLER_POINT);
static_assert(k_InvalidHandleId == bgfx::kInvalidHandle);

/// The memory of the graphics library that device memory is. The device gives the memory of the graphics library as
/// device memory, so the conversion gives back the first pointer.
const bgfx::Memory* ToBgfx(const DeviceMemory* memory)
{
	return reinterpret_cast<const bgfx::Memory*>(memory);
}

const DeviceMemory* FromBgfx(const bgfx::Memory* memory)
{
	return reinterpret_cast<const DeviceMemory*>(memory);
}
} // namespace

const DeviceMemory* BgfxGraphicsDevice::Copy(const void* data, uint32_t size) const
{
	return FromBgfx(bgfx::copy(data, size));
}

const DeviceMemory* BgfxGraphicsDevice::Alloc(uint32_t size) const
{
	return FromBgfx(bgfx::alloc(size));
}

const DeviceMemory* BgfxGraphicsDevice::Reference(const void* data, uint32_t size) const
{
	return FromBgfx(bgfx::makeRef(data, size));
}

std::span<uint8_t> BgfxGraphicsDevice::Bytes(const DeviceMemory* memory) const
{
	const auto* bgfxMemory = ToBgfx(memory);
	return {bgfxMemory->data, bgfxMemory->size};
}

bool BgfxGraphicsDevice::HasTextureFormat(TextureFormat format, TextureUse use) const
{
	const auto flag = use == TextureUse::VertexTexture ? BGFX_CAPS_FORMAT_TEXTURE_VERTEX : BGFX_CAPS_FORMAT_TEXTURE_2D;
	return (bgfx::getCaps()->formats[toBgfx(format)] & flag) != 0;
}

uint32_t BgfxGraphicsDevice::MaxTextureSize() const
{
	return bgfx::getCaps()->limits.maxTextureSize;
}

void BgfxGraphicsDevice::Frame() const
{
	bgfx::frame();
}

TextureHandle BgfxGraphicsDevice::CreateTexture2D(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format,
                                                  uint64_t flags, const DeviceMemory* texels) const
{
	return fromBgfx(bgfx::createTexture2D(width, height, false, layers, toBgfx(format), flags, ToBgfx(texels)));
}

void BgfxGraphicsDevice::UpdateTexture2D(TextureHandle texture, uint16_t layer, glm::u16vec2 origin, glm::u16vec2 size,
                                         const DeviceMemory* texels) const
{
	bgfx::updateTexture2D(toBgfx(texture), layer, 0, origin.x, origin.y, size.x, size.y, ToBgfx(texels));
}

void BgfxGraphicsDevice::ReadTexture(TextureHandle texture, uint16_t layer, void* texels) const
{
	bgfx::TextureRegion region;
	region.init(toBgfx(texture));
	region.z = layer;
	bgfx::read(region, texels);
}

void BgfxGraphicsDevice::SetName(TextureHandle texture, std::string_view name) const
{
	bgfx::setName(toBgfx(texture), name.data(), static_cast<int32_t>(name.size()));
}

void BgfxGraphicsDevice::Destroy(TextureHandle texture) const
{
	bgfx::destroy(toBgfx(texture));
}

VertexBufferHandle BgfxGraphicsDevice::CreateVertexBuffer(const DeviceMemory* vertices, const VertexDecl& decl) const
{
	return fromBgfx(bgfx::createVertexBuffer(ToBgfx(vertices), LayoutOf(decl)));
}

DynamicVertexBufferHandle BgfxGraphicsDevice::CreateDynamicVertexBuffer(const DeviceMemory* vertices,
                                                                        const VertexDecl& decl) const
{
	return fromBgfx(bgfx::createDynamicVertexBuffer(ToBgfx(vertices), LayoutOf(decl)));
}

VertexLayoutHandle BgfxGraphicsDevice::CreateVertexLayout(const VertexDecl& decl) const
{
	return fromBgfx(bgfx::createVertexLayout(LayoutOf(decl)));
}

void BgfxGraphicsDevice::Update(DynamicVertexBufferHandle buffer, uint32_t firstVertex, const DeviceMemory* vertices) const
{
	bgfx::update(toBgfx(buffer), firstVertex, ToBgfx(vertices));
}

void BgfxGraphicsDevice::SetName(VertexBufferHandle buffer, std::string_view name) const
{
	bgfx::setName(toBgfx(buffer), name.data(), static_cast<int32_t>(name.size()));
}

void BgfxGraphicsDevice::Destroy(VertexBufferHandle buffer) const
{
	bgfx::destroy(toBgfx(buffer));
}

void BgfxGraphicsDevice::Destroy(DynamicVertexBufferHandle buffer) const
{
	bgfx::destroy(toBgfx(buffer));
}

void BgfxGraphicsDevice::Destroy(VertexLayoutHandle layout) const
{
	bgfx::destroy(toBgfx(layout));
}

IndexBufferHandle BgfxGraphicsDevice::CreateIndexBuffer(const DeviceMemory* indices, bool wide) const
{
	return fromBgfx(bgfx::createIndexBuffer(ToBgfx(indices), IndexFlags(wide)));
}

DynamicIndexBufferHandle BgfxGraphicsDevice::CreateSharedIndexBuffer(const DeviceMemory* indices, bool wide) const
{
	return fromBgfx(bgfx::createDynamicIndexBuffer(ToBgfx(indices), IndexFlags(wide)));
}

void BgfxGraphicsDevice::SetName(IndexBufferHandle buffer, std::string_view name) const
{
	bgfx::setName(toBgfx(buffer), name.data(), static_cast<int32_t>(name.size()));
}

void BgfxGraphicsDevice::Destroy(IndexBufferHandle buffer) const
{
	bgfx::destroy(toBgfx(buffer));
}

void BgfxGraphicsDevice::Destroy(DynamicIndexBufferHandle buffer) const
{
	bgfx::destroy(toBgfx(buffer));
}

} // namespace openblack::graphics
