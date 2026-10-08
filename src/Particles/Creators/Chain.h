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

#include <array>
#include <vector>

#include <glm/vec2.hpp>

#include "Particles/PSys.h"

// Chain particles: ParticleChainCreator.
// Every atom of the collection is one joint of a chain; the collection is drawn as a single camera-facing ribbon
// through its joints, textured with S_Lightning.raw / S_Beam.raw. Used by the lightning bolt and the storm lightning
// (Rules/Lightning.cpp), the gesture trail and the creature beam. Wiki: docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// ParticleChainCreator (the chain made from it copies its layout)
struct ChainCreator: Creator
{
	int frameOfHead {0};               ///< the frame of the last repeat
	int frameOfTail {0};               ///< the frame of the first repeat
	int numTexturesForWholeChain {-1}; ///< the chain's repeats; -1: joints - 1
	int frameWidth {32};               ///< texels across the ribbon
	int frameHeight {64};              ///< texels along one repeat
	bool doubleSided {false};          ///< MaterialSetDoubleSided
	bool dynamicLighting {false};      ///< UseDynamicLighting

	/// frame_anim::ChainSegmentUv: the four UVs of segment `index` of a chain of `segments` (joints - 1),
	/// in 0..1 of the 256 x 256 texture: uv0 = (u0, v0), uv1 = (u1, v0) at its first joint, uv2 = (u0, v1),
	/// uv3 = (u1, v1) at the next. U runs across the ribbon (u0 on the joint + side vertices, u1 on the joint - side
	/// ones), V along it; `scroll` is the chain's v-scroll, added to the four v. `textures`: the chain's repeats when a
	/// rule rewrote them (Collection::chainTextures), -1 for the creator's
	[[nodiscard]] std::array<glm::vec2, 4> SegmentUv(int index, int segments, float scroll, int textures = -1) const;
};

namespace chain_atoms
{
/// One ribbon to draw this frame: the joints in order (at least two), with the chain's creator
using Ribbon = Effect::DrawChain;
/// Every chain collection of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Ribbon> Collect();
/// Every drawn frame: each chain's v-scroll += the frame's game ms x its rate x 0.001, kept in one frame height
/// (frame_anim::ChainScroll)
void AdvanceScroll(float milliseconds);
} // namespace chain_atoms

} // namespace openblack::psys
