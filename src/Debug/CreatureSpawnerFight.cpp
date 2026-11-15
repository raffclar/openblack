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
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <imgui.h>

#include "Creature/CreatureFight.h"
#include "CreatureSpawner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "Editor/EditorCreatureActs.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureFighting;
using openblack::ecs::components::CreatureFightRecord;
using openblack::ecs::components::CreatureKnockedOut;
using openblack::ecs::components::Transform;
namespace fight = openblack::creature_fight;

namespace
{
constexpr float k_BarWidth = 160.0f;

std::string_view StageName(CreatureFighting::Stage stage)
{
	constexpr std::array<std::string_view, 10> k_Names {
	    "turning to face", "taunting", "pointing at the arena", "walking to its place", "waiting",
	    "squaring up",     "duelling", "celebrating",           "responding",           "stopping",
	};
	return k_Names.at(static_cast<size_t>(stage));
}

std::string_view KnockedOutName(CreatureKnockedOut::Stage stage)
{
	constexpr std::array<std::string_view, 6> k_Names {
	    "lying out cold", "fading out", "fading in at home", "lying at home", "resting", "getting up",
	};
	return k_Names.at(static_cast<size_t>(stage));
}

std::string MoveName(const fight::QueuedMove& queued)
{
	using Kind = fight::Move::Kind;
	std::string name;
	switch (queued.move.kind)
	{
	case Kind::High:
		name = "high";
		break;
	case Kind::Mid:
		name = "mid";
		break;
	case Kind::Low:
		name = "low";
		break;
	case Kind::Block:
		name = "block";
		break;
	case Kind::Special:
		name = "special";
		break;
	case Kind::Animation:
		name = fmt::format("anim {}", queued.move.value);
		break;
	case Kind::Spell:
		name = fmt::format("spell {}", queued.move.value);
		break;
	}
	return queued.chargeMs.has_value() ? fmt::format("{} ({:.0f} ms)", name, *queued.chargeMs) : name + " (charging)";
}

void Bar(const char* label, float value)
{
	ImGui::ProgressBar(value, ImVec2(k_BarWidth, 0.0f), fmt::format("{:.2f}", value).c_str());
	ImGui::SameLine();
	ImGui::TextUnformatted(label);
}

/// One side of the fight: its stage and state, health, stamina and queue
void DrawFighter(const char* who, const CreatureFighting& fighting)
{
	const auto& fighter = fighting.fighter;
	ImGui::Text("%s: %s, %s, anim %zu at %.0f ms x%.2f", who, StageName(fighting.stage).data(),
	            fight::Name(fighter.state).data(), fighter.animation, static_cast<double>(fighter.timeMs),
	            static_cast<double>(fighter.speed));
	Bar("fight health", fighter.health);
	Bar("stamina", fighter.stamina);
	const auto* control = fighter.control == fight::Control::Player     ? "player"
	                      : fighter.control == fight::Control::Computer ? "computer"
	                                                                    : "nobody (queued moves only)";
	ImGui::Text("%s, computer waits %.1f s, tendency %+.2f (tier %u)", control,
	            static_cast<double>(std::max(fighter.computerWaitMs, 0.0f) / 1000.0f), static_cast<double>(fighter.tendency),
	            fight::TierOf(fighter.tendency));
	std::string queue;
	for (const auto& move : fighter.queue.Moves())
	{
		queue += (queue.empty() ? "" : ", ") + MoveName(move);
	}
	ImGui::TextWrapped("Queue %zu/%zu: %s", fighter.queue.Size(), fight::MoveQueue::k_Capacity,
	                   queue.empty() ? "empty" : queue.c_str());
	ImGui::Text("%zu blows measured", fighting.reaches.size());
}
} // namespace

