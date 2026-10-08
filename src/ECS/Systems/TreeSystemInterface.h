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

#include <functional>
#include <vector>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{
enum class TreeDeletion;
} // namespace openblack::ecs

namespace openblack::ecs::systems
{
/// What every tree of the game shares: the frame's brightness they are drawn with and the listeners told before a
/// tree goes (ECS/Trees.h goes through it)
class TreeSystemInterface
{
public:
	using DeletedListener = std::function<void(entt::entity, TreeDeletion)>;

	virtual ~TreeSystemInterface() = default;

	/// 200 (looking against the light) to 255, recomputed once a frame
	[[nodiscard]] virtual uint8_t Brightness() const = 0;
	virtual void SetBrightness(uint8_t brightness) = 0;

	/// Whether the last bend pass left any tree bent (or none has run yet): with nothing bent and no source this frame, the
	/// pass would change nothing
	[[nodiscard]] virtual bool AnyBent() const = 0;
	virtual void SetAnyBent(bool bent) = 0;

	/// The listeners, in the order they were added
	virtual void AddDeletedListener(DeletedListener listener) = 0;
	[[nodiscard]] virtual const std::vector<DeletedListener>& DeletedListeners() const = 0;
};
} // namespace openblack::ecs::systems
