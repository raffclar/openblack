/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DeveloperToolsGate.h"

#include <spdlog/spdlog.h>

#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Locator.h"
#include "Online/NetworkSession.h"

namespace openblack::developer_tools
{

bool NetworkSessionLive()
{
	return Locator::networkGame::has_value() && Locator::networkGame::value().InGame();
}

bool LocalPlayerIsStaff()
{
	if (!NetworkSessionLive())
	{
		return false;
	}
	const auto* session = Locator::networkGame::value().Session();
	return session != nullptr && session->GetJoined().has_value() && session->GetJoined()->staff;
}

GuiMode CurrentGuiMode()
{
	return GuiModeOf(k_Built, NetworkSessionLive(), LocalPlayerIsStaff());
}

bool EditorAllowed()
{
	const auto refusal = EditorRefusalOf(k_Built, NetworkSessionLive(), LocalPlayerIsStaff());
	if (refusal.has_value())
	{
		if (auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "The game refuses the editor: {}", *refusal);
		}
		return false;
	}
	return true;
}

bool Allowed(Action action)
{
	const auto refusal = RefusalOf(k_Built, NetworkSessionLive());
	if (refusal.has_value())
	{
		if (auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "The game refuses the {}: {}", NameOf(action), *refusal);
		}
		return false;
	}
	return true;
}

} // namespace openblack::developer_tools