void CreatureSpawner::DrawFight(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::creatureFightSystem::has_value() || !registry.AllOf<Creature, Transform>(entity))
	{
		return;
	}
	auto& fights = Locator::creatureFightSystem::value();
	ImGui::SeparatorText("Fight");
	// In a network game each change goes as a command of the turns. The camera of this computer is not part of the world.
	using creature_edit_acts::Act;
	const auto act = [](const player_commands::EditorCreatureAct& edit) { return editor::DoCreatureAct(edit); };

	if (const auto* record = registry.TryGet<const CreatureFightRecord>(entity))
	{
		ImGui::Text("%u fights, %u won, tendency %+.2f", record->fights, record->wins, static_cast<double>(record->tendency));
	}
	auto cameraWatches = fights.GetCameraWatches();
	if (ImGui::Checkbox("Camera watches", &cameraWatches))
	{
		fights.SetCameraWatches(cameraWatches);
	}

	if (const auto* knockedOut = registry.TryGet<const CreatureKnockedOut>(entity))
	{
		ImGui::Text("Knocked out: %s%s, %.1f s", KnockedOutName(knockedOut->stage).data(),
		            knockedOut->permanent ? " for good" : "", static_cast<double>(knockedOut->seconds));
		if (ImGui::Button("Bring round"))
		{
			act(editor::ActOf(entity, Act::BringRound));
		}
		return;
	}

	const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
	if (fighting == nullptr)
	{
		// The other creatures it could fight, the nearest first
		const auto at = registry.Get<const Transform>(entity).position;
		std::vector<std::pair<float, entt::entity>> others;
		registry.Each<const Creature, const Transform>([&](entt::entity other, const Creature&, const Transform& transform) {
			if (other != entity)
			{
				others.emplace_back(glm::distance(at, transform.position), other);
			}
		});
		std::ranges::sort(others);
		if (others.empty())
		{
			ImGui::TextUnformatted("No other creature to fight");
		}
		else
		{
			_fightOpponent = std::min(_fightOpponent, static_cast<int>(others.size()) - 1);
			const auto label = [&others](int index) {
				const auto& [distance, other] = others.at(static_cast<size_t>(index));
				return fmt::format("creature {} at {:.0f}", entt::to_integral(other), distance);
			};
			if (ImGui::BeginCombo("Opponent", label(_fightOpponent).c_str()))
			{
				for (int i = 0; i < static_cast<int>(others.size()); ++i)
				{
					if (ImGui::Selectable(label(i).c_str(), i == _fightOpponent))
					{
						_fightOpponent = i;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Start fight"))
			{
				auto start = editor::SwitchOf(entity, Act::StartFight, _fightAuto);
				start.other = others.at(static_cast<size_t>(_fightOpponent)).second;
				const auto result = act(start);
				_lastFight = !result.has_value() ? "sent" : result->done ? "started" : "refused";
			}
			ImGui::SameLine();
			ImGui::Checkbox("Start fighting by itself", &_fightAuto);
		}
		if (ImGui::Button("Knock out"))
		{
			act(editor::ActOf(entity, Act::KnockOut));
		}
		ImGui::SameLine();
		if (ImGui::Button("Kill for good"))
		{
			act(editor::ActOf(entity, Act::KillForGood));
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("As only a script can: it faints and never gets up, until brought round here");
		}
		if (!_lastFight.empty())
		{
			ImGui::Text("Last: %s", _lastFight.c_str());
		}
		return;
	}

	DrawFighter("It", *fighting);
	if (const auto* other =
	        registry.Valid(fighting->opponent) ? registry.TryGet<const CreatureFighting>(fighting->opponent) : nullptr)
	{
		DrawFighter(fmt::format("Creature {}", entt::to_integral(fighting->opponent)).c_str(), *other);
	}
	ImGui::Text("Arena radius %.1f", static_cast<double>(fighting->arena.radius));
	auto autoFight = fighting->fighter.control != fight::Control::None;
	if (ImGui::Checkbox("Fights by itself", &autoFight))
	{
		act(editor::SwitchOf(entity, Act::FightsByItself, autoFight));
	}
	ImGui::SameLine();
	if (ImGui::Button("Abort fight"))
	{
		act(editor::ActOf(entity, Act::AbortFight));
	}

	// Orders as the player's clicks give them, each blow charged as if held this long
	ImGui::SliderFloat("Charge", &_fightChargeMs, 0.0f, fight::k_MaxChargeMs, "%.0f ms");
	const auto blow = [&](const char* label, fight::Band band) {
		if (ImGui::Button(label))
		{
			auto hit = editor::IndexOf(entity, Act::Blow, static_cast<size_t>(band));
			hit.amount = _fightChargeMs;
			act(hit);
		}
	};
	blow("High", fight::Band::High);
	ImGui::SameLine();
	blow("Mid", fight::Band::Mid);
	ImGui::SameLine();
	blow("Low", fight::Band::Low);
	ImGui::SameLine();
	if (ImGui::Button("Block"))
	{
		act(editor::ActOf(entity, Act::Block));
	}
	ImGui::SameLine();
	if (ImGui::Button("Special"))
	{
		act(editor::ActOf(entity, Act::Special));
	}
	constexpr std::array<std::pair<const char*, fight::Step>, 4> k_Steps {
	    std::pair("Step forward", fight::Step::Forward), std::pair("Step back", fight::Step::Back),
	    std::pair("Step left", fight::Step::Left), std::pair("Step right", fight::Step::Right)};
	for (size_t i = 0; i < k_Steps.size(); ++i)
	{
		if (i > 0)
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(k_Steps.at(i).first))
		{
			act(editor::IndexOf(entity, Act::FightStep, static_cast<size_t>(k_Steps.at(i).second)));
		}
	}
}
