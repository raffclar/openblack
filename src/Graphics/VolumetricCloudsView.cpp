/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the textures and the targets of the volumetric clouds

#include "VolumetricCloudsView.h"

#include <algorithm>
#include <chrono>

#include "Graphics/GraphicsHandleBgfx.h"

using namespace openblack::graphics;
namespace clouds = openblack::graphics::volumetric_clouds;

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

template <typename T>
bool Finished(const std::future<T>& future)
{
	return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

constexpr uint64_t k_Repeat = BGFX_SAMPLER_NONE;
constexpr uint64_t k_Clamp = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
} // namespace

VolumetricCloudsView::VolumetricCloudsView()
    : _baseTexels(std::async(std::launch::async, [] { return clouds::BaseNoiseVolume(); }))
    , _detailTexels(std::async(std::launch::async, [] { return clouds::DetailNoiseVolume(); }))
    , _weatherTexels(std::async(std::launch::async, [] { return clouds::WeatherNoise(); }))
    , _blueTexels(std::async(std::launch::async, [] { return clouds::BlueNoise(); }))
{
}

VolumetricCloudsView::~VolumetricCloudsView()
{
	// The threads finish before the noise goes
	for (auto* future : {&_baseTexels, &_detailTexels, &_weatherTexels, &_blueTexels})
	{
		if (future->valid())
		{
			future->wait();
		}
	}
	ReleaseTargets();
	Release(_base);
	Release(_detail);
	Release(_weather);
	Release(_blue);
	Release(_shadowTarget);
	Release(_shadow);
}

bool VolumetricCloudsView::Ready()
{
	if (Finished(_baseTexels))
	{
		const auto texels = _baseTexels.get();
		constexpr auto k_Side = static_cast<uint16_t>(clouds::k_BaseNoiseSide);
		_base = bgfx::createTexture3D(k_Side, k_Side, k_Side, false, bgfx::TextureFormat::RGBA8, k_Repeat,
		                              bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
		bgfx::setName(_base, "Cloud Base Noise");
	}
	if (Finished(_detailTexels))
	{
		const auto texels = _detailTexels.get();
		constexpr auto k_Side = static_cast<uint16_t>(clouds::k_DetailNoiseSide);
		_detail = bgfx::createTexture3D(k_Side, k_Side, k_Side, false, bgfx::TextureFormat::RGBA8, k_Repeat,
		                                bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
		bgfx::setName(_detail, "Cloud Detail Noise");
	}
	if (Finished(_weatherTexels))
	{
		const auto texels = _weatherTexels.get();
		constexpr auto k_Side = static_cast<uint16_t>(clouds::k_WeatherNoiseSide);
		_weather = bgfx::createTexture2D(k_Side, k_Side, false, 1, bgfx::TextureFormat::RGBA8, k_Repeat,
		                                 bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
		bgfx::setName(_weather, "Cloud Weather Noise");
	}
	if (Finished(_blueTexels))
	{
		const auto texels = _blueTexels.get();
		constexpr auto k_Side = static_cast<uint16_t>(clouds::k_BlueNoiseSide);
		_blue = bgfx::createTexture2D(k_Side, k_Side, false, 1, bgfx::TextureFormat::R8, k_Repeat | BGFX_SAMPLER_POINT,
		                              bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
		bgfx::setName(_blue, "Cloud Blue Noise");
	}
	return bgfx::isValid(_base) && bgfx::isValid(_detail) && bgfx::isValid(_weather) && bgfx::isValid(_blue);
}

TextureHandle VolumetricCloudsView::BaseNoise() const
{
	return fromBgfx(_base);
}

TextureHandle VolumetricCloudsView::DetailNoise() const
{
	return fromBgfx(_detail);
}

TextureHandle VolumetricCloudsView::WeatherNoise() const
{
	return fromBgfx(_weather);
}

TextureHandle VolumetricCloudsView::BlueNoise() const
{
	return fromBgfx(_blue);
}

void VolumetricCloudsView::ReleaseTargets()
{
	Release(_marchTarget);
	for (auto& texture : _march)
	{
		Release(texture);
	}
	for (auto& target : _historyTargets)
	{
		Release(target);
	}
	for (auto& texture : _histories)
	{
		Release(texture);
	}
}

glm::u16vec2 VolumetricCloudsView::Fit(glm::u16vec2 screen)
{
	// Half the screen's size each way: a quarter of its pixels
	const glm::u16vec2 size {std::max<uint16_t>(1, static_cast<uint16_t>((screen.x + 1) / 2)),
	                         std::max<uint16_t>(1, static_cast<uint16_t>((screen.y + 1) / 2))};
	if (size == _size && bgfx::isValid(_marchTarget))
	{
		return size;
	}
	ReleaseTargets();
	_size = size;
	constexpr uint64_t k_Flags = BGFX_TEXTURE_RT | k_Clamp;
	_march.at(0) = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA16F, k_Flags);
	_march.at(1) = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RG32F, k_Flags | BGFX_SAMPLER_POINT);
	bgfx::setName(_march.at(0), "Cloud March");
	bgfx::setName(_march.at(1), "Cloud March Depth");
	_marchTarget = bgfx::createFrameBuffer(2, _march.data(), false);
	for (size_t i = 0; i < _histories.size(); ++i)
	{
		_histories.at(i) = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA16F, k_Flags);
		bgfx::setName(_histories.at(i), "Cloud History");
		_historyTargets.at(i) = bgfx::createFrameBuffer(1, &_histories.at(i), false);
	}
	_historyValid = false;
	return size;
}

TextureHandle VolumetricCloudsView::MarchColour() const
{
	return fromBgfx(_march.at(0));
}

TextureHandle VolumetricCloudsView::MarchDepth() const
{
	return fromBgfx(_march.at(1));
}

TextureHandle VolumetricCloudsView::History() const
{
	return fromBgfx(_histories.at(_history));
}

TextureHandle VolumetricCloudsView::PreviousHistory() const
{
	return fromBgfx(_histories.at(1 - _history));
}

bgfx::FrameBufferHandle VolumetricCloudsView::ShadowTarget()
{
	if (!bgfx::isValid(_shadow))
	{
		constexpr auto k_Side = static_cast<uint16_t>(clouds::k_ShadowTexels);
		_shadow = bgfx::createTexture2D(k_Side, k_Side, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | k_Clamp);
		bgfx::setName(_shadow, "Cloud Shadow");
		_shadowTarget = bgfx::createFrameBuffer(1, &_shadow, false);
	}
	return _shadowTarget;
}

TextureHandle VolumetricCloudsView::Shadow() const
{
	return fromBgfx(_shadow);
}
