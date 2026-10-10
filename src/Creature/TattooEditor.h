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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureTattoo.h"

/// The rules of the tattoo editor, where the player drags the sixteen symbols onto the places on their creature's body
/// and off them again, with two colour pickers to colour them and a button to turn the view round the creature.
///
/// Everything here is pure: the tattoos being edited, what dropping and lifting a symbol does to them, the colour the
/// pickers make, which place the pointer is over, and how the view turns.
namespace openblack::creature_tattoo_editor
{
/// The tattoos as the editor changes them, and as they were when it opened, which Cancel puts back
struct Session
{
	creature_tattoo::Slots slots {};
	creature_tattoo::Slots opened {};
	/// Whether the player has put a tattoo on or lifted one off since the editor opened (or since being warned)
	bool changed {false};
};

[[nodiscard]] Session Open(const creature_tattoo::Slots& slots);

/// A symbol let go of over a place on the body, or over none. Over a place it goes in the slot that already holds that
/// symbol there (only its colour changes), else the first empty slot, else the slot holding something else there; when
/// every slot holds another place's tattoo nothing changes. Over a place the session counts as changed either way.
/// True when the symbol went on.
bool Drop(Session& session, uint8_t design, const glm::u8vec3& colour, std::optional<uint8_t> site);

/// Pressing on a place lifts the tattoo there off, the last slot first: the slot is emptied, keeping its symbol, and
/// the tattoo that came off is returned, for the player to drag on to another place or away. None when the place has
/// no tattoo.
std::optional<creature_tattoo::Slot> Lift(Session& session, uint8_t site);

/// What OK does: in a network game, once a change is made, it tells the player that others see the change only once
/// they leave the temple, and answering that clears the change so that OK then closes; otherwise the editor closes
enum class Accept : uint8_t
{
	Close,
	Warn,
};
[[nodiscard]] Accept Ok(const Session& session, bool networkGame);

/// The colour pickers' controls, in the dialogs' 800 by 600 space: the palette at the left and the brightness bar at
/// the right, each 52 wide and 519 high
struct PickerRect
{
	glm::ivec2 min;
	glm::ivec2 max;
};
constexpr PickerRect k_PaletteRect {.min = {5, 35}, .max = {57, 554}};
constexpr PickerRect k_BrightnessRect {.min = {704, 35}, .max = {756, 554}};

/// The palette's column and row under a point of the palette control. They span the whole control, wider than the
/// strip of colours drawn in its middle, and the edges are held to the first and last.
[[nodiscard]] glm::uvec2 PaletteCell(glm::ivec2 point, const PickerRect& rect = k_PaletteRect);

/// How far down a picker a point is, 0 at the top to 1 at the bottom
[[nodiscard]] float SliderPosition(int y, const PickerRect& rect);

/// A place on the body as the view shows it this frame
struct SiteOnScreen
{
	uint8_t site {0};
	/// Where it is on the screen, in pixels
	glm::ivec2 screen {0};
	/// The cosine between the way its surface faces and the way to the camera
	float facingCamera {0.0f};
};

/// A place counts while its surface is turned no further from the camera than a little past side on (a cosine above
/// -0.2), and only within 64 pixels of the pointer
constexpr float k_FacingLimit = -0.2f;
constexpr int k_PickDistanceSquared = 64 * 64;

/// The place nearest the pointer, in screen pixels, of those that count
[[nodiscard]] std::optional<uint8_t> SiteUnderPointer(std::span<const SiteOnScreen> sites, glm::ivec2 pointer);

/// The view of the creature: the camera's eye and the point it looks at, and how fast it is turning round that point
/// and tipping up and down
struct Orbit
{
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	float turnSpeed {0.0f};
	float tipSpeed {0.0f};
};

/// The view the editor has of its own, as from the main menu
constexpr glm::vec3 k_MenuEye {0.0f, 20.0f, -30.0f};
constexpr glm::vec3 k_MenuFocus {0.0f, 7.0f, 0.0f};

/// The Creature Cave's view of its creature as the player goes in: looking at a point half the creature's reach above
/// its feet, from one and a half reaches away and three quarters of one up, the reach being 15 times its scale held
/// between 1 and 20
[[nodiscard]] Orbit CaveView(const glm::vec3& creature, float scale);

/// Holding the rotation button down and moving away from where it was pressed speeds the turning up for as long as it
/// is held: sideways turns it round, up and down tips it, by how far from that point the pointer is
void Steer(Orbit& orbit, glm::ivec2 grab, glm::ivec2 pointer, float milliseconds);

/// Every frame the view turns round its focus and tips up or down by its speeds, staying between a quarter of and as
/// high above the focus as it is far from it across the ground; both speeds die away
void Turn(Orbit& orbit, float milliseconds);
} // namespace openblack::creature_tattoo_editor
