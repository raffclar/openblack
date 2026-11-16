/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePhysiology.h"
#include "CreatureSpawner.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "Editor/EditorCreatureActs.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureMorph;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{
/// Food is dropped this far in front of the creature for its size
constexpr float k_FoodAhead = 6.0f;

constexpr float k_BarWidth = 160.0f;

std::string_view RestName(CreatureNeeds::Rest rest)
{
	constexpr std::array<std::string_view, 4> k_Names {"awake", "asleep", "resting", "out cold"};
	return k_Names.at(static_cast<size_t>(rest));
}

/// What the player did with the slider of a bar in this frame
struct BarEdit
{
	bool changed {false};
	/// The player let go of the slider after a change
	bool released {false};
};

/// A bar of a value from low to high, with a slider beside it to set it
BarEdit Bar(const char* label, float& value, float low, float high, const char* tooltip)
{
	const auto fraction = high > low ? (value - low) / (high - low) : 0.0f;
	ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(k_BarWidth, 0.0f), fmt::format("{:.3f}", value).c_str());
	ImGui::SameLine();
	ImGui::PushID(label);
	ImGui::SetNextItemWidth(90.0f);
	BarEdit edit;
	edit.changed = ImGui::SliderFloat("##set", &value, low, high, "set");
	edit.released = ImGui::IsItemDeactivatedAfterEdit();
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::TextUnformatted(label);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", tooltip);
	}
	return edit;
}
} // namespace

