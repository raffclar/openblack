/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindWindow.h"

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include "Camera/Camera.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMindView.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindLogSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using creature_desires::Desire;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;

namespace
{
constexpr float k_TurnsPerSecond = 1000.0f / static_cast<float>(ecs::systems::TimeSystemInterface::k_TurnDuration.count());
/// The label stands this far above the creature's feet, in units of its size
constexpr float k_LabelHeight = 17.0f;

std::string ActionName(const creature_mind_tables::Tables* tables, uint32_t action)
{
	return tables != nullptr && action < tables->actions.size() ? tables->actions[action].name : fmt::format("#{}", action);
}

const creature_mind_tables::Tables* Tables()
{
	return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
}

std::string PlanText(const creature_mind_tables::Tables* tables, const creature_planner::Plan& plan)
{
	auto text = fmt::format("{} for {}", ActionName(tables, plan.action), creature_desires::Name(plan.desire));
	if (plan.object.has_value())
	{
		text += fmt::format(" on entity {}", *plan.object);
	}
	return text;
}

/// Why the planner chose or dropped a plan, in words
std::string Reason(const creature_mind_view::PlanStep& step)
{
	using creature_mind_view::Outcome;
	const auto priority = step.plan.has_value() ? step.plan->priority : 0.0f;
	const auto current = step.current.value_or(0.0f);
	switch (step.outcome)
	{
	case Outcome::Chosen:
		return step.current.has_value() ? fmt::format("{:.1f} is more than twice {:.1f}", priority, current)
		                                : fmt::format("{:.1f}, and no plan ran", priority);
	case Outcome::Forced:
		return "an order, a reaction or a script forced it";
	case Outcome::Kept:
		return "it is the plan that runs";
	case Outcome::NotBest:
		return fmt::format("{:.1f} is less than the plan for {} ({:.1f})", priority,
		                   step.best.has_value() ? creature_desires::Name(*step.best) : "?", step.bestPriority);
	case Outcome::NotPressing:
		return fmt::format("{:.1f} is not more than twice {:.1f}", priority, current);
	case Outcome::NoPlan:
		return "no action fits, or the desire cannot have a plan now";
	case Outcome::Ended:
		return "the plan stopped";
	}
	return {};
}

const creature_mind_view::Trace* TraceOf(entt::entity creature)
{
	return Locator::creatureMindLog::has_value() ? Locator::creatureMindLog::value().Find(creature) : nullptr;
}
} // namespace

CreatureMindWindow::CreatureMindWindow() noexcept
    : Window("Creature Mind", ImVec2(560.0f, 720.0f))
{
}

void CreatureMindWindow::Select(entt::entity creature) noexcept
{
	_chosen = creature;
	WatchChosen();
}

void CreatureMindWindow::StopWatching() noexcept
{
	if (_watching.has_value() && Locator::creatureMindLog::has_value())
	{
		Locator::creatureMindLog::value().Watch(*_watching, false);
	}
	_watching.reset();
}

void CreatureMindWindow::WatchChosen() noexcept
{
	if (_watching == _chosen || !Locator::creatureMindLog::has_value())
	{
		return;
	}
	StopWatching();
	auto& log = Locator::creatureMindLog::value();
	if (_chosen.has_value() && !log.IsWatched(*_chosen))
	{
		log.Watch(*_chosen, true);
		_watching = _chosen;
	}
}

void CreatureMindWindow::Update() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (_chosen.has_value() && !(registry.Valid(*_chosen) && registry.AllOf<CreatureMindState>(*_chosen)))
	{
		_chosen.reset();
	}
	if (_followHand && Locator::pickingSystem::has_value())
	{
		const auto& picked = Locator::pickingSystem::value().GetPick().object;
		if (picked.has_value() && registry.Valid(*picked) && registry.AllOf<CreatureMindState>(*picked))
		{
			_chosen = *picked;
		}
	}
	// With no choice, the view shows the first creature that has a mind
	if (!_chosen.has_value())
	{
		registry.Each<const CreatureMindState>([this](entt::entity entity, const CreatureMindState&) {
			if (!_chosen.has_value())
			{
				_chosen = entity;
			}
		});
	}
	WatchChosen();
}

void CreatureMindWindow::UpdateAlways() noexcept
{
	if (!IsOpen() && _watching.has_value())
	{
		StopWatching();
	}
}

