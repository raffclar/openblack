/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the commands of the front end: the menus, the boxes, the chat, the music and the profile.

#include <cmath>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <MindFile.h>
#include <fmt/format.h>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Commands/PlayerCommands.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/FightAutopilotInterface.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Weather.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/GameTexts.h"
#include "Gui/TextDatabase.h"
#include "Help/HelpProfileFile.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "PlayerSlots.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/GameMusic.h"
#include "Sound/MusicMood.h"
#include "Sound/SoundRequestInterface.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;

namespace
{
/// The player has grey rings of their own for a design, which the tattoo editor puts in the creature's block of
/// pictures when a tattoo is dropped
std::string GiveOwnTattooPicture(size_t design)
{
	if (!Locator::tattooEditorSystem::has_value() || design >= creature_tattoo::k_DesignCount)
	{
		return "no tattoo editor or no such design";
	}
	creature_tattoo::Picture rings {};
	constexpr int k_Middle = static_cast<int>(creature_tattoo::k_DesignSize) / 2;
	for (int y = 0; y < static_cast<int>(creature_tattoo::k_DesignSize); ++y)
	{
		for (int x = 0; x < static_cast<int>(creature_tattoo::k_DesignSize); ++x)
		{
			const auto distance = static_cast<int>(
			    std::sqrt(static_cast<float>(((x - k_Middle) * (x - k_Middle)) + ((y - k_Middle) * (y - k_Middle)))));
			const auto index = static_cast<size_t>((y * static_cast<int>(creature_tattoo::k_DesignSize)) + x);
			rings.at(index) = distance < k_Middle && (distance / 6) % 2 == 0 ? uint8_t {255} : uint8_t {0};
		}
	}
	auto& editor = Locator::tattooEditorSystem::value();
	auto pictures = editor.GetPictures();
	pictures.at(design) = rings;
	editor.SetPictures(pictures);
	return fmt::format("design {} has the player's own picture", design + 1);
}

/// A message comes into the list of incoming messages: the message of the real weather with a picture, a chat line or an
/// e-mail
std::string AddIncomingMessage(size_t value, std::string_view text)
{
	if (!Locator::incomingMessageSystem::has_value())
	{
		return "failed: no list of incoming messages";
	}
	auto& messages = Locator::incomingMessageSystem::value();
	if (value == k_IncomingChat || value == k_IncomingMail)
	{
		const auto kind = value == k_IncomingChat ? incoming_messages::Kind::Chat : incoming_messages::Kind::Mail;
		messages.Add(kind, gui::ToUtf16(text), 0);
	}
	else if (value < incoming_messages::k_WeatherPictures)
	{
		messages.AddWeather(gui::ToUtf16(text), static_cast<uint8_t>(value));
	}
	else
	{
		return fmt::format("failed: no message for the value {}", value);
	}
	return fmt::format("{} messages in the list, the top one \"{}\"", messages.GetQueue().Count(),
	                   gui::ToUtf8(messages.GetQueue().ShownText()));
}

/// The chat of a network game: a god says a line, or the chat of this computer opens with a line in it
std::string Chat(GameStageInterface* stage, PlayerNames player, size_t value, std::string_view text)
{
	if (!Locator::chatSystem::has_value() || stage == nullptr || !stage->HasInterface())
	{
		return "failed: no chat";
	}
	auto& chat = Locator::chatSystem::value();
	// The god of this computer and the god that speaks have the names of the testbed
	const auto own = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	auto names = chat.GetNames();
	for (const auto god : {own, player})
	{
		if (static_cast<size_t>(god) < k_ChatNames.size())
		{
			names.try_emplace(god, k_ChatNames.at(static_cast<size_t>(god)));
		}
	}
	chat.SetNames(names);
	const auto line = gui::ToUtf16(text);
	if (value == k_ChatFriends)
	{
		return stage->OpenChatWithFriends();
	}
	if (value == k_ChatStaffSays || value == k_ChatAnnouncement)
	{
		return stage->PingServerSays(value, line);
	}
	if (value == k_ChatTypes)
	{
		stage->OpenChat(line);
		return fmt::format("the chat opens with \"{}\"", text);
	}
	if (value != k_ChatSays)
	{
		return fmt::format("failed: no chat for the value {}", value);
	}
	// The line of this computer goes through the turns, and the line of another god comes in as the turns give it
	if (player == own)
	{
		// The line goes to the other named gods
		std::vector<PlayerNames> others;
		for (const auto& [god, name] : names)
		{
			if (god != own)
			{
				others.push_back(god);
			}
		}
		chat.SendLine(line, others);
		return fmt::format("this computer says \"{}\"", text);
	}
	chat.Receive(player, player_commands::ChatSpeech {.recipients = {}, .text = line});
	return fmt::format("god {} says \"{}\"", static_cast<int>(player), text);
}

/// The game writes the help record of the player to their folder in the profile folder, then reads it again
std::string CheckProfileFolder()
{
	if (!Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value() ||
	    !Locator::playerProfileSystem::has_value())
	{
		return "failed: no profile folder";
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return "failed: no player profile";
	}
	const auto player = gui::ToUtf16(profile->name);
	auto& folder = Locator::profileFolderSystem::value();
	auto& record = Locator::helpProfileSystem::value();
	const auto written = help::profile::ToFileBytes(record.Get());
	if (!record.SaveRecord(folder, player))
	{
		return "failed: the game could not write the help record";
	}
	const auto read = folder.ReadPlayerFile(player, help::profile::k_FileName);
	const auto path = (*folder.GetPlayerFolder(player) / help::profile::k_FileName).generic_string();
	if (!read.has_value() || *read != written)
	{
		return fmt::format("failed: {} does not hold the help record", path);
	}
	record.LoadRecord(folder, player);
	if (help::profile::ToFileBytes(record.Get()) != written)
	{
		return "failed: the help record changed when the game read it again";
	}
	return fmt::format("passed: {} holds the help record ({} bytes) and reads back the same", path, read->size());
}

/// The custom music player plays a track of the custom music folder, or stops; the music moods look at once
std::string PlayCustomMusic(size_t track)
{
	auto* player = Locator::sounds::has_value() ? Locator::sounds::value().GetCustomMusic() : nullptr;
	if (player == nullptr || !Locator::musicMoodSystem::has_value())
	{
		return "failed: no custom music player";
	}
	auto& moods = Locator::musicMoodSystem::value();
	if (track == k_StopCustomMusic)
	{
		player->Stop();
		moods.UpdateOnGameTurn(true);
		return "the custom music stops";
	}
	moods.LoadFolder();
	if (track >= player->GetTracks().size())
	{
		return fmt::format("failed: the custom music folder has {} tracks", player->GetTracks().size());
	}
	player->SetCurrentTrack(track);
	player->Play();
	const auto id = music_mood::TrackId(player->GetSongName());
	const auto records = moods.GetRecords();
	const auto record = std::ranges::find(records, id, &music_mood::Record::id);
	const auto mood = record != records.end() && !record->emotions.empty() ? record->emotions.front().mood : 0.0f;
	moods.UpdateOnGameTurn(true);
	return fmt::format("plays {} (track {:08x}), the creature's mood for it {:.3f}", player->GetSongName(), id, mood);
}
} // namespace

