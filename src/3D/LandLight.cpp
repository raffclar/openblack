/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLight.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "Common/StringUtils.h"
#include "ECS/Systems/RenderFrameSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack
{
namespace
{
constexpr int k_MapCells = 0x200; ///< cells 0..0x1FF along each axis
constexpr int k_MaxStamps = 0xC8;
constexpr int k_ShadowFloor = 0x30; ///< the darkest a shadow stamp makes a cell
constexpr float k_CellScale = 0.1f;

/// What the renderer keeps from frame to frame (Locator::renderFrameSystem)
openblack::ecs::systems::RenderFrameSystemInterface& RenderFrame()
{
	return openblack::Locator::renderFrameSystem::value();
}

/// The colour and the light of a cell's dword
uint32_t ColourOf(uint32_t dword)
{
	return dword | 0xFF000000u;
}
uint32_t LightOf(const LandLightTable& table, uint32_t dword)
{
	return table.GetRaw(dword >> 24u);
}

/// The 4 cells lerped along z (wz) and then along x (wx), the colours and the lights
land_light::Sample Bilinear(const LandLightTable& table, uint32_t c00, uint32_t c01, uint32_t c10, uint32_t c11, int wx, int wz)
{
	const auto mix = [wx, wz](uint32_t a00, uint32_t a01, uint32_t a10, uint32_t a11) {
		return land_light::LerpBytes(land_light::LerpBytes(a00, a01, wz), land_light::LerpBytes(a10, a11, wz), wx);
	};
	land_light::Sample sample;
	// The colours and the lights, each | 0xFF000000
	sample.specular = mix(ColourOf(c00), ColourOf(c01), ColourOf(c10), ColourOf(c11)) | 0xFF000000u;
	sample.diffuse = mix(LightOf(table, c00), LightOf(table, c01), LightOf(table, c10), LightOf(table, c11)) | 0xFF000000u;
	return sample;
}

/// The 4 cells from (ix, iz); false when the cell is off the map or in no block
bool FourCells(const land_light::Cells& cells, glm::ivec2 cell, std::array<uint32_t, 4>& out)
{
	if (!land_light::HasBlock(cells, cell))
	{
		return false;
	}
	// the block's 17 x 17 hold the neighbours z + 1, x + 1 and both
	const std::array<glm::ivec2, 4> offsets = {glm::ivec2(0, 0), glm::ivec2(0, 1), glm::ivec2(1, 0), glm::ivec2(1, 1)};
	for (size_t i = 0; i < offsets.size(); ++i)
	{
		if (!land_light::CellAt(cells, cell + offsets.at(i), out.at(i)))
		{
			return false;
		}
	}
	return true;
}

/// A stamp's cell and weight along one axis: trunc(f), and round(255 - frac 255) at or above 0, round((i - f) 255) and
/// the cell one less below
int CellAndWeight(float f, int& cell)
{
	cell = static_cast<int>(f);
	int weight = 0;
	if (!(f < 0.0f))
	{
		const float fraction = f - static_cast<float>(cell);
		const float scaled = fraction * 255.0f;
		weight = static_cast<int>(std::nearbyint(255.0f - scaled));
	}
	else
	{
		const float fraction = static_cast<float>(cell) - f;
		weight = static_cast<int>(std::nearbyint(fraction * 255.0f));
		--cell;
	}
	return weight & 0xFF;
}
} // namespace

const LandLightTable& land_light::CurrentTable() noexcept
{
	return RenderFrame().GetLandLightTable();
}

uint32_t land_light::FullLight(const LandLightTable& table) noexcept
{
	return table.GetRaw(255);
}

uint32_t land_light::LerpBytes(uint32_t a, uint32_t b, int w) noexcept
{
	uint32_t out = 0;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const auto from = static_cast<int32_t>((a >> shift) & 0xFFu);
		const auto to = static_cast<int32_t>((b >> shift) & 0xFFu);
		// arithmetic shift: floor((to - from) w / 256), what the logical shr and the mask leave in the byte
		out |= static_cast<uint32_t>((from + (((to - from) * w) >> 8)) & 0xFF) << shift;
	}
	return out;
}

const land_light::Cells& land_light::GetCells() noexcept
{
	return RenderFrame().GetLandCells();
}

void land_light::SetCells(Cells cells)
{
	RenderFrame().SetLandCells(std::move(cells));
}

bool land_light::HasBlock(const Cells& cells, glm::ivec2 cell) noexcept
{
	if (cell.x < 0 || cell.y < 0 || cell.x >= k_MapCells || cell.y >= k_MapCells)
	{
		return false;
	}
	return cells.blocks.at(static_cast<size_t>(cell.x >> 4) * 32 + static_cast<size_t>(cell.y >> 4)) != 0;
}