void CreatureSpawner::DrawBody(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* creature = registry.TryGet<Creature>(entity);
	auto* needs = registry.TryGet<CreatureNeeds>(entity);
	auto* transform = registry.TryGet<Transform>(entity);
	if (creature == nullptr || needs == nullptr || transform == nullptr)
	{
		return;
	}

	ImGui::SeparatorText("Body and needs");
	// In a network game the bars send their amounts as commands of the turns, and the buttons send their acts
	const bool network = InNetworkGame();
	using creature_edit_values::Value;
	const auto bar = [this, network, entity](const char* label, Value value, float& field, float low, float high,
	                                         const char* tooltip) {
		if (!network)
		{
			return Bar(label, field, low, high, tooltip).changed;
		}
		auto amount = ShownValue(entity, value, field);
		const auto edit = Bar(label, amount, low, high, tooltip);
		FollowValueSlider(entity, value, amount, edit.changed, edit.released);
		return false;
	};
	using creature_edit_acts::Act;
	const auto act = [](const player_commands::EditorCreatureAct& edit) { return editor::DoCreatureAct(edit); };
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		const auto scale = physiology.GetTimeScale();
		ImGui::TextUnformatted("Body time");
		for (size_t i = 0; i < creature_edit_acts::k_TimeScales.size(); ++i)
		{
			const auto option = creature_edit_acts::k_TimeScales.at(i);
			ImGui::SameLine();
			if (ImGui::RadioButton(fmt::format("x{:.0f}", option).c_str(), scale == option))
			{
				act(editor::IndexOf(entt::null, Act::BodyTime, i));
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Game turns of every creature's body that pass each game turn, to watch them grow and tire");
		}
		bool fainting = physiology.IsFaintingEnabled();
		if (ImGui::Checkbox("Creatures faint", &fainting))
		{
			act(editor::SwitchOf(entt::null, Act::Fainting, fainting));
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Grown up creatures owned by a player faint when exhausted, starved or out of life");
		}
	}

	auto& body = needs->needs;
	ImGui::Text("Age %u (game hours), %u turns, %u meals, %s for %u turns%s", body.age, body.turns, body.meals,
	            RestName(needs->rest).data(), needs->restTurns, needs->rested ? ", rested" : "");
	ImGui::Text("%s%s", needs->moving ? "On the move" : "Standing still",
	            needs->moving ? ": tiring, not growing" : ": growing, not tiring");
	bar("Energy", Value::Energy, body.energy, 0.0f, std::max(1.0f, ShownSize(*creature)),
	    "Hunger is 1 less energy; a big meal fills it up to its size");
	bar("Exhaustion", Value::Exhaustion, body.exhaustion, 0.0f, 1.0f, "Slowed to its slow speed from 0.8, faints at 1");
	bar("Thirst", Value::Thirst, body.dehydration, 0.0f, 1.0f, "Fills in about 83 minutes; drinking clears it");
	bar("Poo", Value::Poo, body.poo, 0.0f, 1.0f, "Builds up from meals");
	bar("Life", Value::Life, body.life, 0.0f, 1.0f, "Heals while asleep");
	bar("Warmth", Value::Warmth, body.warmth, -1.0f, 1.0f,
	    "Cold below 0, hot above, from the temperature against its species' comfort");
	bar("Itchiness", Value::Itchiness, body.itchiness, 0.0f, 1.0f, "Only the itchy spell makes a creature itch");
	bar("Fatness", Value::Fatness, creature->fatness, 0.0f, 1.0f, "Burnt while hungry, put on by overeating");
	if (const auto* morph = registry.TryGet<const CreatureMorph>(entity))
	{
		ImGui::SameLine();
		ImGui::Text("(shown %.2f)", static_cast<double>(morph->shownFatness));
	}
	bar("Strength", Value::Strength, creature->strength, 0.0f, 1.0f, "Gained from hard work");
	if (bar("Size", Value::Size, creature->size, creature_morph::k_MinScale, creature_physiology::k_MaxGrownSize,
	        "Grows while standing still, fast while young"))
	{
		creature->size = creature_morph::ClampScale(creature->size);
		transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, creature->size));
		registry.SetDirty();
	}
	const std::array<std::pair<const char*, Act>, 5> k_Needs {{
	    {"Fill up", Act::FillUp},
	    {"Starve", Act::Starve},
	    {"Tire out", Act::TireOut},
	    {"Parch", Act::Parch},
	    {"Bloat", Act::Bloat},
	}};
	for (size_t i = 0; i < k_Needs.size(); ++i)
	{
		if (i > 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::SmallButton(k_Needs.at(i).first))
		{
			act(editor::ActOf(entity, k_Needs.at(i).second));
		}
	}

	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	ImGui::TextUnformatted("See to it now");
	// In a network game the act comes at the next turn, so the tab does not know if the creature can do it
	const auto report = [this, entity, &act](std::string_view what, Act kind) {
		const auto result = act(editor::ActOf(entity, kind));
		_lastNeed = fmt::format("{}: {}", what,
		                        !result.has_value() ? "sent"
		                        : result->done      ? "started"
		                                            : "nothing to do it with");
	};
	if (ImGui::Button("Sleep"))
	{
		report("Sleep", Act::Sleep);
	}
	ImGui::SameLine();
	if (ImGui::Button("Wake"))
	{
		act(editor::ActOf(entity, Act::Wake));
	}
	ImGui::SameLine();
	if (ImGui::Button("Eat"))
	{
		report("Eat the nearest food", Act::Eat);
	}
	ImGui::SameLine();
	if (ImGui::Button("Drink"))
	{
		report("Drink at the nearest water", Act::Drink);
	}
	ImGui::SameLine();
	if (ImGui::Button("Poo"))
	{
		report("Poo", Act::Poo);
	}
	ImGui::SameLine();
	if (ImGui::Button("Puke"))
	{
		report("Puke", Act::Puke);
	}
	ImGui::SameLine();
	if (ImGui::Button("Faint"))
	{
		report("Faint", Act::Faint);
	}
	if (ImGui::Button("Drop food in front") && Locator::infoConstants::has_value())
	{
		auto ahead = -(transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f));
		ahead.y = 0.0f;
		ahead = glm::length(ahead) > 0.0f ? glm::normalize(ahead) : glm::vec3(0.0f, 0.0f, -1.0f);
		const auto at = transform->position + (ahead * (k_FoodAhead * std::max(ShownSize(*creature), 1.0f)));
		auto drop = editor::ActOf(entity, Act::DropFood);
		drop.position = at;
		const auto result = act(drop);
		_lastNeed = !result.has_value() ? "Sent the pot of food" : "Dropped a pot of food";
	}
	if (!_lastNeed.empty())
	{
		ImGui::TextUnformatted(_lastNeed.c_str());
	}
}
