/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple's outside follows its player: a blend of the fifteen temple meshes by size and alignment, laid on the
// land, with its texture blended from evil to neutral to good. Wiki: docs/bw1-notes/magic.md, "The temple's outside".

#define LOCATOR_IMPLEMENTATIONS

#include "TempleExteriorSystem.h"

#include <cstring>

#include <exception>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include <L3DFile.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "Common/Bitmap16B.h"
#include "Common/StringUtils.h"
#include "Common/Zip.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The mesh every temple's own mesh is made from: its shape, its skins and its landscape footprint
constexpr std::string_view k_FirstTemple = "b_first_temple_l3d";
/// A .16b file's header before its texels
constexpr size_t k_BitmapHeaderBytes = 16;

/// Every file of Data/Citadel/OutsideMeshes by its name in lower case
TempleExteriorSystem::Files OutsideFiles()
{
	TempleExteriorSystem::Files files;
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		fileSystem.Iterate(
		    fileSystem.GetPath<filesystem::Path::Citadel>() / "OutsideMeshes", false,
		    [&files](const std::filesystem::path& f) { files.emplace(string_utils::LowerCase(f.filename().string()), f); });
	}
	catch (const std::exception&)
	{
		files.clear(); // no folder: every file is missing
	}
	return files;
}

/// A compressed mesh file (a u32 of its inflated size, then the deflated bytes), read through the byte cache. Null
/// when it is missing or cannot be read
std::unique_ptr<l3d::L3DFile> ReadMeshFile(const TempleExteriorSystem::Files& files, std::string_view name)
{
	const auto found = files.find(fmt::format("{}.zzz", name));
	if (found == files.end())
	{
		return nullptr;
	}
	try
	{
		const auto& bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(), found->second);
		uint32_t size = 0;
		if (bytes.size() < sizeof(size))
		{
			return nullptr;
		}
		std::memcpy(&size, bytes.data(), sizeof(size));
		const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + sizeof(size), bytes.end()), size);
		auto file = std::make_unique<l3d::L3DFile>();
		if (file->Open(inflated) != l3d::L3DResult::Success)
		{
			return nullptr;
		}
		return file;
	}
	catch (const std::exception&)
	{
		return nullptr;
	}
}

/// The texels of a 16 bit image, read through the byte cache, when it has `count` of them
std::optional<Bitmap16B> ReadImage(const TempleExteriorSystem::Files& files, std::string_view name, size_t count)
{
	const auto found = files.find(fmt::format("{}.16b", name));
	if (found == files.end())
	{
		return std::nullopt;
	}
	try
	{
		const auto& bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(), found->second);
		// the header's width and height (its 2nd and 3rd u32) must give that many, and the file must hold them
		uint32_t width = 0;
		uint32_t height = 0;
		if (bytes.size() < k_BitmapHeaderBytes)
		{
			return std::nullopt;
		}
		std::memcpy(&width, bytes.data() + sizeof(uint32_t), sizeof(width));
		std::memcpy(&height, bytes.data() + (2 * sizeof(uint32_t)), sizeof(height));
		if (static_cast<size_t>(width) * height != count || bytes.size() < k_BitmapHeaderBytes + (count * sizeof(uint16_t)))
		{
			return std::nullopt;
		}
		return Bitmap16B(bytes);
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}
} // namespace

void TempleExteriorSystem::Create(entt::entity heart)
{
	// neutral and small, never blended: the first blend is now, before the first turn
	Locator::entitiesRegistry::value().AssignOrReplace<TempleExterior>(heart);
	Blend(heart);
}

bool TempleExteriorSystem::Step(entt::entity heart, float alignmentTarget, float sizeTarget)
{
	auto* exterior = Locator::entitiesRegistry::value().TryGet<TempleExterior>(heart);
	return exterior != nullptr && TempleExteriorMorph::Turn(exterior->look, alignmentTarget, sizeTarget);
}