bool land_light::CellAt(const Cells& cells, glm::ivec2 cell, uint32_t& dword) noexcept
{
	if (cell.x < 0 || cell.y < 0 || cell.x >= k_MapCells || cell.y >= k_MapCells)
	{
		return false;
	}
	const glm::ivec2 local = cell - cells.firstCell;
	if (local.x < 0 || local.y < 0 || local.x >= cells.size.x || local.y >= cells.size.y)
	{
		return false;
	}
	const auto index = static_cast<size_t>(local.y) * static_cast<size_t>(cells.size.x) + static_cast<size_t>(local.x);
	if (index >= cells.cells.size() || cells.covered.at(index) == 0)
	{
		return false;
	}
	dword = cells.cells[index];
	return true;
}

std::vector<uint8_t> land_light::Luminosity()
{
	const auto& cells = RenderFrame().GetLandCells();
	std::vector<uint8_t> out(cells.cells.size());
	for (size_t i = 0; i < out.size(); ++i)
	{
		out[i] = static_cast<uint8_t>(cells.cells[i] >> 24u);
	}
	return out;
}

void land_light::SetLuminosity(const std::vector<uint8_t>& luminosity)
{
	RenderFrame().SetLandLuminosity(luminosity);
}

std::vector<uint8_t> land_light::Texels()
{
	const auto& cells = RenderFrame().GetLandCells();
	std::vector<uint8_t> out(cells.cells.size() * 4);
	for (size_t i = 0; i < cells.cells.size(); ++i)
	{
		const uint32_t c = cells.cells[i];
		out[i * 4 + 0] = static_cast<uint8_t>(c >> 16u); // red, byte 2
		out[i * 4 + 1] = static_cast<uint8_t>(c >> 8u);
		out[i * 4 + 2] = static_cast<uint8_t>(c);
		out[i * 4 + 3] = static_cast<uint8_t>(c >> 24u);
	}
	return out;
}

void land_light::BeginFrame(const LandIslandInterface& island, uint32_t generation)
{
	auto& frame = RenderFrame();
	const auto extent = island.GetExtent();
	const auto firstCell = glm::ivec2(glm::floor(extent.minimum * k_CellScale + 0.5f));
	const auto size = glm::ivec2(island.GetCellMap().GetResolution());
	if (const auto& current = frame.GetLandCells();
	    current.island != &island || current.generation != generation || current.firstCell != firstCell || current.size != size)
	{
		Cells cells {
		    .firstCell = firstCell,
		    .size = size,
		    .island = &island,
		    .generation = generation,
		};
		const auto count = static_cast<size_t>(size.x) * static_cast<size_t>(size.y);
		// no block: full light (luminosity 255) and colour 0, like the cell map (LandIsland::CreateCellMap)
		cells.base.assign(count, 0xFF000000u);
		cells.covered.assign(count, 0);
		for (const auto& block : island.GetBlocks())
		{
			const auto& lnd = block.GetLndBlock();
			if (!lnd)
			{
				continue;
			}
			const glm::ivec2 position = block.GetBlockPosition();
			if (position.x >= 0 && position.y >= 0 && position.x < 32 && position.y < 32)
			{
				cells.blocks.at(static_cast<size_t>(position.x) * 32 + static_cast<size_t>(position.y)) = 1;
			}
			for (int x = 0; x <= 16; ++x)
			{
				for (int z = 0; z <= 16; ++z)
				{
					const glm::ivec2 local = position * 16 + glm::ivec2(x, z) - firstCell;
					if (local.x < 0 || local.y < 0 || local.x >= size.x || local.y >= size.y)
					{
						continue;
					}
					const auto& cell = lnd->cells.at(static_cast<size_t>(x) * 17 + static_cast<size_t>(z));
					const auto index =
					    static_cast<size_t>(local.y) * static_cast<size_t>(size.x) + static_cast<size_t>(local.x);
					// the first dword: bytes r, g, b (blue, green, red of the D3DCOLOR), luminosity
					cells.base[index] = static_cast<uint32_t>(cell.r) | static_cast<uint32_t>(cell.g) << 8u |
					                    static_cast<uint32_t>(cell.b) << 16u | static_cast<uint32_t>(cell.luminosity) << 24u;
					cells.covered[index] = 1;
				}
			}
		}
		frame.SetLandCells(std::move(cells));
	}
	frame.RestoreLandCells();
}

