/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Texture2D.h"

#include <cassert>

#include <bgfx/bgfx.h>
#include <glm/ext/vector_uint2_sized.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "Common/MainThreadQueue.h"
#include "GraphicsHandleBgfx.h"
#include "MainThreadWork.h"
#include "UploadPacer.h"

namespace openblack::graphics
{

Texture2D::Texture2D(std::string name)
    : _name(std::move(name))
    , _handle(BGFX_INVALID_HANDLE)
{
}

Texture2D::~Texture2D()
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "Texture2D::~Texture2D");
	if (bgfx::isValid(toBgfx(_handle)))
	{
		bgfx::destroy(toBgfx(_handle));
	}
}

void Texture2D::Create(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format, Wrapping wrapping, Filter filter,
                       const void* memory) noexcept
{
	CreateWithinFrame(width, height, layers, format, wrapping, filter, memory);
}

void Texture2D::CreateWithinFrame(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format, Wrapping wrapping,
                                  Filter filter, const void* memory) noexcept
{
	uint64_t flags = BGFX_TEXTURE_NONE;
	switch (wrapping)
	{
	case Wrapping::ClampEdge:
		flags |= BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		break;
	case Wrapping::ClampBorder:
		flags |= BGFX_SAMPLER_U_BORDER | BGFX_SAMPLER_V_BORDER;
		break;
	case Wrapping::Repeat:
		break;
	case Wrapping::MirroredRepeat:
		flags |= BGFX_SAMPLER_U_MIRROR | BGFX_SAMPLER_V_MIRROR;
		break;
	}
	switch (filter)
	{
	case Filter::Nearest:
		flags |= BGFX_SAMPLER_POINT;
		break;
	case Filter::Linear:
		break;
	default:
		assert(false);
	}
	// The size of the texture is known at once. The graphics library makes the texture on the main thread: a turn on
	// another thread gives that work to the main thread, which makes the texture before the next draw.
	Describe(width, height, layers, format);
	const auto* texels = reinterpret_cast<const bgfx::Memory*>(memory);
	PostToMainThread(MainThreadApi::Graphics, [this, width, height, layers, format, flags, texels] {
		if (texels != nullptr)
		{
			UploadPacer::Pace(texels->size);
		}
		_handle = fromBgfx(bgfx::createTexture2D(width, height, false, layers, toBgfx(format), flags, texels));
		// Out of textures, it isn't made: only one that is can be named
		if (bgfx::isValid(toBgfx(_handle)))
		{
			bgfx::setName(toBgfx(_handle), _name.c_str());
		}
	});
}

void Texture2D::Describe(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format) noexcept
{
	bgfx::TextureInfo textureInfo;
	bgfx::calcTextureSize(textureInfo, width, height, 1, false, false, layers, toBgfx(format));
	_resolution = glm::u16vec2(textureInfo.width, textureInfo.height);
	_numLayers = textureInfo.numLayers;
	_format = fromBgfx(textureInfo.format);
	_stride = textureInfo.width * textureInfo.bitsPerPixel / 8;
	_storageSize = textureInfo.storageSize;
}

void Texture2D::Update(const void* data, uint32_t size) const
{
	UpdateLayerFromMemory(0, bgfx::copy(data, size), size);
}

void Texture2D::UpdateLayerRegion(uint16_t layer, glm::u16vec2 origin, glm::u16vec2 size, const void* data,
                                  uint32_t bytes) const
{
	const auto* memory = bgfx::copy(data, bytes);
	PostToMainThread(MainThreadApi::Graphics, [this, layer, origin, size, memory] {
		bgfx::updateTexture2D(toBgfx(_handle), layer, 0, origin.x, origin.y, size.x, size.y, memory);
	});
}

void Texture2D::UpdateLayer(uint16_t layer, const void* data, uint32_t size) const
{
	UpdateLayerFromMemory(layer, bgfx::copy(data, size), std::nullopt);
}

void Texture2D::UpdateLayerFromMemory(uint16_t layer, const void* memory) const
{
	UpdateLayerFromMemory(layer, memory, std::nullopt);
}

void Texture2D::UpdateLayerFromMemory(uint16_t layer, const void* memory, std::optional<uint32_t> paced) const
{
	const auto* texels = static_cast<const bgfx::Memory*>(memory);
	PostToMainThread(MainThreadApi::Graphics, [this, layer, texels, paced] {
		if (paced.has_value())
		{
			UploadPacer::Pace(*paced);
		}
		bgfx::updateTexture2D(toBgfx(_handle), layer, 0, 0, 0, _resolution.x, _resolution.y, texels);
	});
}

void Texture2D::DumpTexture() const
{
	assert(!_name.empty());
	std::vector<uint8_t> pixels;
	pixels.resize(_storageSize);
	// A read gives one layer
	for (uint16_t i = 0; i < _numLayers; ++i)
	{
		bgfx::TextureRegion layer;
		layer.init(toBgfx(_handle));
		layer.z = i;
		bgfx::read(layer, &pixels[static_cast<size_t>(i) * _stride * _resolution.y]);
	}
	bgfx::frame();
	// TODO(bwrsandman): get the number of components from _info.format
	const auto numComponents = 4u;
	for (uint16_t i = 0; i < _numLayers; ++i)
	{
		auto filename = "dump/" + _name + "_" + std::to_string(i) + ".png";
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Writing texture layer {} to {}.", i, filename.c_str());
		auto* currentPixels = &pixels[i * _stride * _resolution.y];
		auto writeResult =
		    stbi_write_png(filename.c_str(), _resolution.x, _resolution.y, numComponents, currentPixels, _stride);
		if (writeResult == 0)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Writing texture to {} failed!", filename.c_str());
			break;
		}
	}
}

} // namespace openblack::graphics
