/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "LeftClickCapture.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Lists the vortices on the land with their type, place and state, and fades out or deletes any of them. Creates an
/// In, Out or Volcano vortex under the hand, now or at the next click on the land, and opens a land's way to the next
/// land as its script does. Closed, it reads and changes nothing.
class Vortices final: public Window
{
public:
	Vortices() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;
	[[nodiscard]] bool TakesEvent(const SDL_Event& event) const noexcept override;

private:
	void DrawList() noexcept;
	void DrawCreate() noexcept;
	void DrawExit() noexcept;

	void CreateAt(VortexType type, glm::vec3 point) noexcept;
	void CreateAtHand() noexcept;
	/// The land under the hand, if the hand is on the land
	[[nodiscard]] static std::optional<glm::vec3> HandLandPoint() noexcept;

	VortexType _type {VortexType::In};
	/// The next left click on the land creates the chosen vortex under the hand
	bool _atClick {false};
	/// A click taken, to create at the next update
	bool _clicked {false};
	/// The left press the window took, whose release it takes too
	debug::LeftClickCapture _leftCapture;
	/// What became of the last action
	std::string _last;
};

} // namespace openblack::debug::gui