void CreatureMindWindow::DrawPicker() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto label = [&registry](entt::entity entity) {
		const auto* body = registry.TryGet<const Creature>(entity);
		return fmt::format("{} {}", body != nullptr ? editor::SpeciesName(body->species) : "creature",
		                   entt::to_integral(entity));
	};
	const auto preview = _chosen.has_value() ? label(*_chosen) : std::string("none");
	if (ImGui::BeginCombo("Creature", preview.c_str()))
	{
		registry.Each<const CreatureMindState>([&](entt::entity entity, const CreatureMindState&) {
			if (ImGui::Selectable(label(entity).c_str(), _chosen == entity))
			{
				_chosen = entity;
				_followHand = false;
			}
		});
		ImGui::EndCombo();
	}
	ImGui::Checkbox("Follow the creature under the hand", &_followHand);
	ImGui::SameLine();
	ImGui::Checkbox("Label in the world", &_showLabel);
}

void CreatureMindWindow::Draw() noexcept
{
	DrawPicker();
	if (!_chosen.has_value())
	{
		ImGui::TextUnformatted("No creature has a mind");
		return;
	}
	const auto creature = *_chosen;
	const auto* mind = Locator::entitiesRegistry::value().TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->desires.has_value())
	{
		ImGui::TextUnformatted("The mind has not thought yet");
		return;
	}
	if (TraceOf(creature) == nullptr)
	{
		ImGui::TextUnformatted("The mind log is not there: the build has no developer tools");
	}
	DrawDesires(creature);
	DrawPlans(creature);
	DrawTimeline(creature);
	if (_showLabel)
	{
		DrawLabel(creature);
	}
}

void CreatureMindWindow::DrawDesires(entt::entity creature) noexcept
{
	const auto& mind = Locator::entitiesRegistry::value().Get<const CreatureMindState>(creature);
	const auto* trace = TraceOf(creature);
	const auto rows =
	    creature_mind_view::ActiveDesires(*mind.desires, trace != nullptr ? trace->Before() : nullptr, k_TurnsPerSecond);
	ImGui::SeparatorText(fmt::format("Active desires: {} (sum {:.2f})", rows.size(), mind.desires->sum).c_str());
	constexpr auto k_Flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
	if (!ImGui::BeginTable("desires", 6, k_Flags))
	{
		return;
	}
	ImGui::TableSetupColumn("Desire");
	ImGui::TableSetupColumn("Value / max");
	ImGui::TableSetupColumn("Change");
	ImGui::TableSetupColumn("Drive");
	ImGui::TableSetupColumn("Growth / turn");
	ImGui::TableSetupColumn("Held back");
	ImGui::TableHeadersRow();
	for (const auto& row : rows)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		const auto name = fmt::format("{}{}", row.winner ? "* " : "", creature_desires::Name(row.desire));
		const bool open = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
		ImGui::TableNextColumn();
		ImGui::Text("%.3f / %.2f", static_cast<double>(row.value), static_cast<double>(row.max));
		ImGui::TableNextColumn();
		if (row.change.has_value())
		{
			ImGui::Text("%+.4f", static_cast<double>(*row.change));
		}
		else
		{
			ImGui::TextUnformatted("-");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.3f", static_cast<double>(row.drive));
		ImGui::TableNextColumn();
		ImGui::Text("%.5f", static_cast<double>(row.growthPerTurn));
		ImGui::TableNextColumn();
		if (row.suppressedTurns > 0)
		{
			ImGui::Text("%u turns", row.suppressedTurns);
		}
		if (!open)
		{
			continue;
		}
		// The sources: what each gives the desire this turn, and what it gave since the creature last decided on it
		for (const auto& source : row.sources)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("  %s", creature_mind_view::SourceName(source.type).data());
			ImGui::TableNextColumn();
			ImGui::Text("%.3f / %.3f", static_cast<double>(source.value), static_cast<double>(source.threshold));
			ImGui::TableNextColumn();
			ImGui::Text("x%.3f", static_cast<double>(source.multiplier));
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", static_cast<double>(source.part));
			ImGui::TableNextColumn();
			ImGui::Text("total %.2f", static_cast<double>(source.drive));
			ImGui::TableNextColumn();
		}
		ImGui::TreePop();
	}
	ImGui::EndTable();
	ImGui::TextDisabled("* the desire that the creature shows. A source row: value / threshold, multiplier, drive.");
}

