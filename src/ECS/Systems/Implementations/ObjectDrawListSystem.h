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

#include <optional>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>
#include <glm/vec2.hpp>

#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "Scenery/ObjectDrawList.h"

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

	void Update(uint32_t turn) override;
	void RequestRebuild(uint32_t frames) override;
	[[nodiscard]] object_draw_list::Frame GetFrame() const override { return _frame; }
	[[nodiscard]] const std::optional<object_draw_list::View>& GetView() const override { return _view; }
	[[nodiscard]] bool Listed(entt::entity entity) const override;
	[[nodiscard]] bool Drawn(const Registry& registry, entt::entity entity) const override;
	[[nodiscard]] std::span<const entt::entity> GetList() const override { return _list; }
	[[nodiscard]] Summary GetSummary() const override;
	void Reset() override;

private:
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
		/// Its triangles at full detail, three corner indices each
		std::vector<uint16_t> triangles;
	};

	/// The land's blocks as the list sees them, read once a land
	void LoadBlocks();
	/// Whether one or more triangles of a block in view show, with its cells at the heights they have now
	[[nodiscard]] static bool Shows(const LandBlock& block, const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye);
	void MakeList();
	void OnPhysicsEnded(entt::registry& registry, entt::entity entity);

	Registry* _registry {nullptr};
	entt::connection _physicsEnded;

	object_draw_list::Clock _clock;
	object_draw_list::Frame _frame {.rebuilt = true, .drawAll = true};
	std::optional<object_draw_list::View> _view;
	std::vector<LandBlock> _blocks;
	uint32_t _rebuildsAsked {0};
	std::optional<uint32_t> _madeAt;
	uint32_t _blocksWalked {0};
	float _farthestWalked {0.0f};

	std::vector<entt::entity> _list;
	/// Each listed thing by its entity's index, for telling at once whether a thing is listed
	std::vector<entt::entity> _listedByIndex;
};

} // namespace openblack::ecs::systems
