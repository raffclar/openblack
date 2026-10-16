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
	/// Opens the editor from the main menu on a creature of its own that it shows, which wears the tattoos as they are
	/// edited. The tattoos start as the player's creature has them, or with none when the player has no creature, and
	/// the player's creature wears them too.
	virtual void OpenFromMenu(std::optional<entt::entity> playerCreature, entt::entity shown,
	                          const creature_tattoo_editor::Orbit& view) = 0;
	/// Whether the editor is open from the main menu
	[[nodiscard]] virtual bool IsFromMenu() const = 0;
	/// The creature the editor shows from the main menu
	[[nodiscard]] virtual std::optional<entt::entity> GetShown() const = 0;
	/// Closes it as it is, as when the camera leaves the creature
	virtual void Close() = 0;
	[[nodiscard]] virtual bool IsOpen() const = 0;
	/// The player's creature, which wears the tattoos; none from the main menu when the player has no creature
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
	/// The face that the creature in the editor pulls after a tattoo goes on or comes off. Each face comes back one time.
	[[nodiscard]] virtual std::optional<creature_face::Face> TakeFace() = 0;

	/// OK: closes the editor, or asks the player to note something first
	virtual creature_tattoo_editor::Accept Ok(bool networkGame) = 0;
	/// The player answered what OK asked
	virtual void Answered() = 0;
	/// Cancel: puts back the tattoos the creature had when the editor opened and closes it
	virtual void Cancel() = 0;
	/// The player's own pictures that take the places of designs. A tattoo that the player drops puts each picture that
	/// the creature's tattoos use into the creature's block of pictures.
	virtual void SetPictures(const creature_tattoo::Pictures& pictures) = 0;
	[[nodiscard]] virtual const creature_tattoo::Pictures& GetPictures() const = 0;
};

} // namespace openblack::ecs::systems
