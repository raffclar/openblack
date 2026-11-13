/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>
#include <glm/vec2.hpp>

#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "Visual/LandDetail.h"

namespace openblack::ecs::systems
{

class ObjectDrawListSystem final: public ObjectDrawListSystemInterface
{
public:
	ObjectDrawListSystem();
	~ObjectDrawListSystem() override;
	ObjectDrawListSystem(const ObjectDrawListSystem&) = delete;
	ObjectDrawListSystem& operator=(const ObjectDrawListSystem&) = delete;
	ObjectDrawListSystem(ObjectDrawListSystem&&) = delete;
	ObjectDrawListSystem& operator=(ObjectDrawListSystem&&) = delete;

	void Update(uint32_t turn, bool landDrawn) override;
	void PrepareLand() override;
	void RequestRebuild(uint32_t frames) override;
	[[nodiscard]] object_draw_list::Frame GetFrame() const override { return _frame; }
	[[nodiscard]] const std::optional<object_draw_list::View>& GetView() const override { return _view; }
	[[nodiscard]] bool Listed(entt::entity entity) const override;
	[[nodiscard]] bool LandCallsOwnDraw(const Registry& registry, entt::entity entity) const override;
	[[nodiscard]] bool Drawn(const Registry& registry, entt::entity entity) const override;
	[[nodiscard]] bool DrawnWith(const Registry& registry, entt::entity entity, const DrawnStores& stores) const override;
	[[nodiscard]] bool Decides(const Registry& registry, entt::entity entity) const override;
	[[nodiscard]] bool NotesKept(entt::entity entity) const override;
	void NoteFullFrame(entt::entity entity, bool staysDrawn) override;
	[[nodiscard]] std::span<const entt::entity> GetList() const override { return _list; }
	[[nodiscard]] Summary GetSummary() const override;
	[[nodiscard]] bool BlockInView(const glm::mat4& viewProjection, float nearPlane,
	                               const object_draw_list::Block& block) override
	{
		return _blockViews.InView(viewProjection, nearPlane, block);
	}
	void Reset() override;

private:
	/// The numbers of the sets of triangles: each mesh, each set of coarser sides and each open-sea rule
	static constexpr size_t k_TriangleSetCount = 3 * 16 * 2;

	struct LandBlock
	{
		object_draw_list::Block block;
		/// Its column and row
		glm::ivec2 at {0};
		/// Its place in the land's blocks
		size_t landIndex {0};
		bool inView {false};
		/// It showed triangles when it was last in view
		bool shown {false};
		/// What its triangles need of each of its cells
		std::array<graphics::land_detail::CellFlags, graphics::land_detail::k_CornerCount> cells {};
		/// Its triangles for each mesh, set of coarser sides and open-sea rule that the land drew it with, three corner
		/// indices each, by the number of the set (see TrianglesOf)
		std::vector<std::pair<uint16_t, std::vector<uint16_t>>> triangleSets;
		/// The place in triangleSets of each set by its number, counted from 1. Zero means that the set is not there yet.
		std::array<uint8_t, k_TriangleSetCount> setPlaces {};
	};

	/// The land's blocks as the list sees them, read once a land
	void LoadBlocks();
	/// The triangles of a block for the mesh, coarser sides and open-sea rule that the land draws it with
	[[nodiscard]] static std::span<const uint16_t> TrianglesOf(LandBlock& block, graphics::land_detail::Mesh mesh,
	                                                           uint8_t coarserSides, bool dropOpenSea);
	/// Whether one or more triangles of a block in view show, as the land draws them in the frame: its mesh, its
	/// stitched sides and its corners at the heights they are drawn at
	[[nodiscard]] static bool Shows(LandBlock& block, const graphics::land_detail::FrameDetail* detail,
	                                const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye);
	void MakeList();
	/// The parts that the draw of each village totem draws with it
	void FindParts();
	void OnLetGo(entt::registry& registry, entt::entity entity);
	/// Whether the list decides if a thing that is not a part of another thing is drawn
	[[nodiscard]] bool DecidesWhole(const Registry& registry, entt::entity entity) const;

	Registry* _registry {nullptr};
	entt::connection _letGo;

	object_draw_list::Clock _clock;
	object_draw_list::Frame _frame {.rebuilt = true, .drawAll = true};
	std::optional<object_draw_list::View> _view;
	std::vector<LandBlock> _blocks;
	object_draw_list::BlockViewMemo _blockViews;
	object_draw_list::RebuildRequests _requests;
	std::optional<uint32_t> _madeAt;
	uint32_t _blocksWalked {0};
	float _farthestWalked {0.0f};

	std::vector<entt::entity> _list;
	/// Each listed thing by its entity's index, for telling at once whether a thing is listed
	std::vector<entt::entity> _listedByIndex;
	/// For each listed thing by its entity's index, whether the last full frame kept it for the frames that do not
	/// draw all of the list
	std::vector<bool> _keptByIndex;
	/// The things that the draw of another thing draws with it
	object_draw_list::Parts _parts;
	/// The owners that a part kept drawn on this full frame
	object_draw_list::KeptParts _keptParts;
};

} // namespace openblack::ecs::systems
