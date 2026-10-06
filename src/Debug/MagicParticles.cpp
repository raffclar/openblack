/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Magic window's particles tab: spawn any particle type or file at the hand or where the camera looks, and watch
// the running effects

#include <algorithm>
#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Magic.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::debug::gui;

namespace
{
constexpr int k_PlayerCount = 8;
constexpr float k_MaxMagnitude = 10.0f;
constexpr float k_MaxHeight = 50.0f;
constexpr float k_MaxCloseAfter = 30.0f;

std::string TypeLabel(ParticleType type)
{
	const auto file = particles::ParticleTypeFile(type);
	return fmt::format("{} {} ({})", static_cast<uint32_t>(type), particles::ParticleTypeName(type),
	                   file.empty() ? "no file" : file);
}
} // namespace

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
	auto& particles = Locator::particleSystem::value();
	if (_particleFiles.empty())
	{
		_particleFiles = particles.GetFileNames();
	}

	ImGui::Checkbox("By file", &_byFile);
	if (_byFile)
	{
		const auto preview = _particleFiles.empty() ? std::string("no files")
		                                            : _particleFiles.at(static_cast<size_t>(std::clamp(
		                                                  _particleFile, 0, static_cast<int>(_particleFiles.size()) - 1)));
		if (ImGui::BeginCombo("File", preview.c_str()))
		{
			for (int i = 0; i < static_cast<int>(_particleFiles.size()); ++i)
			{
				if (ImGui::Selectable(_particleFiles.at(static_cast<size_t>(i)).c_str(), i == _particleFile))
				{
					_particleFile = i;
				}
			}
			ImGui::EndCombo();
		}
	}
	else if (ImGui::BeginCombo("Type", TypeLabel(_particleType).c_str()))
	{
		for (size_t i = 0; i < particles::k_ParticleTypeCount; ++i)
		{
			const auto type = static_cast<ParticleType>(i);
			ImGui::BeginDisabled(particles::ParticleTypeFile(type).empty());
			if (ImGui::Selectable(TypeLabel(type).c_str(), type == _particleType))
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
	ImGui::SliderInt("Player", &_player, 0, k_PlayerCount - 1);
	ImGui::SliderFloat("Close after", &_closeAfter, 0.0f, k_MaxCloseAfter, _closeAfter > 0.0f ? "%.1f s" : "never");
	ImGui::Checkbox("Synced random numbers", &_synced);

	if (ImGui::Button("Spawn"))
	{
		const auto point = SpawnPoint();
		const auto id = _byFile && !_particleFiles.empty()
		                    ? particles.Start(_particleFiles.at(static_cast<size_t>(_particleFile)), point, _magnitude, _synced)
		                    : particles.Start(_particleType, point, _magnitude, _synced);
		particles.SetPlayer(id, _player);
		if (id != ecs::systems::ParticleSystemInterface::k_NoEffect && _closeAfter > 0.0f)
		{
			_timedEffects.push_back({id, _closeAfter});
		}
	}
	ImGui::SameLine();
	bool paused = particles.IsPaused();
	if (ImGui::Checkbox("Pause", &paused))
	{
		particles.SetPaused(paused);
	}
	ImGui::SameLine();
	if (ImGui::Button("Close all"))
	{
		for (const auto& effect : particles.GetEffects())
		{
			particles.CloseDown(effect.id);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete all"))
	{
		for (const auto& effect : particles.GetEffects())
		{
			particles.Delete(effect.id);
		}
	}
	ImGui::Separator();
	DrawRunningEffects();
}

void Magic::DrawRunningEffects() noexcept
{
	auto& particles = Locator::particleSystem::value();
	const auto effects = particles.GetEffects();
	ImGui::Text("%zu running", effects.size());
	if (!ImGui::BeginTable("ParticleEffects", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
	{
		return;
	}
	ImGui::TableSetupColumn("File");
	ImGui::TableSetupColumn("Age");
	ImGui::TableSetupColumn("Atoms");
	ImGui::TableSetupColumn("Groups");
	ImGui::TableSetupColumn("State");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();
	for (const auto& effect : effects)
	{
		ImGui::PushID(static_cast<int>(effect.id));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(effect.file.c_str());
		if (!effect.unportedClasses.empty() && ImGui::IsItemHovered())
		{
			std::string list = "Not run yet:";
			for (const auto& name : effect.unportedClasses)
			{
				list += "\n  " + name;
			}
			ImGui::SetTooltip("%s", list.c_str());
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.1f s", effect.age);
		ImGui::TableNextColumn();
		ImGui::Text("%zu", effect.atoms);
		ImGui::TableNextColumn();
		ImGui::Text("%zu", effect.collections);
		ImGui::TableNextColumn();
		if (effect.closing)
		{
			ImGui::TextUnformatted("closing");
		}
		else if (effect.secondsLeft.has_value())
		{
			ImGui::Text("%.1f s left", *effect.secondsLeft);
		}
		else
		{
			ImGui::TextUnformatted(effect.ownedBySpell ? "miracle" : "running");
		}
		ImGui::TableNextColumn();
		if (ImGui::SmallButton("Close"))
		{
			particles.CloseDown(effect.id);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete"))
		{
			particles.Delete(effect.id);
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void Magic::UpdateTimedEffects(float seconds) noexcept
{
	if (!Locator::particleSystem::has_value() || _timedEffects.empty())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	std::erase_if(_timedEffects, [&](TimedEffect& timed) {
		if (!particles.IsRunning(timed.id))
		{
			return true;
		}
		timed.secondsLeft -= seconds;
		if (timed.secondsLeft > 0.0f)
		{
			return false;
		}
		particles.CloseDown(timed.id);
		return true;
	});
}
