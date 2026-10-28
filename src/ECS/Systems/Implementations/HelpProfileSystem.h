/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "Help/HelpProfile.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HelpProfileSystem final: public HelpProfileSystemInterface
{
public:
	void Trigger(uint32_t event) override;
	void Report(uint32_t event) override;
	[[nodiscard]] std::optional<player_commands::HelpReport>
	MakeReport(bool boxShows, const std::optional<help::profile::WorldCameraView>& worldCamera, bool cursorOnPoint) override;
	void DoReport(const player_commands::HelpReport& report) override;
	void ProcessTurn() override;
	[[nodiscard]] const player_commands::HelpReport& GetLastReport() const override { return _lastReport; }
	void LoadRecord(const ProfileFolderSystemInterface& folder, std::u16string_view player) override;
	bool SaveRecord(ProfileFolderSystemInterface& folder, std::u16string_view player) const override;

	[[nodiscard]] const help::profile::HelpProfile& Get() const override { return _profile; }
	[[nodiscard]] help::profile::HelpProfile& Get() override { return _profile; }

private:
	help::profile::HelpProfile _profile;
	help::profile::ReportedEvents _reported;
	/// The last report that this computer made
	player_commands::HelpReport _lastMade {};
	/// The last report that a turn did
	player_commands::HelpReport _lastReport {};
};

} // namespace openblack::ecs::systems
