/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include "Graphics/UploadPacer.h"
#include "Loaders.h"
#include "ResourceManager.h"

namespace openblack::resources
{
using MeshManager = ResourceManager<L3DLoader>;
using L3DFileManager = ResourceManager<L3DFileLoader>;
using Bitmap16BManager = ResourceManager<Bitmap16BLoader>;
using LandLightPaletteManager = ResourceManager<LandLightPaletteLoader>;
using PhysicsMaterialsManager = ResourceManager<PhysicsMaterialsLoader>;
using ClipSoundsManager = ResourceManager<ClipSoundsLoader>;
using TextureManager = ResourceManager<Texture2DLoader>;
using AnimationManager = ResourceManager<L3DAnimLoader>;
using LevelManager = ResourceManager<LevelLoader>;
using CreatureMindManager = ResourceManager<CreatureMindLoader>;
using CreatureRigManager = ResourceManager<CreatureRigLoader>;
using CreatureSkinArtManager = ResourceManager<CreatureSkinArtLoader>;
using SoundManager = ResourceManager<SoundLoader>;
using GlowManager = ResourceManager<LightLoader>;
using CameraPathManager = ResourceManager<CameraPathLoader>;
using CameraEditManager = ResourceManager<CameraEditLoader>;
using CameraZoneManager = ResourceManager<CameraZoneLoader>;
using DanceFileManager = ResourceManager<DanceFileLoader>;
using ParticleFileManager = ResourceManager<ParticleFileLoader>;
using GestureTemplatesManager = ResourceManager<GestureTemplatesLoader>;
using VideoManager = ResourceManager<VideoLoader>;
using ParticleBitmapManager = ResourceManager<ParticleBitmapLoader>;
using AdvisorModelManager = ResourceManager<AdvisorModelLoader>;

class ResourcesInterface
{
public:
	virtual MeshManager& GetMeshes() = 0;
	/// Meshes' data, for meshes changed on the CPU
	virtual L3DFileManager& GetL3DFiles() = 0;
	virtual Bitmap16BManager& GetBitmaps() = 0;
	virtual LandLightPaletteManager& GetLandLightPalettes() = 0;
	/// The physics materials, by physics::k_MaterialsId
	virtual PhysicsMaterialsManager& GetPhysicsMaterials() = 0;
	/// The sounds on the people's, animals' and birds' clips, by audio::clip_sounds::k_TableId
	virtual ClipSoundsManager& GetClipSounds() = 0;
	virtual TextureManager& GetTextures() = 0;
	virtual AnimationManager& GetAnimations() = 0;
	virtual LevelManager& GetLevels() = 0;
	virtual CreatureMindManager& GetCreatureMinds() = 0;
	/// What moves each species' body, by creature::GetRigId
	virtual CreatureRigManager& GetCreatureRigs() = 0;
	/// What creatures' tattoos and marks are painted with, by creature_skin::k_ArtId
	virtual CreatureSkinArtManager& GetCreatureSkinArt() = 0;
	virtual SoundManager& GetSounds() = 0;
	virtual GlowManager& GetGlows() = 0;
	virtual CameraPathManager& GetCameraPaths() = 0;
	/// The scripts' numbered cameras and tracks, by camera_edits::k_FileId
	virtual CameraEditManager& GetCameraEdits() = 0;
	/// The lands' camera zones, by their file names
	virtual CameraZoneManager& GetCameraZones() = 0;
	/// The dances' choreographies, by their names in the dances' table
	virtual DanceFileManager& GetDanceFiles() = 0;
	/// The particle effect files, by particles::ParticleFileId of their names
	virtual ParticleFileManager& GetParticleFiles() = 0;
	/// The light maps the particle effects stamp on the land, by their paths
	virtual ParticleBitmapManager& GetParticleBitmaps() = 0;
	/// The templates the hand's drawn gestures are matched against, by gesture::k_TemplatesId
	virtual GestureTemplatesManager& GetGestureTemplates() = 0;

	/// Once a frame: moves what the loading threads have finished into the caches, then hands them more of the
	/// prefetched resources, until about `budget` bytes of them are loading. What they make reaches the graphics card
	/// within `uploads` a frame, so no frame waits long for it.
	virtual void UpdateLoading(size_t budget, graphics::UploadPacer::Allowance uploads) = 0;
	/// Asks for every resource registered and not loaded yet to be loaded on the loading threads
	virtual void PrefetchAll() = 0;
	/// How many registered resources are not loaded yet
	[[nodiscard]] virtual size_t PendingCount() const = 0;
	/// Stops loading on the loading threads: drops what hasn't started and waits for what has
	virtual void StopLoading() = 0;
	/// The videos playing, by their paths; each is let go when it ends
	virtual VideoManager& GetVideos() = 0;
	/// The two advisors, by help::spirits::ModelId
	virtual AdvisorModelManager& GetAdvisorModels() = 0;
};

} // namespace openblack::resources