void TempleExteriorSystem::Blend(entt::entity heart)
{
	using namespace TempleExteriorMorph;
	auto& registry = Locator::entitiesRegistry::value();
	auto* exterior = registry.TryGet<TempleExterior>(heart);
	if (exterior == nullptr || !NeedsBlend(exterior->look))
	{
		return;
	}
	// the look is taken as blended even without the meshes, so that the turns go on as the game's
	const auto look = exterior->look;
	Blended(exterior->look);
	auto* mesh = registry.TryGet<Mesh>(heart);
	if (mesh == nullptr || !Locator::resources::has_value() || !Locator::filesystem::has_value() || !LoadMeshFiles())
	{
		return;
	}

	// the first temple's vertices, each the blend of the four meshes about the look, laid on the land
	_vertices.resize(_firstTemple->GetVertices().size());
	const auto verticesOf = [this](uint32_t size, uint32_t stage) -> const std::vector<l3d::L3DVertex>* {
		const auto& file = _temples.at((size * k_Stages) + stage);
		return file ? &file->GetVertices() : nullptr;
	};
	if (!BlendVertices(Corners(look.size, look.alignment), verticesOf, _vertices))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Temple outside: a temple mesh is not the shape of the first temple's");
		return;
	}
	BakeToLand(land_morph::CurrentAltitude(), affine::Model(registry.Get<const Transform>(heart)), _vertices);

	// the heart's own mesh, made once for its entity's index and changed after
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto name = fmt::format("temple/exterior/{}", entt::to_entity(heart));
	const auto id = entt::hashed_string(name.c_str()).value();
	if (!meshes.Contains(id))
	{
		try
		{
			meshes.Load(id, resources::L3DLoader::FromDynamicFileTag {}, name, *_firstTemple);
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Temple outside {}: {}", name, e.what());
			return;
		}
	}
	auto& own = *meshes.Handle(id);
	own.RebuildSubMeshes(*_firstTemple, _vertices);
	own.UpdateBoundingBox();
	BlendTexture(own, look.alignment, registry.Get<const Temple>(heart).owner);
	mesh->id = id;
	// the partly built temple is cut again from the new mesh; a built one is drawn whole again
	registry.Remove<AbodeConstructionDraw>(heart);
	ecs::abodes::RedrawConstruction(heart);
}

std::optional<TempleExteriorMorph::State> TempleExteriorSystem::GetLook(entt::entity heart) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* exterior = registry.Valid(heart) ? registry.TryGet<const TempleExterior>(heart) : nullptr;
	return exterior != nullptr ? std::optional(exterior->look) : std::nullopt;
}

void TempleExteriorSystem::SnapAlignment(entt::entity heart, float alignmentTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* exterior = registry.Valid(heart) ? registry.TryGet<TempleExterior>(heart) : nullptr;
	if (exterior == nullptr)
	{
		return;
	}
	exterior->look.alignmentTarget = alignmentTarget;
	exterior->look.alignment = alignmentTarget;
	if (!TempleExteriorMorph::NeedsBlend(exterior->look))
	{
		return;
	}
	// as the citadel's turn does: out of the map cells while the mesh changes, then back in
	const bool inMap = ecs::map_cells::IsObjectInMap(heart);
	if (inMap)
	{
		ecs::map_cells::RemoveMapObject(heart);
	}
	Blend(heart);
	if (inMap)
	{
		ecs::map_cells::InsertMapObject(heart);
	}
}

bool TempleExteriorSystem::LoadMeshFiles()
{
	if (_triedLoading)
	{
		return _firstTemple != nullptr;
	}
	_triedLoading = true;
	_files = OutsideFiles();
	auto first = ReadMeshFile(_files, k_FirstTemple);
	bool complete = first != nullptr;
	for (uint32_t size = 0; size < TempleExteriorMorph::k_Sizes; ++size)
	{
		for (uint32_t stage = 0; stage < TempleExteriorMorph::k_Stages; ++stage)
		{
			auto& file = _temples.at((size * TempleExteriorMorph::k_Stages) + stage);
			file = ReadMeshFile(_files, TempleExteriorMorph::MeshName(size, stage));
			complete = complete && file != nullptr;
		}
	}
	if (!complete)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Temple outside: the temple meshes of Data/Citadel/OutsideMeshes could "
		                                         "not all be read; the temples keep the first temple's look");
		return false;
	}
	_firstTemple = std::move(first);
	return true;
}

void TempleExteriorSystem::BlendTexture(graphics::L3DMesh& mesh, float alignment, PlayerNames owner)
{
	using namespace TempleExteriorMorph;
	const auto& skins = _firstTemple->GetSkins();
	if (skins.empty())
	{
		return;
	}
	const auto count = skins.front().texels.size();
	const auto texture = TextureOf(alignment);
	const auto set = static_cast<uint32_t>(owner) & 3u;
	const auto from = ReadImage(_files, ImageName(texture.from, set), count);
	const auto to = ReadImage(_files, ImageName(texture.to, set), count);
	if (!from || !to)
	{
		if (!_textureMissingLogged)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Temple outside: the textures of set {} could not be read", set);
			_textureMissingLogged = true;
		}
		return;
	}
	_texels.resize(count);
	BlendTexels({from->Data(), count}, {to->Data(), count}, texture.weight, _texels);
	mesh.UpdateSkin(skins.front().id, _texels);
}
