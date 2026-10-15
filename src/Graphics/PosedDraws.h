/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::graphics::posed_draws
{

/// How the things of one mesh are placed for a frame
struct MeshPlacers
{
	/// Its things blend over what is behind them, so they are drawn with the blended things
	bool translucent {false};
	/// One or more of its things is posed by itself (an animal, a villager, a creature or posed scenery such as an opened
	/// reward chest), so each of its things is drawn by itself, in its own pose
	bool perEntity {false};
};

/// Whether the pass of the blended things draws all the placed things of a mesh together, each in the model's own pose.
/// A mesh whose things are posed one by one is not drawn this way: each of its things is drawn by itself, in its own
/// pose, and a second draw here would show it again in the model's own pose (an opened reward chest would show closed
/// as well as open).
[[nodiscard]] constexpr bool BlendedPassDrawsAllPlaced(const MeshPlacers& placers) noexcept
{
	return placers.translucent && !placers.perEntity;
}

/// Whether a thing posed by itself is drawn with the blended things (true) or with the solid things (false). A thing
/// fading out in the distance blends by its alpha, and a thing of a translucent mesh blends by its materials. Each
/// posed thing is drawn in one of the two passes only.
[[nodiscard]] constexpr bool PosedThingIsBlended(bool fading, const MeshPlacers& placers) noexcept
{
	return fading || placers.translucent;
}

} // namespace openblack::graphics::posed_draws