land_light::Sample land_light::At(const Cells& cells, const LandLightTable& table, glm::vec2 xz) noexcept
{
	// x 0.1 and z 0.1, truncated
	const float fx = xz.x * k_CellScale;
	const float fz = xz.y * k_CellScale;
	const glm::ivec2 cell(static_cast<int>(fx), static_cast<int>(fz));
	std::array<uint32_t, 4> c {};
	if (!FourCells(cells, cell, c))
	{
		return {FullLight(table), 0xFF000000u};
	}
	// trunc((f - trunc(f)) x 256)
	const int wx = static_cast<int>((fx - static_cast<float>(cell.x)) * 256.0f);
	const int wz = static_cast<int>((fz - static_cast<float>(cell.y)) * 256.0f);
	return Bilinear(table, c[0], c[1], c[2], c[3], wx, wz);
}

land_light::Sample land_light::At(const LandLightTable& table, glm::vec2 xz) noexcept
{
	return At(RenderFrame().GetLandCells(), table, xz);
}

land_light::Sample land_light::AtCellShift(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept
{
	std::array<uint32_t, 4> c {};
	if (!FourCells(cells, cell, c))
	{
		return {FullLight(table), 0xFF000000u};
	}
	// The weights are CellX >> 8 and CellZ >> 8
	return Bilinear(table, c[0], c[1], c[2], c[3], cell.x >> 8, cell.y >> 8);
}

land_light::Sample land_light::AtCell(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept
{
	uint32_t dword = 0;
	if (!HasBlock(cells, cell) || !CellAt(cells, cell, dword))
	{
		return {FullLight(table), 0xFF000000u};
	}
	return {LightOf(table, dword), ColourOf(dword)};
}

bool land_light::AddStamp(const glm::vec3& position, const uint8_t* texels, int pitch, bool centre, float alpha, int mode,
                          int keepBrighter)
{
	auto& frame = RenderFrame();
	if (static_cast<int>(frame.GetLandStamps().size()) >= k_MaxStamps)
	{
		return false;
	}
	Stamp stamp;
	// alpha x 255, clamped (below 0 -> 0; above 255 -> 255), truncated
	float scaled = alpha * 255.0f;
	if (scaled < 0.0f)
	{
		scaled = 0.0f;
	}
	else if (scaled > 255.0f)
	{
		scaled = 255.0f;
	}
	stamp.alpha = static_cast<int>(scaled);
	stamp.position = position;
	stamp.texels = texels;
	stamp.pitch = pitch;
	stamp.mode = mode;
	stamp.keepBrighter = keepBrighter;
	if (centre)
	{
		// (pitch - 1) x 5 off x and z
		const float offset = static_cast<float>(pitch - 1) * 5.0f;
		stamp.position.x -= offset;
		stamp.position.z -= offset;
	}
	frame.AddLandStamp(stamp);
	return true;
}

void land_light::ApplyStamp(const Stamp& stamp, glm::ivec2 firstCell, glm::ivec2 size, const std::vector<uint8_t>& covered,
                            std::vector<uint32_t>& cells)
{
	// The bpp per mode (modes 1..8: 3, 1, 1, 1, 1, 1, 1, 4); only 1 and 2 stamp
	if ((stamp.mode != 1 && stamp.mode != 2) || stamp.texels == nullptr || stamp.pitch < 2)
	{
		return;
	}
	const int bpp = stamp.mode == 1 ? 3 : 1;
	int ix = 0;
	int iz = 0;
	const int wz = CellAndWeight(stamp.position.z * k_CellScale, iz);
	const int wx = CellAndWeight(stamp.position.x * k_CellScale, ix);
	// Clipped to the 0x200 cells; the stamp covers pitch - 1 cells each way
	const int pitch = stamp.pitch;
	const int count = pitch - 1;
	const auto* texels = stamp.texels;
	const auto texel = [texels, pitch, bpp](int i, int j, int c) {
		return static_cast<int>(
		    texels[(static_cast<size_t>(i) * static_cast<size_t>(pitch) + static_cast<size_t>(j)) * static_cast<size_t>(bpp) +
		           static_cast<size_t>(c)]);
	};
	const auto lerp = [](int a, int b, int w) { return a + (((b - a) * w) >> 8); };
	for (int i = 0; i < count; ++i)
	{
		const int cx = ix + i;
		if (cx < 0 || cx >= k_MapCells)
		{
			continue;
		}
		for (int j = 0; j < count; ++j)
		{
			const int cz = iz + j;
			if (cz < 0 || cz >= k_MapCells)
			{
				continue;
			}
			const glm::ivec2 local = glm::ivec2(cx, cz) - firstCell;
			if (local.x < 0 || local.y < 0 || local.x >= size.x || local.y >= size.y)
			{
				continue;
			}
			const auto index = static_cast<size_t>(local.y) * static_cast<size_t>(size.x) + static_cast<size_t>(local.x);
			if (index >= cells.size() || covered.at(index) == 0)
			{
				continue;
			}
			auto& cell = cells[index];
			const auto bilinear = [&](int c) {
				const int r1 = lerp(texel(i, j, c), texel(i, j + 1, c), wz);
				const int r2 = lerp(texel(i + 1, j, c), texel(i + 1, j + 1, c), wz);
				return lerp(r1, r2, wx) & 0xFF;
			};
			if (stamp.mode == 2)
			{
				// Shadow: darkens the luminosity, never below the floor
				const int r = bilinear(0);
				int v = 255 - static_cast<int>((static_cast<uint32_t>(255 - r) * static_cast<uint32_t>(stamp.alpha)) / 255u);
				v = std::max(v, k_ShadowFloor);
				const auto luminosity = static_cast<int>(cell >> 24u);
				if (v < luminosity)
				{
					cell = (cell & 0x00FFFFFFu) | static_cast<uint32_t>(v) << 24u;
				}
				continue;
			}
			// Light: texel byte c into the cell's byte 2 - c
			for (int c = 0; c < 3; ++c)
			{
				const int r = bilinear(c);
				const auto v = static_cast<int>((static_cast<uint32_t>(r) * static_cast<uint32_t>(stamp.alpha)) / 255u);
				const uint32_t shift = static_cast<uint32_t>(2 - c) * 8u;
				const auto old = static_cast<int>((cell >> shift) & 0xFFu);
				const int value = stamp.keepBrighter != 0 ? std::max(old, v) : std::min(old + v, 255);
				cell = (cell & ~(0xFFu << shift)) | static_cast<uint32_t>(value) << shift;
			}
		}
	}
}

void land_light::ApplyStamps()
{
	RenderFrame().ApplyLandStamps();
}

void land_light::ClearStamps() noexcept
{
	RenderFrame().ClearLandStamps();
}

uint32_t land_light::StampFrame() noexcept
{
	return RenderFrame().GetLandStampFrame();
}

const std::vector<land_light::Stamp>& land_light::GetStamps() noexcept
{
	return RenderFrame().GetLandStamps();
}

std::shared_ptr<const graphics::frame_anim::StackedFrames>
land_light::LoadBitmapFile(const std::string& path, int pitch, int bpp, int framesInFile, int framesInUse)
{
	if (!Locator::filesystem::has_value() || pitch <= 0 || framesInFile <= 0)
	{
		return nullptr;
	}
	auto name = path;
	std::replace(name.begin(), name.end(), '\\', '/');
	if (name.starts_with("./"))
	{
		name = name.substr(2);
	}
	if (string_utils::LowerCase(name).starts_with("data/"))
	{
		name = name.substr(5);
	}
	const auto key = string_utils::LowerCase(name) + "/" + std::to_string(pitch) + "/" + std::to_string(bpp) + "/" +
	                 std::to_string(framesInFile) + "/" + std::to_string(framesInUse);
	// once into the light bitmap cache; one that cannot be read is cached as null, so it is tried once
	auto& bitmaps = Locator::resources::value().GetLightBitmaps();
	const auto id = entt::hashed_string(("lightbitmap/" + key).c_str()).value();
	if (!bitmaps.Contains(id))
	{
		bitmaps.Load(id, resources::LightBitmapLoader::FromDiskTag {}, name, pitch, bpp, framesInFile, framesInUse);
	}
	return bitmaps.Handle(id).handle();
}

std::shared_ptr<graphics::frame_anim::StackedFrames> land_light::ReadBitmapFile(const std::string& name, int pitch, int bpp,
                                                                                int framesInFile, int framesInUse)
{
	std::shared_ptr<graphics::frame_anim::StackedFrames> bitmap;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& bytes =
		    resources::LoadBlob(Locator::resources::value().GetBlobs(), fileSystem.GetPath<filesystem::Path::Data>() / name);
		if (auto loaded = graphics::frame_anim::LoadBitmapFromFile(bytes, pitch, bpp, framesInFile, framesInUse))
		{
			bitmap = std::make_shared<graphics::frame_anim::StackedFrames>(std::move(*loaded));
		}
		else
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Land light bitmap {}: {} bytes, {} x {} x {} x {} expected", name,
			                   bytes.size(), pitch, pitch, bpp, framesInFile);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Land light bitmap {}: {}", name, e.what());
	}
	return bitmap;
}

} // namespace openblack
