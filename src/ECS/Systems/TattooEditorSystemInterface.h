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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/TattooEditor.h"

namespace openblack::ecs::systems
{

/// The tattoo editor: the player drags the sixteen symbols onto the places on their creature's body and off them, the
/// tattoos going into the creature's skin as they do, and OK keeps them while Cancel puts back those it had when the
/// editor opened. While it is open it turns the view round the creature, which the camera showing the creature follows.
///
/// The Creature Cave opens it as its camera zooms onto the creature it shows, and zooms back out once it has closed.
class TattooEditorSystemInterface
{
public:
	virtual ~TattooEditorSystemInterface() = default;

	/// Opens the editor on a creature, the player's own, with the view the camera has of it
	virtual void Open(entt::entity creature, const creature_tattoo_editor::Orbit& view) = 0;
	/// Closes it as it is, as when the camera leaves the creature
	virtual void Close() = 0;
	[[nodiscard]] virtual bool IsOpen() const = 0;
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	/// The view of the creature as the editor turns it, for the camera to take while it is open
	[[nodiscard]] virtual const creature_tattoo_editor::Orbit& GetView() const = 0;
	/// The tattoos as they are being edited
	[[nodiscard]] virtual const creature_tattoo_editor::Session& GetSession() const = 0;

	/// Every frame: the view turns and slows
	virtual void Update(float milliseconds) = 0;
	/// The rotation button held down at a point of the dialogs' space since it was pressed at another
	virtual void Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds) = 0;

	/// Which place on the body the pointer, in screen pixels, is over, of the places as the view shows them this frame
	virtual void Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer) = 0;
	[[nodiscard]] virtual std::optional<uint8_t> GetHoveredSite() const = 0;
	/// Where on the screen, in pixels, the place under the pointer is
	[[nodiscard]] virtual std::optional<glm::ivec2> GetHoveredPoint() const = 0;
	/// A symbol let go of, onto the place under the pointer if there is one; true when it went on
	virtual bool Drop(uint8_t design, const glm::u8vec3& colour) = 0;
	/// Lifts the tattoo off the place under the pointer, if it has one
	virtual std::optional<creature_tattoo::Slot> Lift() = 0;

	/// OK: closes the editor, or asks the player to note something first
	virtual creature_tattoo_editor::Accept Ok(bool networkGame) = 0;
	/// The player answered what OK asked
	virtual void Answered() = 0;
	/// Cancel: puts back the tattoos the creature had when the editor opened and closes it
	virtual void Cancel() = 0;
};

} // namespace openblack::ecs::systems
