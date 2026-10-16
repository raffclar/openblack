/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "UnderwaterSystem.h"

#include <cstddef>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Underwater.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;

void UnderwaterSystem::Update(std::chrono::milliseconds frameRealTime)
{
	// The debug tour puts the camera where it is now on its way
	if (_tourSeconds.has_value() && Locator::camera::has_value())
	{
		*_tourSeconds += std::chrono::duration<float>(frameRealTime).count();
		using graphics::underwater::Tour;
		float length = graphics::underwater::k_TourSeconds;
		if (_tour == Tour::FarSeabed)
		{
			// Six units over the sea bed, looking ahead along it and a little down
			length = graphics::underwater::k_FarTourSeconds;
			const auto here = graphics::underwater::FarTourGround(*_tourSeconds);
			const auto ahead = graphics::underwater::FarTourGround(*_tourSeconds + 1.5f);
			const auto along = glm::length(ahead - here) > 0.0f ? glm::normalize(ahead - here) : glm::vec2(-1.0f, 0.0f);
			const auto target = here + along * 45.0f;
			const glm::vec3 eye {here.x, -_seabed.DepthAt(here) + 6.0f, here.y};
			const glm::vec3 focus {target.x, -_seabed.DepthAt(target) + 1.0f, target.y};
			Locator::camera::value().Place(eye, focus);
		}
		else
		{
			const auto pose = graphics::underwater::TourAt(*_tourSeconds);
			Locator::camera::value().Place(pose.eye, pose.focus);
		}
		if (*_tourSeconds >= length)
		{
			_tourSeconds.reset();
		}
	}
	// How deep the camera is, and the land's sounds and the music turned down under the sea and back up above it. The
	// change takes about a third of a second each way, so nothing clicks at the surface.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	_eyeDepth = _enabled && !inTemple ? graphics::underwater::EyeDepth(eye.y) : 0.0f;
	_eyeUnder = _eyeDepth > 0.0f;
	constexpr float k_DuckUnder = 0.3f;
	constexpr float k_DuckPerSecond = 2.5f;
	const float target = _eyeUnder ? k_DuckUnder : 1.0f;
	const float step = k_DuckPerSecond * std::chrono::duration<float>(frameRealTime).count();
	_duck = target < _duck ? std::max(target, _duck - step) : std::min(target, _duck + step);
	if (Locator::audio::has_value())
	{
		Locator::audio::value().SetUnderwaterDuck(_duck);
	}

	if (!Locator::terrainSystem::has_value() || !Locator::terrainSystem::value().IsLoaded())
	{
		return;
	}
	// The texture caches can be emptied when a land opens, so the sand is looked for each frame
	LoadSand();
	const auto& island = Locator::terrainSystem::value();
	const void* land = &island.GetHeightMap();
	if (land == _builtFor)
	{
		return;
	}
	_builtFor = land;
	// A corner for each cell corner of the whole map: the sea bed goes on past the land's blocks to the map's edge
	constexpr int k_Corners = LandIslandInterface::k_MapCellsPerSide + 1;
	const glm::ivec2 size {k_Corners, k_Corners};
	std::vector<uint8_t> altitudes(static_cast<size_t>(size.x) * static_cast<size_t>(size.y));
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			altitudes[static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x)] =
			    island.GetCell(glm::u16vec2(x, z)).altitude;
		}
	}
	_seabed = seabed::Build(altitudes, size, glm::vec2(0.0f), LandIslandInterface::k_CellSize);
	++_revision;
}

void UnderwaterSystem::CycleEffectOff()
{
	using graphics::underwater::Effect;
	const auto next = (static_cast<uint8_t>(_effectOff) + 1) % static_cast<uint8_t>(Effect::_Count);
	_effectOff = static_cast<Effect>(next);
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: effect {} turned off (0 is none)", next);
}

void UnderwaterSystem::LoadSand()
{
	// The sand of the sea bed, from the experiment's folder, through the texture cache
	auto& textures = Locator::resources::value().GetTextures();
	const std::filesystem::path sand {graphics::underwater::k_SandPath};
	if (textures.Contains(graphics::underwater::k_SandTextureId.value()) || !std::filesystem::exists(sand))
	{
		return;
	}
	try
	{
		textures.Load(graphics::underwater::k_SandTextureId.value(), resources::Texture2DLoader::FromDiskTag {}, sand);
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Loaded the sea bed's sand from {}", sand.string());
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "{}", err.what());
	}
}

float UnderwaterSystem::FloorAt(glm::vec2 point, float landHeight) const
{
	if (!_enabled)
	{
		return landHeight;
	}
	return seabed::FloorHeight(_seabed, point, landHeight);
}
