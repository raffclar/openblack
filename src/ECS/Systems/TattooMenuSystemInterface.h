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

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{

/// The tattoo editor that the main menu opens from its Players page. The editor shows a creature of its own over the
/// menu: the body that the profile's physique file keeps, or a stand-in Giant Ape. The tattoos start as the player's
/// creature has them, and each change goes into that creature too. The creature fidgets now and then, looks at the
/// pointer for a second and a half after a mouse button goes down, and the editor's view of it turns round it.
class TattooMenuSystemInterface
{
public:
	virtual ~TattooMenuSystemInterface() = default;

	/// Whether the Players page shows the Edit Tattoo button: the physique file is there, the scripts let the temple
	/// show the creature and leaving the page does not start the game again
	[[nodiscard]] virtual bool IsButtonShown(bool restartPending) const = 0;

	/// Makes the creature from the profile's physique file, or the stand-in when the file is not there, and opens the editor
	/// on it
	virtual void Open() = 0;
	/// The editor has closed: the creature goes. The view stays as it is for the next time.
	virtual void Close() = 0;
	[[nodiscard]] virtual bool IsOpen() const = 0;

	/// Where the editor's view is on the window, in pixels: its top left corner and its size
	struct Viewport
	{
		glm::vec2 origin {0.0f};
		glm::vec2 size {0.0f};
	};
	/// Every frame while it is open: the creature is posed some milliseconds on, it looks at the pointer while a button
	/// is held, and the editor learns which place on its body the pointer, in the window's pixels, is over
	virtual void Update(float milliseconds, glm::ivec2 pointer, bool buttonHeld, bool leftHeld, const Viewport& viewport) = 0;

	/// What the renderer draws in the editor's view: the copy that is posed, and the camera's view and projection
	struct Shown
	{
		entt::entity copy;
		glm::mat4 view {1.0f};
		glm::mat4 projection {1.0f};
		Viewport viewport;
	};
	[[nodiscard]] virtual std::optional<Shown> GetShown() const = 0;
};

} // namespace openblack::ecs::systems