bool Runner::GiveFrontEndCommand(const Command& command)
{
	if (command.kind == Kind::OpenTattooMenu)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenTattooMenu() : "failed: no menu")));
		return true;
	}
	if (command.kind == Kind::ShowDemoPing)
	{
		_demoPingSince = command.value != 0 ? std::optional(_seconds) : std::nullopt;
		if (!_demoPingSince.has_value() && _stage != nullptr)
		{
			_stage->SetDemoRoundTrip(std::nullopt);
		}
		UpdateDemoPing();
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _demoPingSince.has_value() ? "shown" : "gone"));
		return true;
	}
	if (command.kind == Kind::TattooEditorAsNetworkGame)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->TattooEditorAsNetworkGame(command.value != 0) : "failed: no interface")));
		return true;
	}
	if (command.kind == Kind::OpenControlsBox)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenControlsBox(command.value) : "failed: no menu")));
		return true;
	}
	if (command.kind == Kind::JoinOnline)
	{
		const bool asked = _stage != nullptr && _stage->RequestJoinOnline();
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), asked ? "asked" : "failed: no game"));
		return true;
	}
	if (command.kind == Kind::Chat)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                Chat(_stage, command.player, command.value, command.action)));
		return true;
	}
	if (command.kind == Kind::AddIncomingMessage)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), AddIncomingMessage(command.value, command.action)));
		return true;
	}
	if (command.kind == Kind::OpenMultiplayerBox)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenMultiplayerBox(command.value) : "failed: no interface")));
		return true;
	}
	if (command.kind == Kind::SkirmishBox)
	{
		if (command.value == 5 || command.value == 6)
		{
			// The game's own mind file takes the place of the creature that the profile keeps. This needs no interface, thus
			// the dedicated server brings the creature too.
			const auto path =
			    Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / std::string(command.mindFile);
			auto mind = std::make_shared<creaturemind::MindFileData>();
			const bool read = creaturemind::ReadFile(path, *mind) == creaturemind::MindResult::Success;
			const auto brought = ecs::creature_from_file::BringKept(PlayerNames::PLAYER_ONE, read ? mind : nullptr);
			// With 6, the profile also keeps the new creature in its file. Thus a load of a saved skirmish brings it again.
			if (command.value == 6 && brought.creature.has_value() && Locator::creatureCarryOverSystem::has_value())
			{
				Locator::creatureCarryOverSystem::value().KeepCreature(*brought.creature);
			}
			Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
			                brought.creature.has_value()
			                    ? fmt::format("the player's god brings creature {}", entt::to_integral(*brought.creature))
			                    : fmt::format("no creature, reason {}", static_cast<int>(brought.refusal))));
			return true;
		}
		if (_stage == nullptr || !_stage->HasInterface())
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return true;
		}
		if (command.value == 1 || command.value == 4)
		{
			Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->OpenSkirmishMenu(command.value == 4)));
			return true;
		}
		if (command.value == 3)
		{
			// The two lines of the script of Two Gods
			auto& lands = Locator::landSystem::value();
			lands.ShowLandMessage({.kind = land_message::Message::Kind::Start, .text = "Two Gods", .number = 2});
			lands.ShowLandMessage(
			    {.kind = land_message::Message::Kind::AddLine, .text = "Battle another god for this realm", .number = 2});
			Log(fmt::format("{:.1f}s: {}: the land information box shows", _seconds, Name(command.kind)));
			return true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->OpenSkirmishBox(command.value == 2)));
		return true;
	}
	if (command.kind == Kind::EndBox)
	{
		if (_stage == nullptr || !_stage->HasInterface())
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->OpenEndBox(command.value)));
		return true;
	}
	if (command.kind == Kind::OpenMenuPage)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenMenuPage(command.value, command.ctrl) : "failed: no menu")));
		return true;
	}
	if (command.kind == Kind::PressMenuKey)
	{
		Log(fmt::format(
		    "{:.1f}s: {}: {}", _seconds, Name(command.kind),
		    (_stage != nullptr ? _stage->PressMenuKey(command.value, command.ctrl) : "failed: the menu is not open")));
		return true;
	}
	if (command.kind == Kind::ShowCustomMusicControls)
	{
		if (_stage == nullptr || !_stage->HasInterface())
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->ShowCustomMusicControls(command.value)));
		return true;
	}
	if (command.kind == Kind::StartScriptMusic)
	{
		if (command.value >= static_cast<size_t>(audio::MusicType::_COUNT))
		{
			Log(fmt::format("{:.1f}s: {}: no music type {}", _seconds, Name(command.kind), command.value));
			return true;
		}
		const auto type = static_cast<audio::MusicType>(command.value);
		if (Locator::scriptMusicSystem::has_value())
		{
			Locator::scriptMusicSystem::value().ResetLyrics();
			Locator::scriptMusicSystem::value().Start(type);
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), audio::GetMusicTypeName(type)));
		return true;
	}
	if (command.kind == Kind::PlayCustomMusic)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), PlayCustomMusic(command.value)));
		return true;
	}
	if (command.kind == Kind::OwnTattooPicture)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveOwnTattooPicture(command.value)));
		return true;
	}
	if (command.kind == Kind::ProfileFolderCheck)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), CheckProfileFolder()));
		return true;
	}
	return false;
}
