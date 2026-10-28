/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AlignmentHistory.h"

#include <cstddef>

#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "ECS/AlignmentHistory.h"
#include "ECS/Components/AlignmentHistory.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{

/// A line is white for a value of 0 or more, and light red for a value less than 0
void Line(const std::string& text, float value)
{
	const auto colour = value < 0.0f ? ImVec4(1.0f, 125.0f / 255.0f, 125.0f / 255.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	ImGui::TextColored(colour, "%s", text.c_str());
}

} // namespace

AlignmentHistory::AlignmentHistory() noexcept
    : Window("Alignment History", ImVec2(260.0f, 220.0f))
{
}

void AlignmentHistory::Draw() noexcept
{
	if (!Locator::alignmentSystem::has_value())
	{
		ImGui::TextUnformatted("No alignment system");
		return;
	}
	const float current = Locator::alignmentSystem::value().GetPlayerAlignment(PlayerNames::PLAYER_ONE);
	if (!_start.has_value())
	{
		_start = current;
	}
	Line(fmt::format("Current :{:.8f}", current), current);
	const float change = current - *_start;
	Line(fmt::format("Change  :{:.8f}", change), change);

	const ecs::components::AlignmentHistory* history = nullptr;
	if (Locator::playerSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		const auto player = Locator::playerSystem::value().FindPlayer(
		    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE));
		if (player.has_value() && registry.Valid(*player))
		{
			history = registry.TryGet<ecs::components::AlignmentHistory>(*player);
		}
	}
	const ecs::components::AlignmentHistory none {};
	const auto& totals = history != nullptr ? *history : none;
	for (std::size_t i = 0; i < ecs::components::k_AlignmentCauseCount; ++i)
	{
		Line(fmt::format("{}:{:.8f}[{}]", ecs::alignment_history::k_Labels.at(i), totals.totals.at(i), totals.counts.at(i)),
		     totals.totals.at(i));
	}
}