void CreatureMindWindow::DrawPlans(entt::entity creature) noexcept
{
	const auto& mind = Locator::entitiesRegistry::value().Get<const CreatureMindState>(creature);
	const auto* tables = Tables();
	const auto& planner = mind.planner;
	ImGui::SeparatorText("Plans");
	ImGui::Text("Activity: %s, agenda step %zu of %zu", creature_mind::Name(mind.idle.activity).data(), mind.idle.step,
	            mind.idle.agenda.size());
	if (planner.current.has_value())
	{
		ImGui::Text("Plan: %s, priority %.1f%s", PlanText(tables, *planner.current).c_str(),
		            static_cast<double>(planner.current->priority), mind.planForced ? " (forced)" : "");
		const auto opinion = mind.learnt.has_value() && planner.current->action < mind.learnt->opinions.size()
		                         ? mind.learnt->opinions.at(planner.current->action)
		                         : 0.0f;
		ImGui::Text("Opinion of the action %+.2f. A new plan must have more than %.1f.", static_cast<double>(opinion),
		            static_cast<double>(planner.current->priority * 2.0f));
	}
	else
	{
		ImGui::TextUnformatted("Plan: none. Any plan with a priority more than 0 can start.");
	}
	const auto best = creature_planner::Best(planner);
	if (ImGui::TreeNodeEx("The best plan of each desire", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const auto& plan : planner.best)
		{
			if (plan.has_value())
			{
				const bool top = best.has_value() && best->desire == plan->desire;
				ImGui::Text("%s%s: goal %.2f, action %.4f, priority %.1f", top ? "> " : "  ", PlanText(tables, *plan).c_str(),
				            static_cast<double>(plan->goalUsefulness), static_cast<double>(plan->actionPriority),
				            static_cast<double>(plan->priority));
			}
		}
		ImGui::TreePop();
	}
	const auto* trace = TraceOf(creature);
	if (trace != nullptr && ImGui::TreeNodeEx("Planner steps, newest first", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const auto& step : trace->Steps() | std::views::reverse)
		{
			ImGui::Text("turn %u %-20s %s: %s", step.turn, creature_mind_view::Name(step.outcome).data(),
			            step.plan.has_value() ? PlanText(tables, *step.plan).c_str()
			                                  : std::string(creature_desires::Name(step.desire)).c_str(),
			            Reason(step).c_str());
		}
		ImGui::TreePop();
	}
}

void CreatureMindWindow::DrawTimeline(entt::entity creature) noexcept
{
	const auto* trace = TraceOf(creature);
	if (trace == nullptr || trace->Samples().empty() ||
	    !ImGui::TreeNodeEx("Timeline", ImGuiTreeNodeFlags_DefaultOpen, "Timeline of the last %zu turns",
	                       trace->Samples().size()))
	{
		return;
	}
	const auto& mind = Locator::entitiesRegistry::value().Get<const CreatureMindState>(creature);
	std::vector<float> values;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		if (!creature_mind_view::IsActive(*mind.desires, d))
		{
			continue;
		}
		values.clear();
		std::ranges::transform(trace->Samples(), std::back_inserter(values),
		                       [d](const creature_mind_view::Sample& sample) { return sample.values.at(d); });
		const auto name = creature_desires::Name(static_cast<Desire>(d));
		ImGui::PlotLines(fmt::format("{} {:.2f}", name, values.back()).c_str(), values.data(), static_cast<int>(values.size()),
		                 0, nullptr, 0.0f, mind.desires->desires.at(d).max, ImVec2(0.0f, 32.0f));
	}
	ImGui::TreePop();
}

void CreatureMindWindow::DrawLabel(entt::entity creature) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto& mind = registry.Get<const CreatureMindState>(creature);
	if (transform == nullptr || body == nullptr || !Locator::camera::has_value())
	{
		return;
	}
	const auto display = ImGui::GetIO().DisplaySize;
	glm::vec3 screen;
	const auto head = transform->position + glm::vec3(0.0f, k_LabelHeight * body->size, 0.0f);
	if (!Locator::camera::value().ProjectWorldToScreen(head, glm::vec4(0.0f, 0.0f, display.x, display.y), screen))
	{
		return;
	}
	const auto rows = creature_mind_view::ActiveDesires(*mind.desires, nullptr, k_TurnsPerSecond);
	auto text = rows.empty() ? std::string("no desire")
	                         : fmt::format("{} {:.2f}", creature_desires::Name(rows.front().desire), rows.front().value);
	text += "\n" + (mind.planner.current.has_value() ? PlanText(Tables(), *mind.planner.current) : std::string("no plan"));
	auto* drawList = ImGui::GetBackgroundDrawList();
	const auto size = ImGui::CalcTextSize(text.c_str());
	const ImVec2 corner {screen.x - size.x * 0.5f, screen.y - size.y};
	drawList->AddRectFilled(ImVec2(corner.x - 4.0f, corner.y - 2.0f),
	                        ImVec2(corner.x + size.x + 4.0f, corner.y + size.y + 2.0f), IM_COL32(0, 0, 0, 160), 3.0f);
	drawList->AddText(corner, IM_COL32(255, 230, 120, 255), text.c_str());
}
