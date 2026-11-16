/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandGeneratorPanel.h"

#include <array>
#include <string>

#include <imgui.h>

#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "Editor/EditorStyle.h"
#include "Editor/LandGenerator.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::editor;

namespace
{
/// The names of the directions of the wind, after the first entry for the wind that the seed picks
constexpr std::array<const char*, 9> k_WindNames {
    "picked by the seed", "west", "south-west", "south", "south-east", "east", "north-east", "north", "north-west",
};

void DrawChoices(generated_land::Choices& choices)
{
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
	ImGui::InputScalar("seed", ImGuiDataType_U64, &choices.seed);
	ImGui::SameLine();
	if (ImGui::Button("Random seed"))
	{
		choices.seed = land_generator::RandomSeed();
	}
	ImGui::SameLine();
	if (ImGui::Button("First values"))
	{
		choices = {.seed = choices.seed};
	}
	ImGui::SetItemTooltip("Puts each choice other than the seed back to its first value");
	for (const auto& range : generated_land::ChoiceRanges())
	{
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
		const std::string label(range.label);
		auto& value = choices.*range.member;
		if (range.member == &generated_land::Choices::wind)
		{
			const auto index = static_cast<size_t>(value - range.lowest);
			if (ImGui::BeginCombo(label.c_str(), k_WindNames.at(index)))
			{
				for (size_t name = 0; name < k_WindNames.size(); ++name)
				{
					if (ImGui::Selectable(k_WindNames.at(name), name == index))
					{
						value = range.lowest + static_cast<int32_t>(name);
					}
				}
				ImGui::EndCombo();
			}
			continue;
		}
		ImGui::SliderInt(label.c_str(), &value, range.lowest, range.highest, "%d", ImGuiSliderFlags_AlwaysClamp);
	}
}
} // namespace

void LandGeneratorPanel::Draw(EditorContext& context) noexcept
{
	auto& session = context.system.GetLandGenerator();
	const auto landPath = Locator::landSystem::value().GetLandPath().generic_string();
	if (!land_generator::IsOn(session, landPath))
	{
		session.returnSlot.reset();
		ImGui::TextWrapped("The land generator makes a new island from a seed. When it starts, the game saves into "
		                   "an empty save slot, clears the world and makes a land from a random seed.");
		ImGui::TextColored(style::k_Muted, "To go back to the game, click Exit Editor in the menu or the exit button here.");
		if (ImGui::Button("Start the land generator"))
		{
			land_generator::Start(session, land_generator::RandomSeed());
		}
	}
	else
	{
		const auto returnSlot = *session.returnSlot;
		DrawChoices(session.choices);
		if (ImGui::Button("Generate"))
		{
			land_generator::Generate(session);
		}
		ImGui::SetItemTooltip("Clears the world and makes a land from the seed and the choices");
		ImGui::SameLine();
		if (ImGui::Button("Exit the land generator"))
		{
			land_generator::Exit(session);
		}
		ImGui::SetItemTooltip("Loads the save from before the generator started (slot %u)", returnSlot);
		if (const auto open = generated_land::ChoicesOf(landPath); open.has_value())
		{
			ImGui::TextColored(style::k_Muted, "The land that is open has the seed %llu.",
			                   static_cast<unsigned long long>(open->seed));
		}
	}
	if (!session.status.empty())
	{
		ImGui::TextColored(style::k_Muted, "%s", session.status.c_str());
	}
}
