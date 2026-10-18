/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLightFrame.h"

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack
{

LandLightInputs FrameLandLightInputs()
{
	LandLightInputs inputs;
	if (Locator::skySystem::has_value())
	{
		inputs.skyType = Locator::skySystem::value().GetCurrentSkyType();
	}
	if (Locator::alignmentSystem::has_value())
	{
		inputs.alignment = Locator::alignmentSystem::value().GetSkyAlignment();
	}
	if (Locator::weatherSystem::has_value() && Locator::camera::has_value())
	{
		const auto camera = Locator::camera::value().GetOrigin();
		inputs.overcast = Locator::weatherSystem::value().GetOvercast(camera);
		inputs.flash = Locator::weatherSystem::value().GetLightningFlash(camera);
	}
	return inputs;
}

uint32_t FrameLandLight(uint8_t level)
{
	const auto& palettes = Locator::resources::value().GetLandLightPalettes();
	if (!palettes.Contains(LandLightPalette::k_Id.value()))
	{
		return 0xFFFFFFu;
	}
	const auto inputs = FrameLandLightInputs();
	LandLightTable table;
	table.Build(*palettes.Handle(LandLightPalette::k_Id.value()), inputs.skyType, inputs.alignment, inputs.overcast,
	            inputs.flash);
	// The texels are red, green, blue and alpha bytes
	const auto texel = table.GetTexels().at(level);
	return ((texel & 0xFFu) << 16u) | (texel & 0xFF00u) | ((texel >> 16u) & 0xFFu);
}

FrameLightTable FrameLandLightTable()
{
	FrameLightTable light;
	const auto& palettes = Locator::resources::value().GetLandLightPalettes();
	if (!palettes.Contains(LandLightPalette::k_Id.value()))
	{
		light.colours.fill(0xFFFFFFFFu);
		return light;
	}
	const auto inputs = FrameLandLightInputs();
	LandLightTable table;
	table.Build(*palettes.Handle(LandLightPalette::k_Id.value()), inputs.skyType, inputs.alignment, inputs.overcast,
	            inputs.flash);
	// The texels are red, green, blue and alpha bytes
	const auto& texels = table.GetTexels();
	for (size_t level = 0; level < texels.size(); ++level)
	{
		const auto texel = texels.at(level);
		light.colours.at(level) =
		    (texel & 0xFF000000u) | ((texel & 0xFFu) << 16u) | (texel & 0xFF00u) | ((texel >> 16u) & 0xFFu);
	}
	const auto& haze = table.GetHaze();
	light.haze = {
	    .on = Locator::config::has_value() && graphics::detail_level::Fog(Locator::config::value().detailLevel),
	    .nearDepth = haze.nearDistance,
	    .farDepth = haze.farDistance,
	    .k = static_cast<int32_t>(haze.k),
	};
	return light;
}

uint32_t FrameLandLightAt(glm::vec3 point)
{
	const auto light = FrameLandLightTable();
	const auto* island = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	// The corners of a cell are its own luminosity and those of the cells after it in the block of land
	const land_point_light::CornersAt corners = [island](int32_t x, int32_t z) -> std::optional<land_point_light::Corners> {
		if (island == nullptr)
		{
			return std::nullopt;
		}
		const auto* cell = island->FindCell({static_cast<uint16_t>(x), static_cast<uint16_t>(z)});
		if (cell == nullptr)
		{
			return std::nullopt;
		}
		return land_point_light::Corners {cell[0].luminosity, cell[1].luminosity, cell[17].luminosity, cell[18].luminosity};
	};
	const auto lit = land_point_light::LightAt(point, light.colours, corners);
	if (!Locator::camera::has_value())
	{
		return lit;
	}
	const auto depth = land_point_light::DepthOf(point, Locator::camera::value().GetViewProjectionMatrix());
	return land_point_light::Hazed(lit, depth, light.haze);
}

} // namespace openblack
