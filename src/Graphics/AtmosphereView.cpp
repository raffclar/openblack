/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the textures and the targets of the air's light

#include "AtmosphereView.h"

#include "Graphics/GraphicsHandleBgfx.h"

using namespace openblack::graphics;
namespace atmos = openblack::graphics::atmosphere;

OwnedTarget::OwnedTarget(const char* name, uint16_t width, uint16_t height, bgfx::TextureFormat::Enum format, bool filtered)
    : _name(name)
    , _width(width)
    , _height(height)
    , _format(format)
    , _filtered(filtered)
{
}

OwnedTarget::~OwnedTarget()
{
	if (bgfx::isValid(_target))
	{
		bgfx::destroy(_target);
	}
	if (bgfx::isValid(_texture))
	{
		bgfx::destroy(_texture);
	}
}

bgfx::FrameBufferHandle OwnedTarget::Target()
{
	if (!bgfx::isValid(_texture))
	{
		uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		if (!_filtered)
		{
			flags |= BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;
		}
		_texture = bgfx::createTexture2D(_width, _height, false, 1, _format, flags);
		bgfx::setName(_texture, _name);
		_target = bgfx::createFrameBuffer(1, &_texture, false);
	}
	return _target;
}

OwnedTarget& AtmosphereView::Aurora(glm::u16vec2 size)
{
	if (_aurora == nullptr || _aurora->Width() != size.x || _aurora->Height() != size.y)
	{
		_aurora = std::make_unique<OwnedTarget>("Aurora", size.x, size.y, bgfx::TextureFormat::RGBA16F);
	}
	return *_aurora;
}

TextureHandle OwnedTarget::Texture() const
{
	return fromBgfx(_texture);
}

AtmosphereView::AtmosphereView()
    : transmittance("Atmosphere Transmittance", atmos::k_TransmittanceWidth, atmos::k_TransmittanceHeight,
                    bgfx::TextureFormat::RGBA16F)
    , skyView("Atmosphere Sky View", atmos::k_SkyViewWidth, atmos::k_SkyViewHeight, bgfx::TextureFormat::RGBA16F)
    , aerial("Atmosphere Aerial", atmos::k_AerialCells * atmos::k_AerialTilesAcross,
             atmos::k_AerialCells * (atmos::k_AerialSlices / atmos::k_AerialTilesAcross), bgfx::TextureFormat::RGBA16F)
{
}
