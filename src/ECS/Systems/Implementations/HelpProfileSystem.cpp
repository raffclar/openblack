/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpProfileSystem.h"

#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "Help/HelpProfileFile.h"
#include "Locator.h"

using namespace openblack::ecs::systems;

namespace
{
/// A script's cut scene: a script task has the cinema bars in
bool InScriptCutScene()
{
	if (!openblack::Locator::cinematicDirectorSystem::has_value())
	{
		return false;
	}
	const auto& director = openblack::Locator::cinematicDirectorSystem::value();
	return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
}
} // namespace

void HelpProfileSystem::Trigger(uint32_t event)
{
	if (!InScriptCutScene())
	{
		_profile.Trigger(event);
	}
}

void HelpProfileSystem::Report(uint32_t event)
{
	_reported.Add(event);
}

std::optional<openblack::player_commands::HelpReport>
HelpProfileSystem::MakeReport(bool boxShows, const std::optional<help::profile::WorldCameraView>& worldCamera,
                              bool cursorOnPoint)
{
	player_commands::HelpReport report {
	    .events = _reported.Take(),
	    .boxShows = boxShows,
	    .cameraShowsWorld = worldCamera.has_value(),
	    .landAtCentre = worldCamera.has_value() && worldCamera->centreDistance.has_value(),
	    .centreDistance = worldCamera.has_value() ? worldCamera->centreDistance.value_or(0.0f) : 0.0f,
	    .pitch = worldCamera.has_value() ? worldCamera->pitch : 0.0f,
	    .cursorOnPoint = cursorOnPoint,
	};
	// The events are new in each report. The other values stay until a report changes them.
	auto unchanged = _lastMade;
	unchanged.events = report.events;
	if (report.events == 0 && report == unchanged)
	{
		return std::nullopt;
	}
	_lastMade = report;
	return report;
}

void HelpProfileSystem::DoReport(const player_commands::HelpReport& report)
{
	for (const auto event : help::profile::EventsOf(report.events))
	{
		Trigger(event);
	}
	_lastReport = report;
	_worldCamera.reset();
	if (report.cameraShowsWorld)
	{
		_worldCamera = help::profile::WorldCameraView {
		    .centreDistance = report.landAtCentre ? std::optional(report.centreDistance) : std::nullopt,
		    .pitch = report.pitch,
		};
	}
}

void HelpProfileSystem::ProcessTurn()
{
	if (_lastReport.boxShows || InScriptCutScene())
	{
		return;
	}
	if (_worldCamera.has_value())
	{
		for (const auto event : help::profile::CameraViewEvents(*_worldCamera))
		{
			_profile.Trigger(event);
		}
	}
	_profile.EndTurn();
}

void HelpProfileSystem::LoadRecord(const ProfileFolderSystemInterface& folder, std::u16string_view player)
{
	if (const auto bytes = folder.ReadPlayerFile(player, help::profile::k_FileName); bytes.has_value())
	{
		help::profile::FromFileBytes(_profile, *bytes);
	}
}

bool HelpProfileSystem::SaveRecord(ProfileFolderSystemInterface& folder, std::u16string_view player) const
{
	return folder.WritePlayerFile(player, help::profile::k_FileName, help::profile::ToFileBytes(_profile));
}
