/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Magic window's tab of town miracles: give a town a miracle or take it away, through the same steps as the land's
// scripts. A town without a worship site can then have its site, which goes up for its builders.

#include <algorithm>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include "Debug/TownMiracles.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::debug::gui;
namespace tool = openblack::debug::town_miracles;

namespace
{
[[nodiscard]] std::string TownLabel(const ecs::Registry& registry, entt::entity town)
{
	const auto& component = registry.Get<const ecs::components::Town>(town);
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto tribeName =
	    tribe != nullptr && *tribe != Tribe::NONE ? k_TribeStrs.at(static_cast<size_t>(*tribe)) : std::string_view("none");
	return fmt::format("Town {} ({}, {})", component.id, k_PlayerNamesStrs.at(static_cast<size_t>(component.owner)), tribeName);
}
} // namespace

void Magic::DrawTownMiracles() noexcept
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::spellIconSystem::has_value())
	{
		ImGui::TextUnformatted("No land loaded");
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	if (towns.empty())
	{
		ImGui::TextUnformatted("The land has no towns");
		return;
	}
	// The towns by their numbers
	std::vector<uint32_t> ids;
	for (const auto& [id, entity] : towns)
	{
		ids.push_back(id);
	}
	std::ranges::sort(ids);
	if (!towns.contains(_townId))
	{
		_townId = ids.front();
	}
	const auto town = towns.at(_townId);
	if (!registry.Valid(town))
	{
		ImGui::TextUnformatted("The town is gone");
		return;
	}

	if (ImGui::BeginCombo("Town", TownLabel(registry, town).c_str()))
	{
		for (const auto id : ids)
		{
			const auto entity = towns.at(id);
			if (registry.Valid(entity) && ImGui::Selectable(TownLabel(registry, entity).c_str(), id == _townId))
			{
				_townId = id;
				_townResult.clear();
			}
		}
		ImGui::EndCombo();
	}

	// What the town has now
	const auto& component = registry.Get<const ecs::components::Town>(town);
	std::string held;
	for (const auto type : tool::HeldBy(town))
	{
		held += fmt::format("{}{}", held.empty() ? "" : ", ", tool::MagicName(type));
	}
	ImGui::TextWrapped("Holds: %s", held.empty() ? "nothing" : held.c_str());
	const auto shown = Locator::spellIconSystem::value().CentreMiraclesShown(town);
	if (shown.has_value())
	{
		ImGui::Text("Village centre: %zu of 6 hands show a miracle", *shown);
	}
	else
	{
		ImGui::TextUnformatted("Village centre: none working");
	}
	if (registry.Valid(component.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		const auto* icons = registry.TryGet<const ecs::components::SiteIcons>(component.worshipSite);
		ImGui::Text("Worship site: %s, %zu icons",
		            Locator::worshipSiteSystem::value().IsBuilt(component.worshipSite) ? "built" : "going up",
		            icons != nullptr ? icons->icons.size() : 0);
	}
	else
	{
		ImGui::TextUnformatted("Worship site: none");
	}
	ImGui::Separator();

	if (ImGui::BeginCombo("Miracle", tool::MagicName(_townMiracle).c_str()))
	{
		for (size_t i = 1; i < magic::k_MagicTypeCount; ++i)
		{
			const auto type = static_cast<MagicType>(i);
			if (ImGui::Selectable(fmt::format("{}##{}", tool::MagicName(type), i).c_str(), type == _townMiracle))
			{
				_townMiracle = type;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Checkbox("Give its plain miracle too, as a land file line does", &_withPlain);
	ImGui::Checkbox("Take all its levels, as a creature that steals it does", &_allLevels);
	ImGui::Checkbox("Let a town without a worship site have one", &_openSite);
	ImGui::BeginDisabled(!_openSite);
	ImGui::Checkbox("Also on the first land, which never has worship sites", &_anyLand);
	ImGui::EndDisabled();

	const auto apply = [&](bool give) {
		const auto outcome = tool::Apply({.town = town,
		                                  .type = _townMiracle,
		                                  .give = give,
		                                  .withPlain = _withPlain,
		                                  .allLevels = _allLevels,
		                                  .openSite = _openSite,
		                                  .anyLand = _anyLand});
		if (!outcome.error.empty())
		{
			_townResult = outcome.error;
			return;
		}
		std::string changed;
		for (const auto type : outcome.changed)
		{
			changed += fmt::format("{}{}", changed.empty() ? "" : ", ", tool::MagicName(type));
		}
		_townResult = fmt::format("{}: {}", give ? "Given" : "Taken", changed.empty() ? "nothing" : changed);
		if (outcome.siteIsNew)
		{
			_townResult += ". A new worship site goes up for the town's builders.";
		}
		else if (outcome.noSite.has_value())
		{
			_townResult += fmt::format(". No worship site: {}.", openblack::town_miracle_tool::Describe(*outcome.noSite));
		}
	};
	if (ImGui::Button("Give"))
	{
		apply(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Take away"))
	{
		apply(false);
	}
	if (!_townResult.empty())
	{
		ImGui::TextWrapped("%s", _townResult.c_str());
	}
}
