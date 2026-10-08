/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mist.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <memory>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Common/GameRandom.h"
#include "Graphics/Mists.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::unique_ptr<Creator> MakeMistCreator(const Object& object)
{
	auto creator = std::make_unique<MistCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Other; // drawn by mist_atoms::SubmitFrame, not by the sprite or mesh passes
	// the defaults: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame in the
	// file and in use, InitialScaleMin 1.0, Ratio 0, TextureFileName ""
	creator->takeRatioFromMatrix = object.Bool("TakeRatioFromMatrix", false);
	creator->isShadowMap = object.Bool("IsShadowMap", true);
	creator->loadLightMap = object.Bool("LoadLightMap", true);
	creator->lightMap = object.String("TextureFileName");
	if (creator->lightMap == "NULL_STRING")
	{
		creator->lightMap.clear();
	}
	creator->pitch = std::clamp(object.Int("Pitch", 12), 1, 12);                    // range [1, 12]
	creator->numFramesInFile = std::clamp(object.Int("NumFramesInFile", 1), 1, 32); // range [1, 32]
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, 32);
	creator->initialScaleMin = object.Float("InitialScaleMin", 1.0f);
	creator->ratio = object.Float("Ratio", 0.0f);
	// the bitmap: only with LoadLightMap; bpp 1 with IsShadowMap, else 3.
	// (inferred) the file's Pitch as written, not the [1, 12] property range: SF_LightningStormPush's S_SMClouds16 is
	// Pitch 16 (256 bytes, 16 x 16 x 1)
	if (creator->loadLightMap && !creator->lightMap.empty())
	{
		creator->landBitmap =
		    land_light::LoadBitmapFile(creator->lightMap, std::max(1, object.Int("Pitch", 12)), creator->isShadowMap ? 1 : 3,
		                               creator->numFramesInFile, creator->numFramesInUse);
	}
	return creator;
}
} // namespace

void MistCreator::InitAtom(Effect& effect, Atom& atom) const
{
	// after NewAtom's common part: the mist first. Making it starts its counter at int(Random(0, 16)) & 15 on the CRT
	// rand (game_random::crt), not the particle stream
	atom.mist = {graphics::frame_anim::MistStartCounter(game_random::crt::Random(0.0f, 16.0f)), 0.0f};
	// k = Ratio, or LocalFloatRand(2.5) + 2.5 when it is 0 (or NaN: the original's compare treats it as equal)
	if (ratio == 0.0f || std::isnan(ratio))
	{
		const float r = game_random::LocalFloatRand(2.5f);
		atom.mistK = r + 2.5f;
	}
	else
	{
		atom.mistK = ratio;
	}
	// then RandomiseScale ? random(InitialScaleMin, InitialScale) : InitialScale as the atom's base scale (it replaces
	// the common part's)
	atom.baseScale = randomiseScale ? effect.Random(initialScaleMin, initialScale) : initialScale;
	// frame rate 1.0, 1 frame, PlayAnim 0, LoopAnim the creator's: one frame that never steps
	atom.frame = 0.0f;
	atom.frameRate = 1.0f;
	atom.playAnim = false;
}

uint32_t mist_atoms::MistColour(uint32_t atomArgb, uint32_t baseArgb)
{
	// the base's alpha byte is set to 0xFF, then each channel is (c x g) >> 8
	const auto channel = [&](int shift, uint32_t g) { return ((((atomArgb >> shift) & 0xFFu) * g) >> 8) << shift; };
	return channel(24, 0xFFu) | channel(16, (baseArgb >> 16) & 0xFFu) | channel(8, (baseArgb >> 8) & 0xFFu) |
	       channel(0, baseArgb & 0xFFu);
}

bool mist_atoms::Describe(const Effect::DrawAtom& atom, mists::MistDesc& mist)
{
	const auto* creator = dynamic_cast<const MistCreator*>(atom.creator);
	if (creator == nullptr)
	{
		return false;
	}
	mist = {};
	mist.position = atom.position; // the drawn position
	mist.size = atom.scale;        // the drawn scale
	// set when the mist was made: the effect branch and the k it drew for this mist
	mist.edgeShrink = true;
	mist.k = atom.atom != nullptr ? atom.atom->mistK : creator->ratio;
	// the draw: TakeRatioFromMatrix -> k = M[1][1] / M[0][0] when |M[0][0]| > 0.0001
	// (the draw matrix's Y axis carries the stretch: the storm clouds' cloud ratio, UR_CloudGather)
	const glm::mat3 matrix =
	    atom.rotation * glm::mat3(atom.scale, 0.0f, 0.0f, 0.0f, atom.scale * atom.stretch, 0.0f, 0.0f, 0.0f, atom.scale);
	if (creator->takeRatioFromMatrix && std::abs(matrix[0][0]) > 0.0001f)
	{
		mist.k = matrix[1][1] / matrix[0][0];
	}
	// the draw colour (the atom's, its alpha with the collection's) x the land light table's base colour (the
	// renderer's last Build, land_light::CurrentTable().GetLandColour(): (approximate) the previous frame's)
	const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
	const uint32_t argb = (alpha << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) |
	                      (static_cast<uint32_t>(atom.colour[1]) << 8) | static_cast<uint32_t>(atom.colour[2]);
	mist.colour = MistColour(argb, land_light::CurrentTable().GetLandColour());
	if (atom.atom != nullptr)
	{
		mist.counter = atom.atom->mist.counter;
	}
	// the atom's specular, passed to the mist as is
	mist.specular = atom.specular;
	return true;
}

void mist_atoms::SubmitFrame(float milliseconds)
{
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MistCreator*>(atom.creator);
			if (creator == nullptr)
			{
				continue;
			}
			// every mist its own counter += int(game time step x 0.255), modulo 900 once past it, run only for a mist
			// on screen (mists::InView); the fraction kept as the map mists do (frame_anim::MistAdvance)
			if (atom.atom != nullptr && mists::InView(atom.position, atom.scale))
			{
				graphics::frame_anim::MistAdvance(atom.atom->mist, milliseconds);
			}
			mists::MistDesc mist {};
			Describe(atom, mist);
			// a Sorted effect's mist gets its own Z object: mists::Submit; else it is drawn at once inside its effect's
			// draw (manager::CollectQueued / HandEffects, mist_atoms::Describe). Until the renderer draws by path
			// (manager::k_DrawByPath) every mist still goes to mists::Submit
			if (!manager::k_DrawByPath || drawable.path == DrawPath::Sorted)
			{
				mists::Submit(mist);
			}
			// with the creator's bitmap a land light map record at (x, 0, z), frame 0, alpha = the draw alpha / 255
			// clamped to [0, 1]; placed at + (10, 0, 10), centred, mode 1 for bpp 3 / 2 for bpp 1 (the storm's shadow:
			// land_light::AddStamp)
			if (creator->landBitmap)
			{
				const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
				const int mode = creator->landBitmap->channels == 3 ? 1 : 2;
				land_light::AddStamp(glm::vec3(mist.position.x + 10.0f, 0.0f, mist.position.z + 10.0f),
				                     graphics::frame_anim::FrameTexels(*creator->landBitmap, 0), creator->landBitmap->pitch,
				                     true, static_cast<float>(alpha) * 0.00392157f, mode);
			}
		}
	}
}

void openblack::psys::RegisterMistCreator()
{
	RegisterCreator("ParticleMistCreator", MakeMistCreator);
}
