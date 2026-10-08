/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "KeyBindingsWindow.h"

#include <string>

#include "Input/GameActionMapInterface.h"
#include "Input/KeyBindings.h"
#include "Locator.h"

using namespace openblack::debug::gui;
using namespace openblack::input;

KeyBindingsWindow::KeyBindingsWindow() noexcept
    : Window("Key Bindings", ImVec2(720.0f, 640.0f))
{
}

void KeyBindingsWindow::Draw() noexcept
{
	if (!openblack::Locator::gameActionSystem::has_value())
	{
		ImGui::TextUnformatted("No action map");
		return;
	}
	const auto& actions = openblack::Locator::gameActionSystem::value();
	// What each action is bound to now (the options screen's defaults until something rebinds them)
	const auto bindings = actions.GetKeyBindings();

	ImGui::TextDisabled("The game's bindings; Held lights up while the action map holds the action");

	for (const auto& [first, second] : FindConflicts(bindings))
	{
		ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "\"%s\" and \"%s\" share a binding", bindings[first].name.data(),
		                   bindings[second].name.data());
	}

	constexpr auto k_TableFlags =
	    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("bindings", 5, k_TableFlags))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Category");
	ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Key");
	ImGui::TableSetupColumn("Mouse");
	ImGui::TableSetupColumn("Held");
	ImGui::TableHeadersRow();
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		const auto& binding = bindings[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(CategoryName(binding.category).data());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(binding.name.data());
		ImGui::TableNextColumn();
		const auto keyName = binding.key.has_value() ? KeyChordName(*binding.key) : std::string("-");
		ImGui::TextUnformatted(keyName.c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(MouseInputName(binding.mouse).data());
		ImGui::TableNextColumn();
		if (actions.GetBindable(binding.action))
		{
			ImGui::TextColored({0.45f, 0.85f, 0.45f, 1.0f}, "held");
		}
		else
		{
			ImGui::TextDisabled("-");
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void KeyBindingsWindow::Update() noexcept {}

void KeyBindingsWindow::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void KeyBindingsWindow::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
