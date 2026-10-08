/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Magic.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicModel.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysManagerState.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::magic_window;

namespace
{
constexpr float k_MaxMagnitude = 10.0f;
constexpr float k_MaxHeight = 50.0f;
constexpr std::array k_DrawPaths {psys::DrawPath::Sorted, psys::DrawPath::Queued, psys::DrawPath::Immediate};

void Row(std::string_view label, const std::string& value)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(label.data(), label.data() + label.size());
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(value.c_str());
}

std::string YesNo(bool value)
{
	return value ? "yes" : "no";
}
} // namespace

Magic::Magic() noexcept
    : Window("Magic", ImVec2(560.0f, 640.0f))
{
}

void Magic::Draw() noexcept
{
	if (!Locator::infoConstants::has_value())
	{
		ImGui::TextUnformatted("No info.dat loaded");
		return;
	}
	if (ImGui::BeginTabBar("MagicTabs"))
	{
		if (ImGui::BeginTabItem("Miracle"))
		{
			DrawSelected();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("All miracles"))
		{
			DrawTable();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Particles"))
		{
			DrawParticles();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void Magic::DrawSelected() noexcept
{
	const auto& info = Locator::infoConstants::value();
	if (ImGui::BeginCombo("Magic type", MagicName(info, _selected).c_str()))
	{
		for (size_t i = 0; i < magic::k_MagicTypeCount; ++i)
		{
			const auto type = static_cast<MagicType>(i);
			if (ImGui::Selectable(MagicName(info, type).c_str(), type == _selected))
			{
				_selected = type;
			}
		}
		ImGui::EndCombo();
	}

	const auto& record = magic::GetMagicInfo(info, _selected);
	const auto& effect = magic::GetMagicEffectInfo(info, _selected);
	const auto slot = magic::SlotOf(_selected);
	const auto seed = magic::GetFirstSpellSeedForMagicType(info, _selected);
	const auto powerUp = magic::GetPowerUpGestureForMagicType(info, _selected);

	if (ImGui::BeginTable("MagicInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		Row("Section", fmt::format("{} #{}", SectionName(slot.section), slot.index));
		Row("Cost to create", fmt::format("{:.0f}", effect.costToCreate));
		Row("Prayer power when cast", fmt::format("{:.0f}", effect.initialChants));
		Row("Upkeep per turn", fmt::format("{:.1f}", effect.costPerGameTurn));
		Row("Cost per event", fmt::format("{:.1f}", effect.costPerEvent));
		Row("Cost per shield impact", fmt::format("{:.1f}", effect.costPerShieldCollide));
		Row("Maintained", YesNo(magic::IsMaintainedSpell(_selected)));
		Row("Recharged by its caster", YesNo(record.isSpellRecharged != 0));
		Row("Cheaper with tribal power", YesNo(effect.divideCostsByTribalPower == 1));
		Row("Timer, one shot", Seconds(effect.timerWhenOneShot));
		Row("Timer, player", Seconds(effect.timerWhenPlayerCasting));
		Row("Timer, creature", Seconds(effect.timerWhenCreatureCasting));
		Row("Timer, computer player", Seconds(effect.timerWhenComputerPlayerCasting));
		Row("Creature casts from above", YesNo(magic::IsCreatureCastFromAbove(info, _selected)));
		Row("Aggressive range", fmt::format("{:.0f} to {:.0f}", effect.agressiveRangeMin, effect.agressiveRangeMax));
		Row("Perceived power", fmt::format("{:.2f}", record.perceivedPower));
		Row("First seed", SeedName(info, seed));
		Row("Power-up level", PowerUpLevelName(powerUp.level));
		Row("Power-up gesture", fmt::format("{}", static_cast<uint32_t>(powerUp.gesture)));
		if (const auto* creatureSpell = magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, _selected))
		{
			Row("Total duration", Seconds(creatureSpell->totalDuration));
		}
		if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, _selected))
		{
			Row("Radius", fmt::format("{:.0f} to {:.0f} (normal cost at {:.0f})", radius->minRadius, radius->maxRadius,
			                          radius->radiusForNormalCost));
		}
		ImGui::EndTable();
	}
}

void Magic::DrawTable() noexcept
{
	const auto& info = Locator::infoConstants::value();
	constexpr auto k_Flags =
	    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("AllMagic", 7, k_Flags))
	{
		return;
	}
	ImGui::TableSetupScrollFreeze(1, 1);
	ImGui::TableSetupColumn("Magic type");
	ImGui::TableSetupColumn("Create");
	ImGui::TableSetupColumn("When cast");
	ImGui::TableSetupColumn("Per turn");
	ImGui::TableSetupColumn("Per event");
	ImGui::TableSetupColumn("Player");
	ImGui::TableSetupColumn("Creature");
	ImGui::TableHeadersRow();
	for (size_t i = 0; i < magic::k_MagicTypeCount; ++i)
	{
		const auto type = static_cast<MagicType>(i);
		const auto& effect = magic::GetMagicEffectInfo(info, type);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Selectable(MagicName(info, type).c_str(), type == _selected, ImGuiSelectableFlags_SpanAllColumns))
		{
			_selected = type;
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.0f", static_cast<double>(effect.costToCreate));
		ImGui::TableNextColumn();
		ImGui::Text("%.0f", static_cast<double>(effect.initialChants));
		ImGui::TableNextColumn();
		ImGui::Text("%.1f", static_cast<double>(effect.costPerGameTurn));
		ImGui::TableNextColumn();
		ImGui::Text("%.1f", static_cast<double>(effect.costPerEvent));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Seconds(effect.timerWhenPlayerCasting).c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Seconds(effect.timerWhenCreatureCasting).c_str());
	}
	ImGui::EndTable();
}

