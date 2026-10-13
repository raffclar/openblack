/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/AllMeshes.h"
#include "3D/AxisAlignedBoundingBox.h"
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
		/// The trees: how many are drawn in each detail this frame, then where the next of each goes
		std::array<uint32_t, 5> treeGroups {};
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
	/// A tree as it is drawn this frame, before the trees are laid out by mesh and detail
	struct PlacedTree
	{
		/// From the most detail down, then fading out, then too far off to be drawn
		enum class Group : uint8_t
		{
			High,
			Standard,
			Low,
			Fading,
			Gone,
		};
		entt::id_type mesh;
		InstanceSlots* slots;
		Group group;
		RenderContext::TreeInstanceData data;
		glm::mat4 box;
	};
	std::vector<PlacedTree> _placedTrees;
	/// The bounding box of each mesh the distance bands have looked at, kept while the draw lists are
	std::unordered_map<entt::id_type, std::optional<AxisAlignedBoundingBox>> _meshBoxes;
	/// The bounding box of a mesh, none when it isn't loaded
	[[nodiscard]] const std::optional<AxisAlignedBoundingBox>& MeshBox(entt::id_type mesh);
	/// The things drawn by the distance bands that are fading out this frame, by how opaque, and those too far off to be
	/// drawn at all: their other draws (a building's part built, a graveyard's graves) follow them
	std::unordered_map<entt::entity, uint8_t> _fadingThings;
	std::unordered_set<entt::entity> _goneThings;
	/// The detail the explorers' boat was last drawn at, which the sea mirrors it at the frame after (kept while it is
	/// too far off to be drawn)
	std::unordered_map<entt::entity, uint8_t> _filmReflectionLevels;
};
} // namespace openblack::ecs::systems
