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

#include <map>
#include <string>
#include <string_view>

#include "ResourcesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::resources
{
class Resources final: public ResourcesInterface
{
public:
	[[nodiscard]] MeshManager& GetMeshes() override { return _meshes; }
	[[nodiscard]] L3DFileManager& GetL3DFiles() override { return _l3dFiles; }
	[[nodiscard]] TextureManager& GetTextures() override { return _textures; }
	[[nodiscard]] AnimationManager& GetAnimations() override { return _animations; }
	[[nodiscard]] LevelManager& GetLevels() override { return _levels; }
	[[nodiscard]] CreatureMindManager& GetCreatureMinds() override { return _creatureMinds; }
	[[nodiscard]] CreatureRigManager& GetCreatureRigs() override { return _creatureRigs; }
	[[nodiscard]] CreatureSkinArtManager& GetCreatureSkinArt() override { return _creatureSkinArt; }
	[[nodiscard]] SoundManager& GetSounds() override { return _sounds; }
	[[nodiscard]] GlowManager& GetGlows() override { return _glows; }
	[[nodiscard]] BlobManager& GetBlobs() override { return _blobs; }
	[[nodiscard]] FontManager& GetFonts() override { return _fonts; }
	[[nodiscard]] HelpTextManager& GetHelpTexts() override { return _helpTexts; }
	[[nodiscard]] CameraTrackManager& GetCameraTracks() override { return _cameraTracks; }
	[[nodiscard]] SourceMeshManager& GetSourceMeshes() override { return _sourceMeshes; }
	[[nodiscard]] HdModelManager& GetHdModels() override { return _hdModels; }
	[[nodiscard]] PSysFileManager& GetPSysFiles() override { return _psysFiles; }
	[[nodiscard]] LightBitmapManager& GetLightBitmaps() override { return _lightBitmaps; }
	[[nodiscard]] EnumHeaderManager& GetEnumHeaders() override { return _enumHeaders; }
	[[nodiscard]] TextureStemsManager& GetTextureStems() override { return _textureStems; }
	[[nodiscard]] GestureTemplatesManager& GetGestureTemplates() override { return _gestureTemplates; }
	[[nodiscard]] GestureShapeManager& GetGestureShapes() override { return _gestureShapes; }
	[[nodiscard]] CameraPathManager& GetCameraPaths() override { return _cameraPaths; }
	[[nodiscard]] LandLightPaletteManager& GetLandLightPalettes() override { return _landLightPalettes; }
	[[nodiscard]] uint32_t NextGeneratedMeshNumber(std::string_view family) override
	{
		auto [it, added] = _generatedMeshNumbers.try_emplace(std::string(family), 0);
		return it->second++;
	}

private:
	MeshManager _meshes;
	L3DFileManager _l3dFiles;
	TextureManager _textures;
	AnimationManager _animations;
	LevelManager _levels;
	CreatureMindManager _creatureMinds;
	CreatureRigManager _creatureRigs;
	CreatureSkinArtManager _creatureSkinArt;
	SoundManager _sounds;
	GlowManager _glows;
	BlobManager _blobs;
	FontManager _fonts;
	HelpTextManager _helpTexts;
	CameraTrackManager _cameraTracks;
	SourceMeshManager _sourceMeshes;
	HdModelManager _hdModels;
	PSysFileManager _psysFiles;
	LightBitmapManager _lightBitmaps;
	EnumHeaderManager _enumHeaders;
	TextureStemsManager _textureStems;
	GestureTemplatesManager _gestureTemplates;
	GestureShapeManager _gestureShapes;
	CameraPathManager _cameraPaths;
	LandLightPaletteManager _landLightPalettes;
	std::map<std::string, uint32_t, std::less<>> _generatedMeshNumbers;
};
} // namespace openblack::resources
