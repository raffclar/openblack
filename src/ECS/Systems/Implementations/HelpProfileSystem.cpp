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

#include <algorithm>

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
	    .cursorOnPoint = cursorOnPoint,
	};
	// The report keeps only the events of the view, so that it changes only when they change
	if (worldCamera.has_value())
	{
		const auto viewEvents = help::profile::CameraViewEvents(*worldCamera);
		const auto has = [&viewEvents](uint32_t event) { return std::ranges::find(viewEvents, event) != viewEvents.end(); };
		report.cameraSeesLand = has(help::profile::k_CameraSeesLand);
		report.cameraNearAndSteep = has(help::profile::k_CameraNearAndSteep);
		report.cameraSeesSky = has(help::profile::k_CameraSeesSky);
	}
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
}

void HelpProfileSystem::ProcessTurn()
{
	if (_lastReport.boxShows || InScriptCutScene())
	{
		return;
	}
	// The events of the view of the camera, in the order that they count
	if (_lastReport.cameraSeesLand)
	{
		_profile.Trigger(help::profile::k_CameraSeesLand);
	}
	if (_lastReport.cameraNearAndSteep)
	{
		_profile.Trigger(help::profile::k_CameraNearAndSteep);
	}
	if (_lastReport.cameraSeesSky)
	{
		_profile.Trigger(help::profile::k_CameraSeesSky);
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
