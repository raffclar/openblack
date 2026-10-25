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
#include <string_view>

namespace openblack::player_commands
{
struct HelpReport;
} // namespace openblack::player_commands

namespace openblack::help::profile
{
class HelpProfile;
struct WorldCameraView;
} // namespace openblack::help::profile

namespace openblack::ecs::systems
{
class ProfileFolderSystemInterface;

/// The help system's profile of what the player has done, kept for the whole session. Nothing is counted, and its clock
/// stands still, while a script's cut scene has the cinema bars in. A turn also stops while a front-end box shows.
class HelpProfileSystemInterface
{
public:
	virtual ~HelpProfileSystemInterface() = default;

	/// At the turn: the world does a thing of the player that the help counts. The world does it from the commands of the
	/// player, thus the count is the same at all frame rates.
	virtual void Trigger(uint32_t event) = 0;
	/// The input of this computer's player, or what its camera or its hand shows, gives an event. The event waits for
	/// the next report of the help (player_commands::HelpReport). The help counts it at the turn that does the report.
	virtual void Report(uint32_t event) = 0;
	/// The next report of this computer's player: the events that wait, the front-end box, the view of the world camera
	/// and whether the cursor is on a point of the world. The events then go. None when the report has no event and its
	/// other values are those of the last report that this function gave.
	[[nodiscard]] virtual std::optional<player_commands::HelpReport>
	MakeReport(bool boxShows, const std::optional<help::profile::WorldCameraView>& worldCamera, bool cursorOnPoint) = 0;
	/// At the turn: the report of this computer's player. The profile counts its events. It keeps the events of the view
	/// of the world camera and the front-end box for each turn until the next report.
	virtual void DoReport(const player_commands::HelpReport& report) = 0;
	/// A game turn ends. The profile counts the events of the view of the world camera of the last report, and then its
	/// clock moves on. Nothing occurs while the last report has a front-end box (the game's menu, for example). The
	/// report has no view when the player's world camera does not show the world (a script, the temple or a creature has
	/// the camera).
	virtual void ProcessTurn() = 0;
	/// The last report of this computer's player that a turn did. Its events are the events of that report only.
	[[nodiscard]] virtual const player_commands::HelpReport& GetLastReport() const = 0;

	/// The game reads the count of each event from the file of the player in the profile folder. When the file is
	/// missing, the counts stay as they are. The clock does not change.
	virtual void LoadRecord(const ProfileFolderSystemInterface& folder, std::u16string_view player) = 0;
	/// The game writes the count of each event to the file of the player in the profile folder. Returns true when it
	/// wrote the file.
	virtual bool SaveRecord(ProfileFolderSystemInterface& folder, std::u16string_view player) const = 0;

	[[nodiscard]] virtual const help::profile::HelpProfile& Get() const = 0;
	[[nodiscard]] virtual help::profile::HelpProfile& Get() = 0;
};

} // namespace openblack::ecs::systems
