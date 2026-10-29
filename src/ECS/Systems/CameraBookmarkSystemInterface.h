/*******************************************************************************
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
#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Camera/CameraBookmarks.h"
#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::ecs::systems
{

/// The camera's bookmarks: the number keys keep places on the land or on things, and the camera jumps back to them. A
/// marker on the land shows each set bookmark. The bookmarks are a part of the camera of this computer, so the synced
/// world does not keep them. The saved game keeps them.
class CameraBookmarkSystemInterface: public SavedStateInterface
{
public:
	virtual bool Initialize() = 0;
	/// After the load of a saved world: the slots get new markers. The slots take the bookmarks that an older saved world
	/// keeps among its entities, and these entities go.
	virtual void FindAfterLoad() = 0;
	/// Each frame: the markers throb by the frame's game time, only on a frame that draws the land. Each marker goes
	/// above its place, or above the place where the frame draws its thing.
	virtual void Update(std::chrono::milliseconds frameGameTime, bool landDrawn) = 0;
	/// Each turn: a bookmark on a thing moves to the thing, or forgets the thing if it has gone. The bookmark that the
	/// camera follows flies the camera to it again.
	virtual void ProcessTurn() = 0;
	/// The marker of each slot, in the registry of the things that only the frames make
	[[nodiscard]] virtual const std::array<entt::entity, camera_bookmarks::k_KeySlots>& GetBookmarks() const = 0;
	/// The player keeps the place of the hand, or the thing under the hand, in a slot. The camera's position is kept
	/// from the place. Any follow stops.
	virtual void SetBookmark(size_t slot, glm::vec3 point, std::optional<entt::entity> thing, glm::vec3 cameraOrigin) = 0;
	/// The bookmark and its marker go. A follow of the slot does not stop, but it does nothing.
	virtual void ClearBookmark(size_t slot) = 0;
	[[nodiscard]] virtual bool IsSet(size_t slot) const = 0;
	/// The camera flies to a set bookmark, but not in the temple. The view before the jump goes into the slot of the
	/// go-back key.
	virtual void JumpTo(size_t slot) = 0;
	/// The camera flies back to the view before the last jump. The go-back key thus goes between the two views.
	virtual void JumpBack() = 0;
	/// The camera jumps to the next or the last set bookmark from the slot of the last jump or set
	virtual void JumpToNext() = 0;
	virtual void JumpToPrevious() = 0;
	/// The Tab key: the camera flies to the next town of the player at this machine, to its storage pit or its village
	/// centre. The town key keeps the place of the town, or of the thing under the hand. Any follow stops.
	virtual void FlyToNextTown(std::optional<entt::entity> thingUnderHand) = 0;
	/// Shift and a number key: a bookmark on a thing starts or stops a follow, and a bookmark on the land is a jump
	virtual void ToggleFollow(size_t slot) = 0;
	[[nodiscard]] virtual std::optional<size_t> GetFollowed() const = 0;
	/// The half width and the half height of the markers on the land this frame. All the markers throb together.
	[[nodiscard]] virtual glm::vec2 GetMarkerHalfSize() const = 0;
	/// A script puts the bookmarks away, neither shown nor taken by their keys, or brings them back
	virtual void SetEnabled(bool enabled) = 0;
	[[nodiscard]] virtual bool IsEnabled() const = 0;
};

} // namespace openblack::ecs::systems