glm::vec3 Magic::SpawnPoint() const noexcept
{
	glm::vec3 point(0.0f);
	if (_spawnAt == SpawnAt::Hand && Locator::handSystem::has_value())
	{
		for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
		{
			if (hand.has_value())
			{
				return *hand + glm::vec3(0.0f, _spawnHeight, 0.0f);
			}
		}
	}
	if (Locator::camera::has_value())
	{
		point = Locator::camera::value().GetFocus();
	}
	if (Locator::terrainSystem::has_value())
	{
		point.y = Locator::terrainSystem::value().GetHeightAt({point.x, point.z});
	}
	return point + glm::vec3(0.0f, _spawnHeight, 0.0f);
}

void Magic::DrawParticles() noexcept
{
	if (!Locator::particleSystem::has_value())
	{
		ImGui::TextUnformatted("No particle system");
		return;
	}
	if (ImGui::BeginCombo("Type", ParticleTypeLabel(_particleType).c_str()))
	{
		for (size_t i = 0; i < psys::k_ParticleTypeCount; ++i)
		{
			const auto type = static_cast<ParticleType>(i);
			ImGui::BeginDisabled(psys::ParticleTypeFile(type).empty());
			if (ImGui::Selectable(ParticleTypeLabel(type).c_str(), type == _particleType))
			{
				_particleType = type;
			}
			ImGui::EndDisabled();
		}
		ImGui::EndCombo();
	}

	ImGui::RadioButton("At what the camera looks at", reinterpret_cast<int*>(&_spawnAt),
	                   static_cast<int>(SpawnAt::CameraFocus));
	ImGui::SameLine();
	ImGui::RadioButton("At the hand", reinterpret_cast<int*>(&_spawnAt), static_cast<int>(SpawnAt::Hand));
	ImGui::SliderFloat("Height", &_spawnHeight, 0.0f, k_MaxHeight, "%.1f");
	ImGui::SliderFloat("Magnitude", &_magnitude, 0.0f, k_MaxMagnitude, "%.2f");
	if (ImGui::BeginCombo("Drawn", DrawPathName(_drawPath).data()))
	{
		for (const auto path : k_DrawPaths)
		{
			if (ImGui::Selectable(DrawPathName(path).data(), path == _drawPath))
			{
				_drawPath = path;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Sorted: each sprite, model, mist and ribbon in its own place among what blends.\n"
		                  "Queued: the whole effect at its origin, drawn in its own order, as some spot visuals are.\n"
		                  "Immediate: drawn just after the hand, as the miracle in the hand is.");
	}

	const auto file = std::string(psys::ParticleTypeFile(_particleType));
	ImGui::BeginDisabled(file.empty());
	if (ImGui::Button("Spawn"))
	{
		const auto id = psys::manager::Start(file, SpawnPoint(), _magnitude);
		if (id != 0)
		{
			psys::manager::SetDrawPath(id, _drawPath);
			_started.push_back({.id = id, .file = file});
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Close all"))
	{
		for (const auto& started : _started)
		{
			psys::manager::CloseDown(started.id);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete all"))
	{
		for (const auto& started : _started)
		{
			psys::manager::Delete(started.id);
		}
	}
	ImGui::Separator();
	DrawRunningEffects();
}

void Magic::DrawRunningEffects() noexcept
{
	// The effects this window started that are still running
	std::erase_if(_started, [](const Started& started) { return psys::manager::Find(started.id) == nullptr; });
	size_t running = 0;
	for ([[maybe_unused]] const auto& effect : Locator::particleSystem::value().GetState().effects)
	{
		++running;
	}
	ImGui::Text("%zu running, %zu started here", running, _started.size());
	if (!ImGui::BeginTable("ParticleEffects", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
	{
		return;
	}
	ImGui::TableSetupColumn("File");
	ImGui::TableSetupColumn("Drawn");
	ImGui::TableSetupColumn("Age");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();
	// Changed once the walk is done
	std::optional<uint32_t> toClose;
	std::optional<uint32_t> toDelete;
	std::optional<std::pair<uint32_t, psys::DrawPath>> toRedraw;
	for (const auto& started : _started)
	{
		const auto* effect = psys::manager::Find(started.id);
		if (effect == nullptr)
		{
			continue;
		}
		ImGui::PushID(static_cast<int>(started.id));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(started.file.c_str());
		ImGui::TableNextColumn();
		// Click to draw it the next way
		const auto path = psys::manager::GetDrawPath(started.id);
		if (ImGui::SmallButton(DrawPathName(path).data()))
		{
			const auto next = (static_cast<size_t>(path) + 1) % k_DrawPaths.size();
			toRedraw = {started.id, k_DrawPaths.at(next)};
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.1f s%s", static_cast<double>(effect->GetAge()), effect->Closing() ? ", closing" : "");
		ImGui::TableNextColumn();
		if (ImGui::SmallButton("Close"))
		{
			toClose = started.id;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete"))
		{
			toDelete = started.id;
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
	if (toRedraw)
	{
		psys::manager::SetDrawPath(toRedraw->first, toRedraw->second);
	}
	if (toClose)
	{
		psys::manager::CloseDown(*toClose);
	}
	if (toDelete)
	{
		psys::manager::Delete(*toDelete);
	}
}

void Magic::Update() noexcept {}

void Magic::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Magic::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
