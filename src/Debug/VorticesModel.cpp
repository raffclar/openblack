/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VorticesModel.h"

#include <algorithm>
#include <iterator>

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::debug;
using namespace openblack::debug::vortices;

std::string_view vortices::TypeName(VortexType type)
{
	switch (type)
	{
	case VortexType::In:
		return "In";
	case VortexType::Out:
		return "Out";
	case VortexType::Volcano:
		return "Volcano";
	}
	return "unknown";
}

std::string_view vortices::StateName(VortexStateType state)
{
	switch (state)
	{
	case VortexStateType::Inactive:
		return "inactive";
	case VortexStateType::Active:
		return "active";
	case VortexStateType::FadeIn:
		return "fading in";
	case VortexStateType::FadeOut:
		return "fading out";
	}
	return "unknown";
}

std::vector<Row> vortices::Rows(std::span<const VortexAt> vortices)
{
	std::vector<Row> rows;
	rows.reserve(vortices.size());
	std::ranges::transform(vortices, std::back_inserter(rows), [](const VortexAt& vortex) {
		return Row {
		    .entity = vortex.entity,
		    .type = TypeName(vortex.type),
		    .state = StateName(vortex.state),
		    .position = vortex.position,
		    .canFadeOut = vortex.state != VortexStateType::FadeOut,
		};
	});
	std::ranges::sort(rows, {}, [](const Row& row) { return entt::to_integral(row.entity); });
	return rows;
}

std::string vortices::CreatedMessage(VortexType type, glm::vec3 position, entt::entity made)
{
	if (made == entt::null)
	{
		return fmt::format("No {} vortex was made", TypeName(type));
	}
	return fmt::format("{} vortex made at ({:.0f}, {:.0f})", TypeName(type), position.x, position.z);
}
