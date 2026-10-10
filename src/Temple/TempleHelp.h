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
#include <string_view>

#include "3D/CreatureCaveTargets.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleScrolls.h"

/// What the temple asks of the scripts while the player is inside: the help scripts its rooms, scrolls and places
/// start and stop, and the clock of the turns the temple's scripts get while the world outside stands still. Each help
/// script shows its help only the first time by itself, so the temple starts them every time. Free of state, so it is
/// tested on its own with a fake of the scripts.
namespace openblack::temple_help
{

/// The scripts, as the temple's help starts and stops them
class Scripts
{
public:
	virtual ~Scripts() = default;
	/// Starts a script by its name
	virtual void Start(std::string_view name) = 0;
	/// Every help script stops: the world's, the temple's and a multiplayer game's
	virtual void StopHelp() = 0;
	/// Every running script of a name stops
	virtual void Stop(std::string_view name) = 0;
};

/// The help script a room starts as the player comes into it, none for a room without one. The options room shares
/// the main room's.
[[nodiscard]] std::optional<std::string_view> RoomHelp(TempleRoom room);
/// The help script a scroll starts as the camera is sent to look at it, none for a scroll without one
[[nodiscard]] std::optional<std::string_view> ScrollHelp(TempleScrolls::Content content);
/// The scroll help scripts a room's camera stops as the player takes it away from a scroll
[[nodiscard]] std::span<const std::string_view> ScrollHelpsStoppedIn(TempleRoom room);
/// The help script a place in the creature's room starts as the camera zooms to it, none for the others
[[nodiscard]] std::optional<std::string_view> CaveTargetHelp(CreatureCaveTargets::Target target);

/// The player comes into a room, on the way into the temple or from another room: the help scripts running stop, and
/// the room's own starts while the help system is on
void EnterRoom(Scripts& scripts, TempleRoom room, bool helpSystemOn);
/// The camera is sent to look at a scroll
void LookAtScroll(Scripts& scripts, TempleScrolls::Content content);
/// The player takes the room's camera away from the scroll it looks at
void LeaveScroll(Scripts& scripts, TempleRoom room);
/// The creature's room's camera zooms to one of its places
void ZoomToCaveTarget(Scripts& scripts, CreatureCaveTargets::Target target);
/// The player leaves the temple: the help scripts running stop
void Leave(Scripts& scripts);

/// The temple's scripts take a turn every tenth of a second of real time while the player is inside, whatever the
/// game's speed. A late turn is caught up on the frames after, one a frame, unless it falls more than two turns behind,
/// when the clock starts again from now.
inline constexpr uint32_t k_TurnMilliseconds = 100;
/// Whether a turn of the temple's scripts is due at `now`, in milliseconds of real time; moves on `last`, the time of
/// the last turn, which starts as the first time asked
[[nodiscard]] bool TurnDue(std::optional<uint32_t>& last, uint32_t now);

} // namespace openblack::temple_help
