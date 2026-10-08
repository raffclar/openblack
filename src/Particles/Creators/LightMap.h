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
#include <vector>

#include "3D/FrameAnim.h"
#include "Particles/PSys.h"
#include "Particles/PSysManager.h"

// Landscape light maps: ParticleLightMapCreator — the bright splash a fireball, a lightning fork, a storm or the beam
// leaves on the ground. Each light map atom's draw puts a record in a list, and every frame each record is stamped into
// the land's cells (land_light::AddStamp): the light in the cells' colour, which the land and the models standing there
// take as their specular. Wiki: docs/bw1-notes/rendering.md.

namespace openblack::psys
{

/// ParticleLightMapCreator
struct LightMapCreator: Creator
{
	int pitch {1}; ///< the side of one square frame, pixels
	int numFramesInFile {1};
	int numFramesInUse {1};
	float randJitter {0.0f}; ///< with UseRandJitter: metres of noise per axis
	bool useRandJitter {false};
	float shiftX {0.0f}; ///< ShiftX / ShiftZ (the draw and the stamp do not read them)
	float shiftZ {0.0f};
	/// The bitmap of (TextureFileName, Pitch, 3, NumFramesInFile, NumFramesInUse) (land_light::LoadBitmapFile); none
	/// when the file is missing or of another size
	std::shared_ptr<const graphics::frame_anim::StackedFrames> bitmap;

	/// The frame animation of a new atom (not a sprite, so PSys.cpp does not set it)
	void InitAtom(Effect& effect, Atom& atom) const override;
};

namespace light_map_atoms
{
/// The draw of every light map atom of this frame, then their stamps: Stamp of manager::Collect, once between two
/// land_light::ClearStamps
void SubmitFrame();
/// One stamp per light map atom of `drawables` (land_light::AddStamp); returns how many went into the list
int Stamp(const std::vector<manager::Drawable>& drawables);
} // namespace light_map_atoms

} // namespace openblack::psys
