/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "3D/CreatureBody.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRig.h"
#include "CreatureSpawner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAudio;

namespace
{
std::string_view KindName(creature_audio::EventKind kind)
{
	switch (kind)
	{
	case creature_audio::EventKind::Voice:
		return "voice";
	case creature_audio::EventKind::HairGroup:
		return "hair";
	case creature_audio::EventKind::Generic:
		return "shared";
	}
	return "?";
}

std::string ActionLabel(audio::SoundAction action)
{
	const auto name = creature_audio::Name(action);
	return name.empty() ? fmt::format("action {}", static_cast<int32_t>(action))
	                    : fmt::format("{} ({})", name, static_cast<int32_t>(action));
}
} // namespace

void CreatureSpawner::DrawAudioSettings() noexcept
{
	if (!Locator::creatureAudioSystem::has_value())
	{
		return;
	}
	auto& sounds = Locator::creatureAudioSystem::value();
	bool muted = sounds.IsMuted();
	if (ImGui::Checkbox("Mute creatures", &muted))
	{
		sounds.SetMuted(muted);
	}
	ImGui::SameLine();
	bool otherVoices = sounds.AreOtherVoicesEnabled();
	if (ImGui::Checkbox("Other players' creatures' voices", &otherVoices))
	{
		sounds.SetOtherVoicesEnabled(otherVoices);
	}
	ImGui::SetItemTooltip("Whether creatures not of player one roar and cry, as a script can let them");
}

void CreatureSpawner::DrawAudio(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* creature = std::as_const(registry).TryGet<const Creature>(entity);
	if (creature == nullptr || !Locator::creatureAudioSystem::has_value())
	{
		return;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature->species);
	const auto* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;

	ImGui::SeparatorText("Sounds");
	if (rig == nullptr)
	{
		ImGui::TextUnformatted("The species' animations are not loaded");
		return;
	}
	const auto voiceBank = creature_audio::VoiceBankStem(rig->soundBankName, creature->species);
	ImGui::Text("Voice bank %s, sound object %d, size key %d", voiceBank.empty() ? "none" : voiceBank.c_str(), rig->soundObject,
	            static_cast<int32_t>(creature_audio::SizeKey(creature->size)));

	if (const auto* heard = std::as_const(registry).TryGet<const CreatureAudio>(entity);
	    heard != nullptr && !heard->recent.empty())
	{
		if (ImGui::BeginTable("Last sounds", 5,
		                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
		                          ImGuiTableFlags_SizingFixedFit,
		                      ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.0f)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Seconds");
			ImGui::TableSetupColumn("Sound");
			ImGui::TableSetupColumn("Bank");
			ImGui::TableSetupColumn("Keys");
			ImGui::TableSetupColumn("Result");
			ImGui::TableHeadersRow();
			for (auto it = heard->recent.rbegin(); it != heard->recent.rend(); ++it)
			{
				const auto keys = it->keys.ToArray();
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", static_cast<double>(it->atMs / 1000.0f));
				ImGui::TableNextColumn();
				ImGui::Text("%s, %s", ActionLabel(it->keys.action).c_str(), KindName(it->kind).data());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(it->bank != audio::k_NoBank ? audio::banks::Path(it->bank).c_str() : "none");
				ImGui::TableNextColumn();
				ImGui::Text("%d %d %d %d %d", keys[0], keys[1], keys[2], keys[3], keys[4]);
				ImGui::TableNextColumn();
				// No channel: the bank's filters left it out, or nothing started
				const auto channel =
				    it->channel != audio::k_NoChannel ? fmt::format("channel {}", it->channel) : std::string("no channel");
				ImGui::Text("%s, %s", channel.c_str(), it->played ? "played" : it->note.c_str());
			}
			ImGui::EndTable();
		}
	}
	else
	{
		ImGui::TextUnformatted("No sounds yet");
	}

	if (!ImGui::TreeNode("Sounds of the animations"))
	{
		return;
	}
	auto& sounds = Locator::creatureAudioSystem::value();
	for (size_t index = 0; index < rig->soundEvents.size(); ++index)
	{
		const auto& events = rig->soundEvents[index];
		if (events.empty())
		{
			continue;
		}
		const auto name = creature_layers::animations::Name(index);
		ImGui::PushID(static_cast<int>(index));
		if (ImGui::TreeNode("animation", "%zu %s (%zu)", index, name.empty() ? "" : std::string(name).c_str(), events.size()))
		{
			for (size_t i = 0; i < events.size(); ++i)
			{
				const auto& event = events[i];
				ImGui::PushID(static_cast<int>(i));
				ImGui::BeginDisabled(event.kind == creature_audio::EventKind::HairGroup);
				if (ImGui::SmallButton("Play"))
				{
					sounds.Play(entity, event);
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::Text("%d ms: %s, %s%s", event.timeMs, ActionLabel(event.action).c_str(), KindName(event.kind).data(),
				            event.mode != 0 ? fmt::format(", mode {}", event.mode).c_str() : "");
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::TreePop();
}
