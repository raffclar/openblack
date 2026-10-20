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

#include "ECS/Systems/CameraBookmarkSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class CameraBookmarkSystem final: public CameraBookmarkSystemInterface
{
public:
	bool Initialize() override;
	void FindAfterLoad() override;
	void Update(std::chrono::milliseconds frameGameTime, bool landDrawn) override;
	void ProcessTurn() override;

	[[nodiscard]] const std::array<entt::entity, camera_bookmarks::k_KeySlots>& GetBookmarks() const override
	{
		return _bookmarks;
	}

	void SetBookmark(size_t slot, glm::vec3 point, std::optional<entt::entity> thing, glm::vec3 cameraOrigin) override;
	void ClearBookmark(size_t slot) override;
	[[nodiscard]] bool IsSet(size_t slot) const override;
	void JumpTo(size_t slot) override;
	void JumpBack() override;
	void JumpToNext() override;
	void JumpToPrevious() override;
	void ToggleFollow(size_t slot) override;
	[[nodiscard]] std::optional<size_t> GetFollowed() const override { return _followed; }
	[[nodiscard]] glm::vec2 GetMarkerHalfSize() const override { return camera_bookmarks::MarkerHalfSize(_pulseClockMs); }
	void SetEnabled(bool enabled) override { _enabled = enabled; }
	[[nodiscard]] bool IsEnabled() const override { return _enabled; }
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

private:
	/// A place that the camera can jump to: a point and the camera's position less the point
	struct View
	{
		glm::vec3 point;
		glm::vec3 cameraOffset;
	};
	/// The view of a set slot, the slot of the go-back key included
	[[nodiscard]] std::optional<View> ViewOf(size_t slot) const;
	/// The Tab keys look at the slots of the number keys
	[[nodiscard]] std::array<bool, camera_bookmarks::k_SteppedSlots> SteppedSet() const;

	std::array<entt::entity, camera_bookmarks::k_KeySlots> _bookmarks {};
	/// The view before the last jump, for the go-back key
	std::optional<View> _back;
	/// The slot of the last bookmark that the player set or jumped to. The Tab keys step from it.
	size_t _current {0};
	std::optional<size_t> _followed;
	int32_t _pulseClockMs {0};
	bool _enabled {true};
};
} // namespace openblack::ecs::systems
