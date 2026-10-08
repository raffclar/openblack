/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <string_view>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Loaders.h"
#include "ResourceManager.h"

namespace openblack::resources
{
using MeshManager = ResourceManager<L3DLoader>;
using L3DFileManager = ResourceManager<L3DFileLoader>;
using TextureManager = ResourceManager<Texture2DLoader>;
using AnimationManager = ResourceManager<L3DAnimLoader>;
using LevelManager = ResourceManager<LevelLoader>;
using CreatureMindManager = ResourceManager<CreatureMindLoader>;
using CreatureRigManager = ResourceManager<CreatureRigLoader>;
using CreatureSkinArtManager = ResourceManager<CreatureSkinArtLoader>;
using SoundManager = ResourceManager<SoundLoader>;
using GlowManager = ResourceManager<LightLoader>;
using BlobManager = ResourceManager<BlobLoader>;
using FontManager = ResourceManager<GameFontLoader>;
using HelpTextManager = ResourceManager<HelpTextLoader>;
using CameraTrackManager = ResourceManager<CameraTrackLoader>;
using CameraPathManager = ResourceManager<CameraPathLoader>;
using LandLightPaletteManager = ResourceManager<LandLightPaletteLoader>;
using SourceMeshManager = ResourceManager<SourceMeshLoader>;
using HdModelManager = ResourceManager<HdModelLoader>;
using PSysFileManager = ResourceManager<PSysFileLoader>;
using LightBitmapManager = ResourceManager<LightBitmapLoader>;
using EnumHeaderManager = ResourceManager<EnumHeaderLoader>;
using TextureStemsManager = ResourceManager<TextureStemsLoader>;
using GestureTemplatesManager = ResourceManager<GestureTemplatesLoader>;
using GestureShapeManager = ResourceManager<GestureShapeLoader>;

class ResourcesInterface
{
public:
	virtual ~ResourcesInterface() = default;

	[[nodiscard]] virtual MeshManager& GetMeshes() = 0;
	/// Meshes' data, for meshes changed on the CPU
	[[nodiscard]] virtual L3DFileManager& GetL3DFiles() = 0;
	[[nodiscard]] virtual TextureManager& GetTextures() = 0;
	[[nodiscard]] virtual AnimationManager& GetAnimations() = 0;
	[[nodiscard]] virtual LevelManager& GetLevels() = 0;
	[[nodiscard]] virtual CreatureMindManager& GetCreatureMinds() = 0;
	/// What moves each species' body, by creature::GetRigId
	[[nodiscard]] virtual CreatureRigManager& GetCreatureRigs() = 0;
	/// What creatures' tattoos and marks are painted with, by creature_skin::k_ArtId
	[[nodiscard]] virtual CreatureSkinArtManager& GetCreatureSkinArt() = 0;
	[[nodiscard]] virtual SoundManager& GetSounds() = 0;
	[[nodiscard]] virtual GlowManager& GetGlows() = 0;
	[[nodiscard]] virtual BlobManager& GetBlobs() = 0;
	[[nodiscard]] virtual FontManager& GetFonts() = 0;
	[[nodiscard]] virtual HelpTextManager& GetHelpTexts() = 0;
	[[nodiscard]] virtual CameraTrackManager& GetCameraTracks() = 0;
	[[nodiscard]] virtual SourceMeshManager& GetSourceMeshes() = 0;
	[[nodiscard]] virtual HdModelManager& GetHdModels() = 0;
	[[nodiscard]] virtual PSysFileManager& GetPSysFiles() = 0;
	[[nodiscard]] virtual LightBitmapManager& GetLightBitmaps() = 0;
	[[nodiscard]] virtual EnumHeaderManager& GetEnumHeaders() = 0;
	[[nodiscard]] virtual TextureStemsManager& GetTextureStems() = 0;
	[[nodiscard]] virtual GestureTemplatesManager& GetGestureTemplates() = 0;
	[[nodiscard]] virtual GestureShapeManager& GetGestureShapes() = 0;
	[[nodiscard]] virtual CameraPathManager& GetCameraPaths() = 0;
	/// The land light's palette, under LandLightPalette::k_Id
	[[nodiscard]] virtual LandLightPaletteManager& GetLandLightPalettes() = 0;
	/// The next number of a family of generated meshes (their ids are "<name>/<number>"): from 0, one counter per
	/// family, never reset while the meshes stay cached
	[[nodiscard]] virtual uint32_t NextGeneratedMeshNumber(std::string_view family) = 0;
};

/// The game font `base` (a path without extension: <base>.met and <base>.fnt), loaded once into `fonts`; null when its
/// files are missing or bad (the menus and the help text then draw no text)
[[nodiscard]] inline std::shared_ptr<const graphics::GameFont> LoadGameFont(FontManager& fonts,
                                                                            const std::filesystem::path& base)
{
	const auto id = entt::hashed_string(("font/" + base.generic_string()).c_str()).value();
	if (!fonts.Contains(id))
	{
		try
		{
			fonts.Load(id, GameFontLoader::FromDiskTag {}, base);
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Font {}: {}", base.filename().string(), e.what());
			return nullptr;
		}
	}
	return fonts.Handle(id).handle();
}

/// The id a data file's bytes are cached under: the hash of its path
[[nodiscard]] inline entt::id_type BlobId(const std::filesystem::path& path)
{
	return entt::hashed_string(path.generic_string().c_str()).value();
}

/// The bytes of `path`, read once into `blobs` and shared afterwards. Throws when the file cannot be read
[[nodiscard]] inline const std::vector<uint8_t>& LoadBlob(BlobManager& blobs, const std::filesystem::path& path)
{
	const auto id = BlobId(path);
	if (!blobs.Contains(id))
	{
		blobs.Load(id, BlobLoader::FromDiskTag {}, path);
	}
	return *blobs.Handle(id);
}

/// LoadBlob for an optional file: one that cannot be read is cached empty, so it is tried and reported only once
[[nodiscard]] inline const std::vector<uint8_t>& LoadOptionalBlob(BlobManager& blobs, const std::filesystem::path& path,
                                                                  std::string_view what)
{
	const auto id = BlobId(path);
	if (!blobs.Contains(id))
	{
		try
		{
			blobs.Load(id, BlobLoader::FromDiskTag {}, path);
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "No {} ({}): {}", what, path.filename().string(), e.what());
			blobs.Load(id, BlobLoader::FromBufferTag {}, std::vector<uint8_t> {});
		}
	}
	return *blobs.Handle(id);
}

/// Every species' rig from its .cbn file in Data/CTR, under creature::GetRigId, and the skin meshes each rig names,
/// under "creature/skins/<name>" in the L3D file cache. Warns and loads nothing without the creature spec file; a rig
/// that cannot be read is logged and skipped.
void LoadCreatureRigs(ResourcesInterface& resources);

} // namespace openblack::resources
