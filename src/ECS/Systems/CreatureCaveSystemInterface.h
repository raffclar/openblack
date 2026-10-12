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

#include <chrono>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/gtc/type_precision.hpp>

#include "3D/ScreenPick.h"
#include "Creature/CreatureCave.h"
#include "Creature/TattooEditor.h"

namespace openblack::gui
{
class GameInterface;
} // namespace openblack::gui

namespace openblack::ecs::systems
{

/// The Creature Cave's screen (see creature_cave): F5 shows the player's own creature's attributes, the actions it has
/// learnt, its mind, the miracles it can cast, and its tattoos to put on and take off. With a temple, F5 goes into the
/// temple's creature room as ever, and the screen stands by its scrolls for the creature the room doesn't show; without
/// one, as on the testbed, the screen alone is the cave. F5 or Escape closes it there. It is always the player's own
/// creature, whichever creature the camera is locked onto.
class CreatureCaveSystemInterface
{
public:
	virtual ~CreatureCaveSystemInterface() = default;

	/// Each frame: reads F5 when there is no temple, and follows the temple into and out of its creature room, where it
	/// shows the player's creature by the fire for as long as the player is in the temple
	virtual void Update(std::chrono::duration<float, std::milli> frameTime) = 0;
	/// The game's texts and font the screen writes with, none while the game has no interface
	virtual void SetInterface(const gui::GameInterface* interface) = 0;
	[[nodiscard]] virtual const gui::GameInterface* GetInterface() const = 0;

	virtual void Open() = 0;
	virtual void Close() = 0;
	[[nodiscard]] virtual bool IsOpen() const = 0;
	/// Whether it is shown by the temple's creature room rather than on its own
	[[nodiscard]] virtual bool InTemple() const = 0;
	/// Escape closes the screen on its own; true when it took the key
	virtual bool Escape() = 0;
	[[nodiscard]] virtual creature_cave::Screen& GetScreen() = 0;

	/// The player's creature the cave is about, and what it knows of it
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	[[nodiscard]] virtual std::optional<creature_cave::Snapshot> Snapshot() const = 0;

	/// Puts a symbol on a place on the creature's body, or takes the symbols off it; false when it can't
	virtual bool ApplyTattoo(uint8_t site, uint8_t design, glm::u8vec3 colour) = 0;
	virtual bool RemoveTattoo(uint8_t site) = 0;

	/// Whether the temple's creature room shows the creature, as a script can say; it does unless told not to, and the
	/// choice counts the next time the player goes into the temple
	virtual void SetCreatureShown(bool shown) = 0;
	[[nodiscard]] virtual bool IsCreatureShown() const = 0;
	/// The copy of the player's creature the temple's creature room shows by its fire (see components::CaveCreature),
	/// none while the player isn't in the temple or has no creature
	[[nodiscard]] virtual std::optional<entt::entity> GetCaveCreature() const = 0;
	/// The camera's view of the copy as it zooms onto it, from in front and above
	[[nodiscard]] virtual std::optional<creature_tattoo_editor::Orbit> GetCaveCreatureView() const = 0;
	/// Whether the cursor is over the copy's body as it is drawn
	[[nodiscard]] virtual bool CursorOverCaveCreature(const screen_pick::View& view) const = 0;
};

} // namespace openblack::ecs::systems
