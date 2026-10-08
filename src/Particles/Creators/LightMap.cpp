/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LightMap.h"

#include <cmath>

#include <algorithm>
#include <memory>
#include <vector>

#include "3D/FrameAnim.h"
#include "3D/LandLight.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::unique_ptr<Creator> MakeLightMapCreator(const Object& object)
{
	auto creator = std::make_unique<LightMapCreator>();
	ReadCreatorProperties(object, *creator);
	// the original's defaults: Pitch 1, 1 frame, FrameRate 1, no animation, no jitter
	creator->pitch = std::max(1, object.Int("Pitch", 1));
	creator->numFramesInFile = std::max(1, object.Int("NumFramesInFile", 1));
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, creator->numFramesInFile);
	creator->randJitter = object.Float("RandJitter", 0.0f);
	creator->useRandJitter = object.Bool("UseRandJitter", false);
	creator->shiftX = object.Float("ShiftX", 0.0f);
	creator->shiftZ = object.Float("ShiftZ", 0.0f);
	// not drawn by the sprite pass: stamped into the land (light_map_atoms::SubmitFrame)
	creator->kind = Creator::Kind::Other;
	creator->texture = object.String("TextureFileName");
	// the bitmap of (TextureFileName, Pitch, 3, NumFramesInFile, NumFramesInUse)
	creator->bitmap =
	    land_light::LoadBitmapFile(creator->texture, creator->pitch, 3, creator->numFramesInFile, creator->numFramesInUse);
	creator->numFrames = creator->numFramesInUse;
	creator->fileOffset = 0;
	creator->initFrame = 0;
	creator->frameRate = object.Float("FrameRate", 1.0f);
	creator->playAnim = object.Bool("PlayAnim", false);
	creator->loopAnim = object.Bool("LoopAnim", false);
	return creator;
}
} // namespace

void LightMapCreator::InitAtom(Effect& /*effect*/, Atom& atom) const
{
	// the atom's frame rate (FrameRate), its frames (NumFramesInUse) and PlayAnim; the frame starts at 0. The light
	// fades through the bitmap's frames (PSys.cpp steps them, FramesPerAtom = numFrames)
	atom.frame = 0.0f;
	atom.frameRate = frameRate;
	atom.playAnim = playAnim;
}

namespace
{
/// What this module keeps between calls (Locator::particleSystem)
struct LightMapAtomsState
{
	/// The stamp frame the atoms were last submitted in
	uint32_t submitted {~0u};
};

LightMapAtomsState& LightMapAtomsData()
{
	return openblack::Locator::particleSystem::value().Module<LightMapAtomsState>();
}
} // namespace

void light_map_atoms::SubmitFrame()
{
	auto& state = LightMapAtomsData();
	// (openblack guard) each atom's record is appended once a frame to the list, which is emptied after the stamps: a
	// second call before this frame's stamps are taken out (land_light::ClearStamps) would stamp every atom twice, so
	// it is skipped
	if (state.submitted == land_light::StampFrame())
	{
		return;
	}
	state.submitted = land_light::StampFrame();
	Stamp(manager::Collect(Creator::Kind::Other));
}

int light_map_atoms::Stamp(const std::vector<manager::Drawable>& drawables)
{
	int stamped = 0;
	for (const auto& drawable : drawables)
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const LightMapCreator*>(atom.creator);
			// a bitmap with data, 3 or 1 bytes per texel
			if (creator == nullptr || !creator->bitmap || (creator->bitmap->channels != 3 && creator->bitmap->channels != 1))
			{
				continue;
			}
			// the frame, the position and alpha = the draw alpha / 255
			const int frame =
			    graphics::frame_anim::ParticleFrameIndex(atom.frame, creator->numFrames, creator->loopAnim) & 0xFF;
			glm::vec3 position = atom.position;
			const float alpha = static_cast<float>(static_cast<int>(std::clamp(atom.alpha, 0.0f, 255.0f))) * (1.0f / 255.0f);
			// with UseRandJitter, LocalFloatRand(RandJitter) three times at every draw; the first goes to z, the second
			// to y and the third to x
			if (creator->useRandJitter)
			{
				const float first = game_random::LocalFloatRand(creator->randJitter);
				const float second = game_random::LocalFloatRand(creator->randJitter);
				const float third = game_random::LocalFloatRand(creator->randJitter);
				position += glm::vec3(third, second, first);
			}
			// the stamp: + (10, 0, 10), the frame frame % frames, centred, mode 1 for 3 bytes per texel and 2 for 1,
			// keepBrighter 0
			const auto* texels = graphics::frame_anim::FrameTexels(*creator->bitmap, frame);
			const int mode = creator->bitmap->channels == 3 ? 1 : 2;
			if (land_light::AddStamp(position + glm::vec3(10.0f, 0.0f, 10.0f), texels, creator->bitmap->pitch, true, alpha,
			                         mode))
			{
				++stamped;
			}
		}
	}
	return stamped;
}

void openblack::psys::RegisterLightMapCreator()
{
	RegisterCreator("ParticleLightMapCreator", MakeLightMapCreator);
}
