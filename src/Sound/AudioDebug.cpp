/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AudioDebug.h"
#include "SoundHandler.h"
#include <imgui.h>

using namespace openblack;
using namespace openblack::audio;

std::string AudioDebug::_selectedPack = "";
const ImVec4 redColor = ImVec4(1.f, .0f, .0f, 1.f);
const ImVec4 greenColor = ImVec4(0.f, 1.f, .0f, 1.f);

void AudioDebug::AudioPlayer(Game& game, const std::vector<std::shared_ptr<SoundPack>>& soundPacks)
{
	auto& handler = game.GetSoundHandler();
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::BeginChild("SoundPacks", ImVec2(ImGui::GetContentRegionAvail().x / 2, ImGui::GetContentRegionAvail().y), true);
	ImGui::Button("Sort by sound count");
	ImGui::SameLine();
	ImGui::Button("Sort by bytes");
	ImGui::SameLine();
	ImGui::Button("Re-initialize sound pack");
	ImGui::Columns(3, "SoundPackColumns", true);
	ImGui::SetColumnWidth(0, 400.0f);
	ImGui::Separator();
	ImGui::Text("File");
	ImGui::NextColumn();
	ImGui::Text("Description");
	ImGui::NextColumn();
	ImGui::Text("Sounds");
	ImGui::NextColumn();
	ImGui::Separator();
	for (auto& soundPack : soundPacks)
	{
		auto& name = soundPack->GetFileName();
		bool isSelected = (_selectedPack == name);
		if (ImGui::Selectable(name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
			_selectedPack = name;

		ImGui::NextColumn();
		ImGui::Text("%s", soundPack->GetName().c_str());
		ImGui::NextColumn();
		ImGui::Text("%llu", soundPack->GetSounds().size());
		ImGui::NextColumn();
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("Sounds", ImVec2(ImGui::GetContentRegionAvail().x - 50, ImGui::GetContentRegionAvail().y), true);
	ImGui::Columns(5, "SoundColumns", true);
	ImGui::SetColumnWidth(0, 350.0f);
	ImGui::SetColumnWidth(1, 70.0f);
	ImGui::SetColumnWidth(2, 150.0f);
	ImGui::SetColumnWidth(3, 50.0f);
	ImGui::Separator();
	ImGui::Text("Name / Play");
	ImGui::NextColumn();
	ImGui::Text("Loaded");
	ImGui::NextColumn();
	ImGui::Text("Length in seconds");
	ImGui::NextColumn();
	ImGui::Text("Sectors merged");
	ImGui::NextColumn();
	ImGui::Text("Bytes");
	ImGui::NextColumn();
	ImGui::Separator();
	for (auto& soundPack : soundPacks)
	{
		if (_selectedPack != soundPack->GetFileName())
			continue;

		auto& sounds = soundPack->GetSounds();

		for (auto& [soundId, sound] : sounds)
		{
			if(ImGui::Selectable(soundId.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
				handler.PlaySound(soundId);

			ImGui::NextColumn();
			ImGui::TextColored(sound->loaded ? greenColor : redColor, sound->loaded ? "Yes" : "No");
			ImGui::NextColumn();
			auto length = sound->lengthInSeconds;
			ImGui::TextColored(length < 0 ? redColor : greenColor, "%s", length < 0 ? "N/A" : std::to_string(length).c_str());
			ImGui::NextColumn();
			ImGui::Text("%d", sound->sectorCount);
			ImGui::NextColumn();
			ImGui::Text("%llu", sound->bytes.size());
			ImGui::NextColumn();
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void AudioDebug::AudioSettings(Game& game) {
	auto& handler = game.GetSoundHandler();
	auto activeEmitters = handler.GetActiveEmitters();
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::BeginChild("Audio Handler Settings", ImVec2(500, ImGui::GetContentRegionAvail().y), true);
	ImGui::Text("Audio handler settings");
	ImGui::Text("Active Emitters");
	ImGui::Separator();
	ImGui::Columns(4, "PlayingEmitters", true);
	ImGui::Text("Sound ID");
	ImGui::NextColumn();
	ImGui::Text("Sound source ID");
	ImGui::NextColumn();
	ImGui::Text("Distance");
	ImGui::NextColumn();
	ImGui::Text("Age");
	ImGui::NextColumn();
	ImGui::Separator();
	for (auto emitter : activeEmitters)
	{
		ImGui::Text("%s", emitter.soundId.c_str());
		ImGui::NextColumn();
		ImGui::Text("%d", emitter.audioSourceId);
		ImGui::NextColumn();
		ImGui::Text("0");
		ImGui::NextColumn();
		ImGui::Text("N/A");
		ImGui::NextColumn();
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("Audio Loader Settings", ImVec2(500, ImGui::GetContentRegionAvail().y), true);
	ImGui::Separator();
	ImGui::Text("Audio loader settings");
	if (ImGui::Button(("Re-initialize Audio Loader (" + handler.GetAudioLoaderName() + ")").c_str()))
		handler.ReplaceAudioLoader(std::make_unique<FfmpegLoader>(FfmpegLoader()));
	ImGui::Separator();
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("Audio Player Settings", ImGui::GetContentRegionAvail(), true);
	ImGui::Text("Audio player settings");
	if(ImGui::Button(("Re-initialize Audio Player (" + handler.GetAudioPlayerName() + ")").c_str()))
		handler.ReplaceAudioPlayer(std::make_unique<OpenAlPlayer>(OpenAlPlayer()));
	ImGui::Separator();
	float volume = handler.GetGlobalVolume();
	ImGui::SliderFloat("Global Volume", &volume, 0.0f, 1.0f, "%.3f");
	ImGui::SliderFloat("Music Volume", &volume, 0.0f, 1.0f, "%.3f");
	ImGui::SliderFloat("SFX Volume", &volume, 0.0f, 1.0f, "%.3f");
	ImGui::Separator();
	if (volume != handler.GetGlobalVolume())
		handler.SetGlobalVolume(volume);
	ImGui::Text("Active Sounds");
	ImGui::Separator();
	ImGui::Columns(6, "PlayingSounds", true);
	ImGui::Text("Sound Source ID");
	ImGui::NextColumn();
	ImGui::Text("Name");
	ImGui::NextColumn();
	ImGui::Text("Buffer size");
	ImGui::NextColumn();
	ImGui::Text("Status");
	ImGui::NextColumn();
	ImGui::Text("Progress");
	ImGui::NextColumn();
	ImGui::Text("Priority");
	ImGui::NextColumn();
	ImGui::Separator();
	for (auto emitter : activeEmitters)
	{
		ImGui::Text("%d", emitter.audioSourceId);
		ImGui::NextColumn();
		ImGui::Text("%s", handler.GetSound(emitter.soundId)->id.c_str());
		ImGui::NextColumn();
		ImGui::Text("%llu", handler.GetSound(emitter.soundId)->bytes.size());
		ImGui::NextColumn();
		switch(handler.GetPlayer()->GetAudioStatus(emitter.audioSourceId))
		{
		case AudioStatus::Playing:
			ImGui::TextColored(greenColor, "Playing");
			break;
		case AudioStatus::Paused:
			ImGui::TextColored(redColor, "Paused");
			break;
		case AudioStatus::Stopped:
			ImGui::TextColored(redColor, "Stopped");
			break;
		}
		ImGui::NextColumn();
		ImGui::ProgressBar(handler.GetPlayer()->GetAudioProgress(emitter));
		ImGui::NextColumn();
		ImGui::Text("N/A");
		ImGui::NextColumn();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void AudioDebug::ShowDebugGui(Game& game)
{
	auto& handler = game.GetSoundHandler();
	auto& soundPacks = handler.GetSoundPacks();

	if (ImGui::Begin("Audio Manager", &game.GetConfig().showSoundPlayer))
	{
		ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;
		if (ImGui::BeginTabBar("Tabs", tabBarFlags))
		{
			if (ImGui::BeginTabItem("Audio Player"))
			{
				ImGui::Text("View sound packs and their contents");
				ImGui::Separator();
				AudioDebug::AudioPlayer(game, soundPacks);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Audio Emitters"))
			{
				ImGui::Text("Manage audio emitters and their properties");
				ImGui::Separator();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Music Player"))
			{
				ImGui::Text("Manage game music");
				ImGui::Separator();
				auto musicPacks = decltype(soundPacks) {};

				for (auto& soundPack : soundPacks)
				{
					if (soundPack->IsMusic())
						musicPacks.push_back(soundPack);
				}

				AudioDebug::AudioPlayer(game, musicPacks);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Audio Settings"))
			{
				ImGui::Text("Manage audio settings");
				ImGui::Separator();
				AudioDebug::AudioSettings(game);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
		ImGui::Separator();
	}
	ImGui::End();
}
