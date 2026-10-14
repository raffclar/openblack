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

void HelpProfileSystem::ProcessTurn(bool frontEndBoxShows, const std::optional<help::profile::WorldCameraView>& worldCamera)
{
	if (frontEndBoxShows || InScriptCutScene())
	{
		return;
	}
	if (worldCamera.has_value())
	{
		for (const auto event : help::profile::CameraViewEvents(*worldCamera))
		{
			_profile.Trigger(event);
		}
	}
	_profile.EndTurn();
}
