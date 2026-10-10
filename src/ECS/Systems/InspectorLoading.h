/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include "ECS/Systems/InspectorSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::systems
{

/// While something loads (a land, the testbed, a testbed scenario laying out its things) the inspector answers that the
/// game is loading, so that tools wait for it rather than read a land half made; loads within loads count as one
class InspectorLoading
{
public:
	explicit InspectorLoading(std::string_view what)
	{
		if (Locator::inspector::has_value())
		{
			Locator::inspector::value().BeginLoading(what);
		}
	}
	~InspectorLoading()
	{
		if (Locator::inspector::has_value())
		{
			Locator::inspector::value().EndLoading();
		}
	}
	InspectorLoading(const InspectorLoading&) = delete;
	InspectorLoading& operator=(const InspectorLoading&) = delete;
	InspectorLoading(InspectorLoading&&) = delete;
	InspectorLoading& operator=(InspectorLoading&&) = delete;
};

} // namespace openblack::ecs::systems
