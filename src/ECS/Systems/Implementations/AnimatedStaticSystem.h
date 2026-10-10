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
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "Scenery/ObjectDrawList.h"

namespace openblack::ecs::components
{
struct Mesh;
struct Transform;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class AnimatedStaticSystem final: public AnimatedStaticSystemInterface
{
public:
	bool SetOpenState(entt::entity object, int32_t openState) override;
	[[nodiscard]] std::optional<uint32_t> GateStoneValue(entt::entity object) const override;
	bool LayGateStone(entt::entity plinth, entt::entity stone) override;
	[[nodiscard]] std::optional<std::vector<animated_static::RouteCircle>> RouteCircles(entt::entity gate) const override;
	void Update(uint32_t turn, float turnFraction) override;
	void Reset() override;

private:
	/// The game's clock when the scenery was last drawn; none before the first frame of a level
	std::optional<uint32_t> _drawTime;

	/// The land's blocks are 32 by 32 at most
	static constexpr int32_t k_GridBlocks = 32;
	struct ListedBlock
	{
		object_draw_list::Block block;
		bool inView {false};
		/// Its objects are in the draw list as it was last made
		bool listed {false};
	};
	/// The land's blocks as the draw list sees them, read once a level
	void LoadBlocks();
	/// The camera's frame of the draw list: whether the list is made again and which of it is drawn
	[[nodiscard]] object_draw_list::Frame DrawFrame(uint32_t turn);
	/// Whether something standing here is in the draw list as it was last made
	[[nodiscard]] bool InDrawList(glm::vec3 position) const;
	/// Whether something's model shows on the screen this frame
	[[nodiscard]] bool OnScreen(const components::Transform& transform, const components::Mesh& mesh) const;

	object_draw_list::Clock _drawClock;
	std::vector<ListedBlock> _blocks;
	/// Each block's place in the blocks, by its column and row, or -1 for none
	std::array<int32_t, static_cast<size_t>(k_GridBlocks* k_GridBlocks)> _blockGrid {};
	/// Whether any block was listed when the list was last made, which lists the things off the land's blocks
	bool _anyListed {false};
	/// How the camera saw the land this frame; none without a camera or a land
	std::optional<object_draw_list::View> _view;
};

} // namespace openblack::ecs::systems
