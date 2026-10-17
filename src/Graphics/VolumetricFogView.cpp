/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the textures and the targets of the volumetric fog

#include "VolumetricFogView.h"

#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/VolumetricFog.h"

using namespace openblack::graphics;
namespace fog = openblack::graphics::volumetric_fog;

namespace
{
template <typename Handle>
void Release(Handle& handle)
{
	if (bgfx::isValid(handle))
	{
		bgfx::destroy(handle);
		handle = BGFX_INVALID_HANDLE;
	}
}

constexpr uint64_t k_Flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
} // namespace

VolumetricFogView::~VolumetricFogView()
{
	for (auto& target : _scatterTargets)
	{
		Release(target);
	}
	for (auto& texture : _scatter)
	{
		Release(texture);
	}
	Release(_sumTarget);
	Release(_sum);
}

glm::u16vec2 VolumetricFogView::Size()
{
	return {fog::k_CellsAcross * fog::k_TilesAcross, fog::k_CellsDown * fog::k_TileRows};
}

void VolumetricFogView::Make()
{
	if (bgfx::isValid(_sum))
	{
		return;
	}
	const auto size = Size();
	for (size_t i = 0; i < _scatter.size(); ++i)
	{
		_scatter.at(i) = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA16F, k_Flags);
		bgfx::setName(_scatter.at(i), "Fog Scatter");
		_scatterTargets.at(i) = bgfx::createFrameBuffer(1, &_scatter.at(i), false);
	}
	_sum = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA16F, k_Flags);
	bgfx::setName(_sum, "Fog Sum");
	_sumTarget = bgfx::createFrameBuffer(1, &_sum, false);
}

bgfx::FrameBufferHandle VolumetricFogView::ScatterTarget()
{
	Make();
	return _scatterTargets.at(_current);
}

TextureHandle VolumetricFogView::Scatter() const
{
	return fromBgfx(_scatter.at(_current));
}

TextureHandle VolumetricFogView::PreviousScatter() const
{
	return fromBgfx(_scatter.at(1 - _current));
}

bgfx::FrameBufferHandle VolumetricFogView::SumTarget()
{
	Make();
	return _sumTarget;
}

TextureHandle VolumetricFogView::Sum() const
{
	return fromBgfx(_sum);
}
