/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Music.h"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "Audio/Engine/MusicStream.h"
#include "Audio/Game/BankTables.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/Services/ScriptAudioState.h"
#include "EngineConfig.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::debug::gui;

namespace
{
const char* StatusName(MusicStatus status)
{
	switch (status)
	{
	case MusicStatus::Free:
		return "free";
	case MusicStatus::Playing:
		return "playing";
	case MusicStatus::Paused:
		return "paused";
	case MusicStatus::Finished:
		return "finished";
	case MusicStatus::LastQueued:
		return "last";
	}
	return "?";
}
} // namespace

Music::Music() noexcept
    : Window("Music", ImVec2(720.0f, 330.0f))
{
}

void Music::Draw() noexcept
{
	auto* system = music::Get();
	if (system == nullptr)
	{
		ImGui::TextUnformatted("No music (no OpenAL context)");
		return;
	}

	// The options dialog's slider: slider = (float)(main volume * (1 / 127)) when it opens, and a change sets the
	// main volume to (int)(slider * 127). The products in double give the original's exact results.
	if (Locator::config::has_value())
	{
		auto& config = Locator::config::value();
		const int mainVolume = system->With([](MusicEngine& engine) { return engine.GetMainVolume(); });
		float slider = static_cast<float>(static_cast<double>(mainVolume) * static_cast<double>(1.0f / 127.0f));
		if (ImGui::SliderFloat("Music volume", &slider, 0.0f, 1.0f))
		{
			config.audioMusicMainVolume =
			    static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(slider) * static_cast<double>(127.0f)));
		}
		ImGui::SameLine();
		ImGui::Text("AudioMusicMasterVolume %u", config.audioMusicMainVolume);
	}

	ImGui::SetNextItemWidth(320.0f);
	if (ImGui::BeginCombo("Music type", k_MusicBanks[static_cast<size_t>(_selectedType)].name.data()))
	{
		for (int i = 1; i < static_cast<int>(MusicType::_COUNT); ++i)
		{
			const bool selected = i == _selectedType;
			const auto label = std::to_string(i) + " " + std::string(k_MusicBanks[static_cast<size_t>(i)].name);
			if (ImGui::Selectable(label.c_str(), selected))
			{
				_selectedType = i;
			}
		}
		ImGui::EndCombo();
	}
	const auto play = [this, system](int sync, int fade) {
		MusicPlayOptions options {
		    .bank = music::GetBank(static_cast<MusicType>(_selectedType)),
		    .sync = sync,
		    .fade = fade,
		};
		system->With([&options](MusicEngine& engine) { return engine.Play(options); });
	};
	if (ImGui::Button("Play"))
	{
		play(0, 0);
	}
	ImGui::SameLine();
	if (ImGui::Button("Play (sync, fade)"))
	{
		play(1, 1);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop (fade)"))
	{
		system->With([](MusicEngine& engine) { engine.Stop(1); });
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop (cut)"))
	{
		system->With([](MusicEngine& engine) { engine.Stop(0); });
	}

	if (ImGui::BeginTable("MusicChannels", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Ch");
		ImGui::TableSetupColumn("Bank");
		ImGui::TableSetupColumn("Status");
		ImGui::TableSetupColumn("Cur");
		ImGui::TableSetupColumn("Target");
		ImGui::TableSetupColumn("Sad");
		ImGui::TableSetupColumn("Chunk");
		ImGui::TableSetupColumn("Queued");
		ImGui::TableSetupColumn("Loops");
		ImGui::TableHeadersRow();
		system->With([](MusicEngine& engine) {
			for (int i = 0; i < k_MusicChannelCount; ++i)
			{
				const auto& ch = engine.GetChannel(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%d%s", i, engine.GetMainChannel() == i ? " M" : "");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ch.bank != nullptr ? ch.bank->GetPath().filename().string().c_str() : "-");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(StatusName(ch.status));
				ImGui::TableNextColumn();
				ImGui::Text("%d", ch.current);
				ImGui::TableNextColumn();
				ImGui::Text("%d", ch.target);
				ImGui::TableNextColumn();
				ImGui::Text("%d", ch.sadVolume);
				ImGui::TableNextColumn();
				ImGui::Text("%u/%u", ch.playingChunk, ch.chunkCount);
				ImGui::TableNextColumn();
				ImGui::Text("%d", engine.GetQueued(i));
				ImGui::TableNextColumn();
				ImGui::Text("%d", ch.loops);
			}
		});
		ImGui::EndTable();
	}

	// The game's music (Audio/Services/GameMusic.h) and the script's switches (Audio/Services/ScriptAudioState.h)
	const auto lock = game_music::Lock();
	if (auto* gameMusic = game_music::Get(); gameMusic != nullptr)
	{
		const auto& script = GetScriptAudioState();
		ImGui::Separator();
		ImGui::Text("%.*s", static_cast<int>(gameMusic->GetPlayingMessage().size()), gameMusic->GetPlayingMessage().data());
		ImGui::Text("script type %d  started %d | alignment type %d  silence turns %u  finished type %d  town %s",
		            gameMusic->GetScriptType(), gameMusic->GetScriptStarted(), gameMusic->GetAlignmentType(),
		            gameMusic->GetSilenceTurns(), gameMusic->GetFinishedType(),
		            gameMusic->GetCurrentTown() ? std::to_string(*gameMusic->GetCurrentTown()).c_str() : "-");
		std::string positions;
		for (const int position : gameMusic->GetGroupPositions())
		{
			positions += std::to_string(position) + " ";
		}
		ImGui::Text("group positions: %s", positions.c_str());
		ImGui::Text("script: creature sound %d  game sound off %d  alignment music %d  music line %u  music beat %d",
		            script.creatureSound.load(), script.gameSoundOff.load(), script.alignmentMusic.load(),
		            script.musicLine.load(), script.musicBeat.load());
		for (const auto& info : gameMusic->GetThingMusic().GetInfos())
		{
			ImGui::Text("thing %u: %s enabled %d finished %d started %d%s", info.thing,
			            k_MusicBanks[static_cast<size_t>(std::clamp(info.type, 0, static_cast<int>(MusicType::_COUNT) - 1))]
			                .name.data(),
			            info.enabled, info.finished, info.started, info.hasPlayPosition != 0 ? " (play position)" : "");
		}
	}
}

void Music::Update() noexcept {}

void Music::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Music::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
