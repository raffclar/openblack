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

namespace openblack::developer_tools
{

bool NetworkSessionLive()
{
	return Locator::networkGame::has_value() && Locator::networkGame::value().InGame();
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
