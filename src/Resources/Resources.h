/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <tuple>

#include "ResourcesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::resources
{
class Resources final: public ResourcesInterface
{
public:
	MeshManager& GetMeshes() override { return _meshes; }
	L3DFileManager& GetL3DFiles() override { return _l3dFiles; }
	Bitmap16BManager& GetBitmaps() override { return _bitmaps; }
	LandLightPaletteManager& GetLandLightPalettes() override { return _landLightPalettes; }
	PhysicsMaterialsManager& GetPhysicsMaterials() override { return _physicsMaterials; }
	ClipSoundsManager& GetClipSounds() override { return _clipSounds; }
	TextureManager& GetTextures() override { return _textures; }
	AnimationManager& GetAnimations() override { return _animations; }
	LevelManager& GetLevels() override { return _levels; }
	CreatureMindManager& GetCreatureMinds() override { return _creatureMinds; }
	CreatureRigManager& GetCreatureRigs() override { return _creatureRigs; }
	CreatureSkinArtManager& GetCreatureSkinArt() override { return _creatureSkinArt; }
	SoundManager& GetSounds() override { return _sounds; }
	GlowManager& GetGlows() override { return _glows; }
	CameraPathManager& GetCameraPaths() override { return _cameraPaths; }
	ParticleFileManager& GetParticleFiles() override { return _particleFiles; }
	ParticleBitmapManager& GetParticleBitmaps() override { return _particleBitmaps; }
	GestureTemplatesManager& GetGestureTemplates() override { return _gestureTemplates; }

	void UpdateLoading(size_t budget, size_t uploadBudget) override
	{
		_loadQueue.GetUploadPacer().BeginFrame(uploadBudget);
		ForEachManager([](auto& manager) { manager.Collect(); });
		// What is still loading counts against the budget, so no more is made in a frame than it allows
		size_t loading = 0;
		ForEachManager([&loading](const auto& manager) { loading += manager.LoadingBytes(); });
		ForEachManager([this, budget, &loading](auto& manager) {
			if (loading < budget)
			{
				loading += manager.StartLoads(_loadQueue, budget - loading);
			}
		});
	}

	void PrefetchAll() override
	{
		ForEachManager([](auto& manager) { manager.PrefetchAll(); });
	}

	[[nodiscard]] size_t PendingCount() const override
	{
		size_t pending = 0;
		ForEachManager([&pending](const auto& manager) { pending += manager.PendingCount(); });
		return pending;
	}

	void StopLoading() override { _loadQueue.Cancel(); }

private:
	template <typename Func>
	void ForEachManager(Func func)
	{
		ForEach(*this, func);
	}

	template <typename Func>
	void ForEachManager(Func func) const
	{
		ForEach(*this, func);
	}

	/// Calls func with each cache, of a const Resources or not
	template <typename Self, typename Func>
	static void ForEach(Self& self, Func& func)
	{
		std::apply([&func](auto&... manager) { (func(manager), ...); },
		           std::tie(self._meshes, self._l3dFiles, self._bitmaps, self._landLightPalettes, self._physicsMaterials,
		                    self._clipSounds, self._textures, self._animations, self._levels, self._creatureMinds,
		                    self._creatureRigs, self._creatureSkinArt, self._sounds, self._glows, self._cameraPaths,
		                    self._particleFiles, self._particleBitmaps, self._gestureTemplates));
	}

	MeshManager _meshes;
	L3DFileManager _l3dFiles;
	Bitmap16BManager _bitmaps;
	LandLightPaletteManager _landLightPalettes;
	PhysicsMaterialsManager _physicsMaterials;
	ClipSoundsManager _clipSounds;
	TextureManager _textures;
	AnimationManager _animations;
	LevelManager _levels;
	CreatureMindManager _creatureMinds;
	CreatureRigManager _creatureRigs;
	CreatureSkinArtManager _creatureSkinArt;
	SoundManager _sounds;
	GlowManager _glows;
	CameraPathManager _cameraPaths;
	ParticleFileManager _particleFiles;
	ParticleBitmapManager _particleBitmaps;
	GestureTemplatesManager _gestureTemplates;
	// Last, so its threads stop before the caches they load into go
	LoadQueue _loadQueue {LoadQueue::DefaultThreadCount()};
};
} // namespace openblack::resources
