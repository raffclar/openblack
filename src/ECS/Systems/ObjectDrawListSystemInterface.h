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

#include <optional>
#include <span>

#include <entt/entity/entity.hpp>

#include "Scenery/ObjectDrawList.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

/// The land's draw list: the things on the land's blocks that are drawn. The blocks in view are walked from near to far
/// up to 700 units off, each adding what stands on it, until the list holds 2999 things; anything else on the land's
/// blocks isn't drawn. The list is made again when a block in view starts or stops to show land triangles, when asked,
/// or after ten turns, so a thing that comes onto the blocks in between waits for it. Things not standing in the map's
/// cells (held, flying in the physics) and those drawn on their own (the temple, things drawn in high detail) aren't the
/// list's to decide.
class ObjectDrawListSystemInterface
{
public:
	/// What the list is like, for the inspector
	struct Summary
	{
		/// The camera and the land are there, so the list decides what is drawn
		bool active {false};
		object_draw_list::Frame frame;
		/// The turn it was last made at
		std::optional<uint32_t> madeAt;
		uint32_t blocksInView {0};
		/// The blocks walked to make it, those in view and within range
		uint32_t blocksWalked {0};
		/// The further of the blocks walked
		float farthestWalked {0.0f};
		uint32_t things {0};
		/// It holds as many as it can, so more were left out
		bool full {false};
		uint32_t rebuildsAsked {0};
	};

	virtual ~ObjectDrawListSystemInterface() = default;

	/// Once a frame, after the camera has moved and before anything is drawn: the list is made again when it is due
	virtual void Update(uint32_t turn) = 0;
	/// Make the list again on the next `frames` frames. This request replaces a request that is not used up.
	///
	/// These events ask for two frames:
	/// - The hand releases the thing that it holds.
	/// - The hand releases a thing that it gripped before the thing came free.
	///
	/// These events ask for one frame:
	/// - A thing in the physics stops, or a hit moves it again.
	/// - The hand pulls a living thing free.
	/// - A player gets a reward. A flock miracle makes an animal.
	/// - A script moves its camera, puts the camera back to the kept details, or releases the camera.
	/// - A script starts or stops the high detail of a thing.
	/// - The high-detail villagers become usual again because the cinema bars go.
	virtual void RequestRebuild(uint32_t frames) = 0;

	/// The frame of the list: whether it was made again and whether all of it is drawn
	[[nodiscard]] virtual object_draw_list::Frame GetFrame() const = 0;
	/// How the camera saw the land this frame; none without a camera or a land
	[[nodiscard]] virtual const std::optional<object_draw_list::View>& GetView() const = 0;
	/// Whether a thing is in the list as it was last made
	[[nodiscard]] virtual bool Listed(entt::entity entity) const = 0;
	/// Whether the list lets a thing be drawn: true for anything it doesn't decide
	[[nodiscard]] virtual bool Drawn(const Registry& registry, entt::entity entity) const = 0;
	/// The list as it was last made, nearest blocks first
	[[nodiscard]] virtual std::span<const entt::entity> GetList() const = 0;
	[[nodiscard]] virtual Summary GetSummary() const = 0;

	/// A new land or scenario: the list is made again on the next frame
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
