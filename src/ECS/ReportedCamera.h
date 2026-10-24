/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs
{

/// Where the world has the player's camera and the point that it looks at, from the last report that a turn did. The
/// turns read the camera only here, so that all the computers see the same camera. None before the first report.
[[nodiscard]] inline std::optional<systems::HandSystemInterface::ReportedCamera> ReportedPlayerCamera()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetReportedCamera() : std::nullopt;
}

} // namespace openblack::ecs
