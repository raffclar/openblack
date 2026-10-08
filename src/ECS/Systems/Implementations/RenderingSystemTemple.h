/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/Temple.h"
#include "ECS/Systems/TempleDrawPlan.h"
#include "RenderingSystemCommon.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

/// The draw lists of the temple's rooms and the player's hand while the player is inside the temple, made again every
/// frame: which rooms are drawn changes as the camera goes through the doors, and the controls glow under the cursor
class RenderingSystemTemple final: public RenderingSystemCommon
{
public:
	~RenderingSystemTemple();
	void PrepareDraw(bool drawBoundingBox, bool drawFootpaths, bool drawStreams) override;

private:
	void PrepareDrawDescs(bool drawBoundingBox) override;
	void PrepareDrawUploadUniforms(bool drawBoundingBox) override;
	/// Whether every primitive of a mesh is blended by its material
	[[nodiscard]] bool AllBlended(entt::id_type meshId);

	/// The parts and the hand drawn this frame, in the order their instances are written
	std::vector<entt::entity> _drawn;
	/// This frame's parts and their entities, kept to be filled again each frame
	std::vector<temple_draw::Part> _parts;
	std::vector<entt::entity> _partEntities;
	/// How many instances of each mesh this frame's uniforms have written, kept to be filled again each frame
	std::vector<std::pair<entt::id_type, uint32_t>> _written;
	/// Whether each mesh drawn so far has every primitive blended, which doesn't change while the player is inside
	std::unordered_map<entt::id_type, bool> _allBlended;
};
} // namespace openblack::ecs::systems
