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

#include <memory>
#include <string>

#include "3D/FrameAnim.h"
#include "Particles/PSys.h"

// Mist particles: ParticleMistCreator and its draw. Each atom owns a mist (the mist.l3d dome with the smoke atlas, the
// same object as the map mists and the storm puffs); a Sorted effect's (psys::DrawPath) is handed every frame to
// mists::Submit (Graphics/Mists.h), which culls, sorts and draws it with Renderer::DrawMist; a Queued / Immediate
// effect's is drawn inside its effect (mist_atoms::Describe). The water miracle's cloud (SF_Water, SF_WaterPU1, the
// water in the hand and on its holder) and the lightning storm use it. Wiki: docs/bw1-notes/miracles.md, "Water".

namespace openblack::mists
{
struct MistDesc;
}

namespace openblack::psys
{

/// ParticleMistCreator
struct MistCreator: Creator
{
	bool takeRatioFromMatrix {false}; ///< k = M[1][1] / M[0][0] of the draw matrix
	bool isShadowMap {true};          ///< default 1
	bool loadLightMap {true};         ///< default 1: with a TextureFileName, a land light / shadow map too
	std::string lightMap;             ///< TextureFileName
	int pitch {12};
	int numFramesInFile {1};
	int numFramesInUse {1};
	float initialScaleMin {1.0f};
	float ratio {0.0f}; ///< the mist's k; 0 = 2.5 + a random 0..2.5 per mist
	/// With LoadLightMap, the bitmap of TextureFileName (Pitch, IsShadowMap ? 1 : 3 bytes per texel, NumFramesInFile,
	/// NumFramesInUse; land_light::LoadBitmapFile): the land shadow (or light) under each mist
	std::shared_ptr<const graphics::frame_anim::StackedFrames> landBitmap;

	/// The atom's mist (the CRT counter, then k), then the scale,
	/// RandomiseScale ? random(InitialScaleMin, InitialScale) : InitialScale
	void InitAtom(Effect& effect, Atom& atom) const override;
};

namespace mist_atoms
{
/// The draw of every mist atom of the running effects, once per frame (magic::Update): size = the atom's scale,
/// colour = the atom's colour x the land light base colour (alpha x 255 >> 8), the effect branch with k = Ratio, then
/// mists::Submit for the mists of the Sorted effects (their own Z object; every mist while manager::k_DrawByPath is
/// false). `milliseconds` advances the atlas counter.
void SubmitFrame(float milliseconds);
/// The mist the draw sets up for a mist atom (Describe = what SubmitFrame submits), for drawing it inside its
/// effect (a Queued / Immediate one). False when the atom is not a mist's
bool Describe(const Effect::DrawAtom& atom, mists::MistDesc& mist);
/// The draw's colour: per channel (c x base) >> 8, the alpha (a x 0xFF) >> 8 (the base's alpha byte is forced to 0xFF)
[[nodiscard]] uint32_t MistColour(uint32_t atomArgb, uint32_t baseArgb);
} // namespace mist_atoms

} // namespace openblack::psys
