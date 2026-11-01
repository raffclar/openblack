/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Challenges.h"

#include <array>
#include <string>

#include <fmt/format.h>

#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
constexpr std::array<const char*, 4> k_HighlightKinds {"Scroll", "Did you know sign", "Silver scroll", "Gold scroll"};

/// The words of a help text, or nothing with no help system
std::string Words(uint32_t text)
{
	if (!Locator::helpTextSystem::has_value())
	{
		return {};
	}
	return openblack::gui::ToUtf8(Locator::helpTextSystem::value().GetWords(text));
}
} // namespace

Challenges::Challenges() noexcept
    : Window("Challenges", ImVec2(520.0f, 560.0f))
{
}

void Challenges::Draw() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No game");
		return;
	}
	if (ImGui::CollapsingHeader("The player's records of the challenges", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawRecords();
	}
	if (ImGui::CollapsingHeader("The scrolls and signs on the land", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawScrolls();
	}
}

void Challenges::DrawRecords() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto listing = ecs::challenge_records::List(registry);
	ImGui::Text("%d started, %d done", listing.discovered, listing.completed);
	if (!ImGui::BeginTable("records", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		return;
	}
	ImGui::TableSetupColumn("Challenge");
	ImGui::TableSetupColumn("Title");
	ImGui::TableSetupColumn("Success");
	ImGui::TableSetupColumn("Alignment");
	ImGui::TableSetupColumn("Turns");
	ImGui::TableSetupColumn("Reminder script");
	ImGui::TableHeadersRow();
	for (const auto entity : ecs::challenge_records::InOrder(registry))
	{
		const auto& record = registry.Get<const ecs::components::ChallengeRecord>(entity);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%u%s", record.challenge, record.quest ? " (quest)" : "");
		ImGui::TableNextColumn();
		ImGui::TextWrapped("%u %s", record.title, Words(record.title).c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%.2f%s", record.success, ecs::challenge_records::IsDone(record) ? " done" : "");
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", record.alignment);
		ImGui::TableNextColumn();
		const auto done = record.doneTurn.has_value() ? fmt::format(" to {}", *record.doneTurn) : std::string();
		ImGui::Text("%u%s", record.startedTurn, done.c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%s (%zu arguments)", record.reminderScript.c_str(), record.arguments.size());
	}
	ImGui::EndTable();
}

void Challenges::DrawScrolls() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ImGui::BeginTable("scrolls", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		return;
	}
	ImGui::TableSetupColumn("Kind");
	ImGui::TableSetupColumn("Challenge or tip");
	ImGui::TableSetupColumn("Started");
	ImGui::TableSetupColumn("Position");
	ImGui::TableHeadersRow();
	registry.Each<const ecs::components::ScriptHighlight, const ecs::components::Transform>(
	    [](entt::entity, const ecs::components::ScriptHighlight& highlight, const ecs::components::Transform& transform) {
		    const auto kind = static_cast<size_t>(highlight.kind);
		    ImGui::TableNextRow();
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(kind < k_HighlightKinds.size() ? k_HighlightKinds.at(kind) : "?");
		    ImGui::TableNextColumn();
		    ImGui::Text("%u", highlight.scriptId);
		    ImGui::TableNextColumn();
		    ImGui::TextUnformatted(highlight.active ? "yes" : "no");
		    ImGui::TableNextColumn();
		    ImGui::Text("%.1f, %.1f, %.1f", transform.position.x, transform.position.y, transform.position.z);
	    });
	ImGui::EndTable();
}
