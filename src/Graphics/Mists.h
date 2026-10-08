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

#include <glm/vec3.hpp>

/// Mist objects that are not map mists (e.g. the weather's storm puffs): the caller keeps its own objects and submits
/// them every frame; they are culled, sorted back to front with the map mists and the blended models, and drawn by the
/// same mist draw (Renderer::DrawMist, RendererMists.cpp).
namespace openblack::mists
{
struct MistDesc
{
	glm::vec3 position; ///< the object's position (the dome's centre)
	float size;         ///< the mesh scale
	uint32_t colour;    ///< ARGB; the alpha is colour >> 24, nothing is drawn with 0
	/// The effect flag (set when k != 1): the effect branch, size shrunk to size / (1 + (k - 1)(1 - |dy| / |d|)) along
	/// local Y and Z, lit from straight above with ambient 210, atlas rows 2-3; without it the plain size, the colour
	/// times the land light under it plus the haze, the models' light, atlas rows 0-1
	bool edgeShrink;
	float k;
	int counter; ///< 0..900: the atlas frame is frame_anim::MistCell, (c / 20) & 15; the caller advances it
	             ///< (the draw adds trunc(frame ms * 0.255) only while the object is on screen: InView)
	/// The specular, D3DCOLOR ARGB: the vertices' specular, added to texture x diffuse (D3DRS_SPECULARENABLE). Only the
	/// effect branch keeps it (the normal branch replaces it with the haze); the storm clouds' lightning glow
	/// (UR_CloudGather)
	uint32_t specular {0};
};

/// Draws this mist in the current frame only (call it every frame, before the scene is drawn)
void Submit(const MistDesc& mist);
/// The visibility test that Submit's mists get when they are drawn: the sphere at the position, of radius the mesh's
/// radius x size x 0.55, touches the camera's view. Only such a mist goes to the Z-sorter, so only its Draw advances
/// its animation counter (frame_anim::MistClock): a caller keeping that counter advances it when this is true. True
/// without a camera or a sky (tests). (approximate) the game camera at update time, not the camera of the pass that
/// draws it (the same one in a frame); the radius taken as the mesh's box half diagonal
[[nodiscard]] bool InView(const glm::vec3& position, float size);
} // namespace openblack::mists
