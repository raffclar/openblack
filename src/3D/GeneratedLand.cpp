/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GeneratedLand.h"

#include <charconv>

#include <exception>
#include <map>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"

using namespace openblack;
using landgen::countries::Surface;

namespace
{
constexpr int k_CellsPerBlock = 16;
constexpr int k_BlockCornersPerSide = k_CellsPerBlock + 1;
constexpr float k_BlockSize = 160.0f;

/// The materials of the story for the surfaces. Each one is the material of its kind that the lands of the story show
/// most: the sand of the beaches of Land 1, the snow and the ice of the mountains of Land 2, the jungle floor of Land 5.
constexpr std::array<generated_land::MaterialSource, landgen::countries::k_SurfaceCount> k_Sources {{
    {"Land2.lnd", 11}, // wet sand
    {"Land1.lnd", 6},  // sand
    {"Land5.lnd", 0},  // red desert sand
    {"Land1.lnd", 1},  // grass
    {"Land2.lnd", 13}, // lush grass
    {"Land4.lnd", 12}, // dry grass
    {"Land1.lnd", 0},  // earth
    {"Land4.lnd", 21}, // river mud
    {"Land4.lnd", 20}, // wet grass
    {"Land5.lnd", 16}, // forest undergrowth, the jungle floor
    {"Land1.lnd", 5},  // heather
    {"Land4.lnd", 9},  // scrub
    {"Land4.lnd", 2},  // gravel
    {"Land1.lnd", 4},  // solid rock
    {"Land4.lnd", 3},  // dark solid rock
    {"Land2.lnd", 5},  // snow
    {"Land2.lnd", 3},  // snowy mountain tops
    {"Land2.lnd", 0},  // solid ice
}};

/// True when each corner of the block is on the sea bed, so that the block is all open sea
bool AllOpenSea(const landgen::Grid<lnd::LNDCell>& cells, int blockX, int blockZ)
{
	for (int cx = 0; cx < k_BlockCornersPerSide; ++cx)
	{
		for (int cz = 0; cz < k_BlockCornersPerSide; ++cz)
		{
			if (cells.At((blockX * k_CellsPerBlock) + cx, (blockZ * k_CellsPerBlock) + cz).altitude != 0)
			{
				return false;
			}
		}
	}
	return true;
}
} // namespace

std::string generated_land::PathOf(uint64_t seed)
{
	return std::string(k_PathPrefix) + std::to_string(seed);
}

std::optional<uint64_t> generated_land::SeedOf(std::string_view path)
{
	if (!path.starts_with(k_PathPrefix))
	{
		return std::nullopt;
	}
	const auto digits = path.substr(k_PathPrefix.size());
	uint64_t seed = 0;
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), seed);
	if (error != std::errc() || end != digits.data() + digits.size() || digits.empty())
	{
		return std::nullopt;
	}
	return seed;
}

const std::array<generated_land::MaterialSource, landgen::countries::k_SurfaceCount>& generated_land::MaterialSources()
{
	return k_Sources;
}

std::optional<generated_land::StoryParts> generated_land::ReadStoryParts(filesystem::FileSystemInterface& files)
{
	std::map<std::string_view, lnd::LNDFile> landscapes;
	const auto open = [&files, &landscapes](std::string_view name) -> const lnd::LNDFile* {
		if (const auto it = landscapes.find(name); it != landscapes.end())
		{
			return &it->second;
		}
		const auto path = files.GetPath<filesystem::Path::Landscape>() / name;
		try
		{
			if (!files.Exists(path))
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "A generated land needs the landscape {}", path.generic_string());
				return nullptr;
			}
			auto& file = landscapes[name];
			if (file.Open(files.ReadAll(path)) != lnd::LNDResult::Success)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The landscape {} does not read", path.generic_string());
				landscapes.erase(name);
				return nullptr;
			}
			return &file;
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The landscape {} does not read: {}", path.generic_string(), e.what());
			landscapes.erase(name);
			return nullptr;
		}
	};

	StoryParts parts;
	for (const auto& source : k_Sources)
	{
		const auto* file = open(source.landscape);
		if (file == nullptr || source.material >= file->GetMaterials().size())
		{
			return std::nullopt;
		}
		parts.surfaces.push_back(file->GetMaterials()[source.material]);
	}
	const auto* noiseFile = open(k_NoiseLandscape);
	if (noiseFile == nullptr)
	{
		return std::nullopt;
	}
	const auto& extra = noiseFile->GetExtra();
	parts.noise.assign(extra.noise.texels.begin(), extra.noise.texels.end());
	parts.bump.assign(extra.bump.texels.begin(), extra.bump.texels.end());
	return parts;
}

LandData generated_land::Build(const landgen::GeneratedLand& land, const StoryParts& parts)
{
	LandData data;
	for (int x = 0; x < LandData::k_BlocksPerSide; ++x)
	{
		for (int z = 0; z < LandData::k_BlocksPerSide; ++z)
		{
			if (AllOpenSea(land.cells, x, z))
			{
				continue;
			}
			auto& block = data.blocks.emplace_back();
			for (int cx = 0; cx < k_BlockCornersPerSide; ++cx)
			{
				for (int cz = 0; cz < k_BlockCornersPerSide; ++cz)
				{
					block.cells.at(static_cast<size_t>((cx * k_BlockCornersPerSide) + cz)) =
					    land.cells.At((x * k_CellsPerBlock) + cx, (z * k_CellsPerBlock) + cz);
				}
			}
			block.index = static_cast<uint32_t>(data.blocks.size());
			block.mapX = static_cast<float>(x) * k_BlockSize;
			block.mapZ = static_cast<float>(z) * k_BlockSize;
			block.blockX = static_cast<uint32_t>(x);
			block.blockZ = static_cast<uint32_t>(z);
			data.blockIndexLookup.at(static_cast<size_t>(x * LandData::k_BlocksPerSide + z)) =
			    static_cast<uint16_t>(block.index);
		}
	}
	data.countries = land.countries;
	for (const auto surface : land.materials)
	{
		data.materials.push_back(parts.surfaces.at(static_cast<size_t>(surface)));
	}
	data.noise = parts.noise;
	data.bump = parts.bump;
	return data;
}

landgen::Settings generated_land::SettingsFor(uint64_t seed)
{
	return {.seed = seed};
}
