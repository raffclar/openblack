/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the textures and the targets of the shadows of the sun and the moon

#include "SunShadowView.h"

#include "Graphics/GraphicsHandleBgfx.h"

using namespace openblack::graphics;
namespace shadows = openblack::graphics::sun_shadows;

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

constexpr uint64_t k_Clamp = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

/// A map of depth seen from the light, which a pass can read
bgfx::TextureHandle DepthMap(const char* name)
{
	const auto handle = bgfx::createTexture2D(shadows::k_MapTexels, shadows::k_MapTexels, false, 1, bgfx::TextureFormat::D32F,
	                                          BGFX_TEXTURE_RT | k_Clamp | BGFX_SAMPLER_POINT);
	bgfx::setName(handle, name);
	return handle;
}
} // namespace

SunShadowView::~SunShadowView()
{
	for (auto& target : _visibilityTargets)
	{
		Release(target);
	}
	for (auto& texture : _visibility)
	{
		Release(texture);
	}
	for (auto& target : _environmentTargets)
	{
		Release(target);
	}
	for (auto& texture : _environment)
	{
		Release(texture);
	}
	Release(_movingTarget);
	Release(_moving);
	Release(_sceneColour);
}

bgfx::FrameBufferHandle SunShadowView::VisibilityTarget(uint8_t index)
{
	if (!bgfx::isValid(_visibility.at(0)))
	{
		for (size_t i = 0; i < _visibility.size(); ++i)
		{
			_visibility.at(i) = bgfx::createTexture2D(shadows::k_VisibilityTexels, shadows::k_VisibilityTexels, false, 1,
			                                          bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | k_Clamp);
			bgfx::setName(_visibility.at(i), "Sun Visibility");
			_visibilityTargets.at(i) = bgfx::createFrameBuffer(1, &_visibility.at(i), false);
		}
	}
	return _visibilityTargets.at(index);
}

TextureHandle SunShadowView::Visibility(uint8_t index) const
{
	return fromBgfx(_visibility.at(index));
}

bgfx::FrameBufferHandle SunShadowView::EnvironmentTarget(uint8_t set, uint8_t cascade)
{
	const size_t index = static_cast<size_t>(set) * 2 + cascade;
	if (!bgfx::isValid(_environment.at(index)))
	{
		_environment.at(index) = DepthMap("Sun Shadow Environment");
		_environmentTargets.at(index) = bgfx::createFrameBuffer(1, &_environment.at(index), false);
	}
	return _environmentTargets.at(index);
}

TextureHandle SunShadowView::Environment(uint8_t set, uint8_t cascade) const
{
	return fromBgfx(_environment.at(static_cast<size_t>(set) * 2 + cascade));
}

bgfx::FrameBufferHandle SunShadowView::MovingTarget()
{
	if (!bgfx::isValid(_moving))
	{
		_moving = DepthMap("Sun Shadow Moving");
		_movingTarget = bgfx::createFrameBuffer(1, &_moving, false);
	}
	return _movingTarget;
}

TextureHandle SunShadowView::Moving() const
{
	return fromBgfx(_moving);
}

bgfx::FrameBufferHandle SunShadowView::SceneColour(TextureHandle colour, glm::u16vec2 size)
{
	const auto texture = toBgfx(colour);
	if (!bgfx::isValid(_sceneColour) || _sceneColourTexture != texture.idx || _sceneColourSize != size)
	{
		Release(_sceneColour);
		_sceneColour = bgfx::createFrameBuffer(1, &texture, false);
		_sceneColourTexture = texture.idx;
		_sceneColourSize = size;
	}
	return _sceneColour;
}
