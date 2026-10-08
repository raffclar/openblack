/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Sky.h"

#include <cassert>

#include <algorithm>
#include <span>

#include <bgfx/bgfx.h>
#include <glm/vec3.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Common/Bitmap16B.h"
#include "Common/StringUtils.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::filesystem;
using namespace openblack::graphics;

namespace openblack
{

Sky::Sky() noexcept
{
	auto& fileSystem = Locator::filesystem::value();

	// the meshes, from the mesh cache; one that does not load stays an empty mesh, which draws nothing
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto load = [&meshes](const std::filesystem::path& path, const char* name) {
		const auto id = entt::hashed_string(("sky/" + path.filename().string()).c_str()).value();
		try
		{
			if (!meshes.Contains(id))
			{
				meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
			}
			return meshes.Handle(id).handle();
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Sky: cannot load {}: {}", path.generic_string(), e.what());
			return std::make_shared<graphics::L3DMesh>(name);
		}
	};
	_mesh = load(fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "sky.l3d", "Sky");
	_sunMesh = load(fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "sun.l3d", "Sun");
	_moonMesh = load(fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "moon.l3d", "Moon");
	_cloudMesh = load(fileSystem.GetPath<filesystem::Path::Landscape>() / "mist.l3d", "Mist");

	// TODO (#749) Maybe use std::views::enumerate
	for (uint32_t idx = 0; const auto& alignment : k_Alignments)
	{
		for (const auto& timeView : k_Times)
		{
			auto time = std::string(timeView);
			auto prefix = std::string("sky");
			if (idx >= k_Times.size() && idx < 2 * k_Times.size())
			{
				time = string_utils::Capitalise(time);
				prefix = string_utils::Capitalise(prefix);
			}
			const auto filename = fmt::format("{}_{}_{}.555", prefix, alignment, time);
			const auto path = fileSystem.GetPath<filesystem::Path::WeatherSystem>() / filename;
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading sky texture: {}", path.generic_string());

			const Bitmap16B bitmap(resources::LoadBlob(Locator::resources::value().GetBlobs(), path));
			assert(bitmap.Size() == k_LayerTexels * sizeof(uint16_t));
			memcpy(&_bitmaps.at(idx * k_LayerTexels), bitmap.Data(), bitmap.Size());
			++idx;
		}
	}

	_timeOfDay = 1.0f;

	// Three dynamic 256 x 256 textures, one per alignment, built whole at once and then followed by sky_type::DomeBlend.
	// The original's set-up builds them with the sky type of its hour on its own thresholds 4.5 / 7 / 7.5 / 8.25; here
	// with the dome's current sky type (inferred, no visible effect: the last hour jump when the land opens and
	// openblack's Reset, DayNightClock::SetScriptTime(12), rebuild them whole right after; the last jump wins)
	_texture = std::make_unique<Texture2D>("Sky");
	_texture->Create(k_Size, k_Size, static_cast<uint16_t>(k_Alignments.size()), TextureFormat::BGR5A1, Wrapping::ClampEdge,
	                 Filter::Linear, nullptr);
	BlendDome({sky_type::Dome().Built(), 0, sky_type::DomeBlend::k_Rows});
}

Sky::~Sky() noexcept = default;

void Sky::SetTime(float time) noexcept
{
	assert(time <= 24.0f);
	_timeOfDay = time;
}

void Sky::UpdateDome() noexcept
{
	const auto blocks = sky_type::Dome().Advance(sky_type::Frame());
	for (int i = 0; i < blocks.count; ++i)
	{
		BlendDome(blocks.blocks.at(i));
	}
}

void Sky::BlendDome(const sky_type::DomeBlock& block) noexcept
{
	// The blend path of detail levels 2..6. Levels 0 and 1 (DetailLevel::skyNoBlend: no blend, day textures, per-T
	// tint, 128 rows) are not ported: openblack blends at every detail level.
	const auto weight = sky_type::DomeWeightOf(block.skyType);
	const int lastRow = std::min(block.firstRow + block.rowCount, static_cast<int>(k_Size));
	if (lastRow <= block.firstRow)
	{
		return;
	}
	const auto first = static_cast<size_t>(block.firstRow) * k_Size;
	const auto count = static_cast<size_t>(lastRow - block.firstRow) * k_Size;
	for (size_t a = 0; a < k_Alignments.size(); ++a)
	{
		// the original's time of day 0 _day, 1 _dusk, 2 _night is k_Times index 2 - tod
		const auto lower = (a * k_Times.size() + (2 - weight.lower)) * k_LayerTexels;
		const auto upper = (a * k_Times.size() + (2 - weight.upper)) * k_LayerTexels;
		sky_type::BlendRows555(std::span(_dome).subspan(a * k_LayerTexels + first, count),
		                       std::span<const uint16_t>(_bitmaps).subspan(lower + first, count),
		                       std::span<const uint16_t>(_bitmaps).subspan(upper + first, count), weight.weight);
	}
	// The original marks the texture dirty only once first + rows reaches the height, and a dirty texture is converted
	// and uploaded on its next bind, in the same frame's sky draw. So the GPU sees the dome change all at once when the
	// last block is done.
	if (lastRow < static_cast<int>(k_Size))
	{
		return;
	}
	for (size_t a = 0; a < k_Alignments.size(); ++a)
	{
		bgfx::updateTexture2D(
		    toBgfx(_texture->GetNativeHandle()), static_cast<uint16_t>(a), 0, 0, 0, k_Size, k_Size,
		    bgfx::copy(&_dome.at(a * k_LayerTexels), static_cast<uint32_t>(k_LayerTexels * sizeof(uint16_t))));
	}
}

} // namespace openblack
