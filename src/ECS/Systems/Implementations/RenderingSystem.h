/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/AllMeshes.h"
#include "RenderingSystemCommon.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class RenderingSystem final: public RenderingSystemCommon
{
public:
	~RenderingSystem();

private:
	void PrepareDrawDescs(bool drawBoundingBox) override;
	void PrepareDrawUploadUniforms(bool drawBoundingBox) override;
	bool UploadUniformsKeepingDescs(bool drawBoundingBox) override;
	void PrepareTreeDrawDescs(bool drawBoundingBox);
	/// Each returns false when an instance had no room left in its mesh's draw list, which must then be made again
	bool UploadInstances(bool drawBoundingBox);
	/// The broken and unfinished buildings drawn as far up as they stand
	void UploadPartialBuilds();
	/// What the villagers drawn this frame hold, a draw for each model, from the held things gathered with the instances
	void UploadHeldObjects();
	/// The souls of the dead rising, each with its own bones
	void UploadSouls();
	bool UploadTreeInstances(bool drawBoundingBox);

	/// Where the next instance of a mesh goes in the instance buffer
	struct InstanceSlots
	{
		uint32_t offset;
		uint32_t count;
		uint32_t filled;
		bool perEntity;
		/// The height of the mesh's bounding box, for the trees
		float height;
		/// The instances are villagers whose bones go into the bone palette
		bool bonePalette {false};
		/// How many, from the last back, are of things too far off to be drawn this frame
		uint32_t gone {0};
	};
	std::unordered_map<entt::id_type, InstanceSlots> _instanceSlots;
	std::unordered_map<entt::id_type, InstanceSlots> _treeSlots;
	/// The things fading out in the distance, by the mesh they are drawn as
	std::unordered_map<entt::id_type, InstanceSlots> _fadingSlots;
	/// The first villager ever seen too far away to be drawn sets the size of every far villager's smudge
	std::optional<float> _farSmudgeScale;
	/// A thing a villager drawn this frame holds, gathered with the instances and drawn with the others of its model
	struct HeldThing
	{
		entt::id_type meshId;
		bool fading;
		bool reflected;
		RenderContext::ObjectInstance instance;
	};
	std::vector<HeldThing> _heldThings;
	/// The things drawn by the distance bands that are fading out this frame, by how opaque, and those too far off to be
	/// drawn at all: their other draws (a building's part built, a graveyard's graves) follow them
	std::unordered_map<entt::entity, uint8_t> _fadingThings;
	std::unordered_set<entt::entity> _goneThings;
};
} // namespace openblack::ecs::systems
