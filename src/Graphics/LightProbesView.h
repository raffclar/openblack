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
#include <memory>
#include <optional>

#include <glm/vec2.hpp>

#include "Graphics/AtmosphereView.h"
#include "Graphics/GlobalIllumination.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): what the renderer keeps for the light in the shade: two grids of light probes (the
/// newest that is done and the one before it, or the one being made), the grid that the scene reads (the old grid
/// blended into the new), and the shade from the scene's depth at half the screen's size. Each grid is one texture of
/// three tiles side by side, one for each colour; each texel holds a probe's four harmonics of that colour.
class LightProbesView
{
public:
	LightProbesView();

	/// The size of each grid's texture
	[[nodiscard]] static glm::u16vec2 Size();

	std::array<std::unique_ptr<OwnedTarget>, 2> grids;
	OwnedTarget blended;
	/// The shade from the scene's depth, made again when the screen's size changes
	std::unique_ptr<OwnedTarget> occlusion;

	global_illumination::ProbeSweep sweep;
	/// The grid being made, and the newest grid that is done
	uint8_t building {0};
	std::optional<uint8_t> newest;
	/// Whether the grid before the newest is done too, and the seconds when the newest was done
	bool olderDone {false};
	float newestAt {0.0f};
	/// The newest grid that the scene's grid shows in full, so that the blend is not drawn again while nothing changes
	std::optional<uint8_t> blendedInFull;
};

} // namespace openblack::graphics
