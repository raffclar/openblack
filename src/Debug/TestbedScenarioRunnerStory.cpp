/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the commands of the story and the world: the films, the temple, the help, the saves, the towns and the
// advisors.

#include <cmath>

#include <limits>
#include <string>

#include <fmt/format.h>

#include "3D/FlatLand.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ScreenPick.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/DoubleClickFlight.h"
#include "Commands/PlayerCommands.h"
#include "Commands/TurnCodec.h"
#include "Common/FileDialog.h"
#include "Common/PortableMath.h"
#include "Creature/CaveCreature.h"
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
#include "Debug/TownMiracles.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Gui/GameTexts.h"
#include "Help/Spirits.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "PlayerSlots.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"
#include "Visual/RendererRequestsInterface.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;

bool Runner::GiveStoryCommand(const Command& command)
{
	if (command.kind == Kind::PlayBoatFilm)
	{
		Log(fmt::format("{:.1f}s: {}", _seconds, PlayBoatFilm(command)));
		return true;
	}
	if (command.kind == Kind::EnterTemple)
	{
		const auto room = static_cast<TempleRoom>(command.value);
		if (Locator::temple::has_value())
		{
			auto& temple = Locator::temple::value();
			// In the temple, no room is a walk through the main room's way out
			if (temple.Active() && room == TempleRoom::Unknown)
			{
				temple.EnterRoom(room);
			}
			else if (temple.Active())
			{
				temple.GoToRoom(room);
			}
			else
			{
				temple.ActivateAfterFrame(room);
			}
		}
		Log(fmt::format("{:.1f}s: the player goes into temple room {}", _seconds, command.value));
		return true;
	}
	if (command.kind == Kind::Reframe)
	{
		Frame(_scenario->framing.shot, _scenario->framing.creature, _scenario->framing.distance);
		Log(fmt::format("{:.1f}s: the camera is put back", _seconds));
		return true;
	}
	if (command.kind == Kind::RestoreDeviceInDomeFollow)
	{
		_deviceRestoreAsked =
		    command.value == 1 ? graphics::device_restore::Request::Lose : graphics::device_restore::Request::MakeAgain;
		Log(fmt::format("{:.1f}s: the graphics device is made again when the dome next follows the sky", _seconds));
		return true;
	}
	if (command.kind == Kind::RestoreDevice)
	{
		const bool lose = command.value == 1;
		if (Locator::rendererRequests::has_value())
		{
			Locator::rendererRequests::value().RequestDeviceRestore(lose ? graphics::device_restore::Request::Lose
			                                                             : graphics::device_restore::Request::MakeAgain);
		}
		Log(fmt::format("{:.1f}s: the graphics device is {}", _seconds, lose ? "lost and made again" : "made again"));
		return true;
	}
	if (command.kind == Kind::PutLoadedArena)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), PutLoadedArena(command)));
		return true;
	}
	if (command.kind == Kind::SetHour)
	{
		if (Locator::skyClockSystem::has_value())
		{
			Locator::skyClockSystem::value().SetTime(command.hour);
		}
		Log(fmt::format("{:.1f}s: the hour is {:.1f}", _seconds, command.hour));
		return true;
	}
	if (command.kind == Kind::Advisor)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveAdvisorCommand(command)));
		return true;
	}
	if (command.kind == Kind::SetOpenClose || command.kind == Kind::LayGateStone)
	{
		GiveSceneryCommand(command);
		return true;
	}
	if (command.kind == Kind::SetAlignment)
	{
		if (Locator::alignmentSystem::has_value())
		{
			Locator::alignmentSystem::value().SetPlayerAlignment(command.player, command.alignment);
		}
		Log(fmt::format("{:.1f}s: the alignment is {:.2f}", _seconds, command.alignment));
		return true;
	}
	if (command.kind == Kind::SaveGame || command.kind == Kind::LoadGame)
	{
		const auto slot = static_cast<uint32_t>(command.value);
		const bool saving = command.kind == Kind::SaveGame;
		bool done = false;
		if (Locator::saveGameSystem::has_value())
		{
			auto& saves = Locator::saveGameSystem::value();
			done = saving ? saves.Save(slot, u"Testbed") : saves.Load(slot);
		}
		Log(fmt::format("{:.1f}s: {} slot {}: {}", _seconds, saving ? "save into" : "load from", slot,
		                done ? "done" : "failed"));
		return true;
	}
	if (command.kind == Kind::PlayLogoEffect)
	{
		if (_stage != nullptr)
		{
			_stage->PlayLogoEffect();
		}
		Log(fmt::format("{:.1f}s: the logo effect played", _seconds));
		return true;
	}
	if (command.kind == Kind::ContinueGame)
	{
		const bool loaded = _stage != nullptr && _stage->ContinueFromExitSave();
		Log(fmt::format("{:.1f}s: continue from the save on exit: {}", _seconds,
		                loaded ? "loaded, the menu shows" : "a new game"));
		return true;
	}
	if (command.kind == Kind::QuickSave)
	{
		// As the key: the key checks the rules, and the command saves at the next turn
		const bool sent = Locator::saveGameSystem::has_value() && Locator::playerCommandSystem::has_value() &&
		                  Locator::saveGameSystem::value().PressQuickSave();
		if (sent)
		{
			Locator::playerCommandSystem::value().Submit(player_commands::QuickSave {});
		}
		Log(fmt::format("{:.1f}s: quick save: {}", _seconds, sent ? "sent" : "refused"));
		return true;
	}
	if (command.kind == Kind::SetTownBeliefInRival)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				magic::town_belief::SetInPlayer(impression->belief, PlayerNames::PLAYER_TWO, command.amount);
			}
		}
		Log(fmt::format("{:.1f}s: the town believes {:.2f} in the second player", _seconds, command.amount));
		return true;
	}
	if (command.kind == Kind::GiveLandTown)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto id = ecs::script_find::TownIdFromScript(static_cast<float>(command.value));
		const auto& towns = registry.Context().towns;
		auto* impression = [&]() -> ecs::components::TownImpression* {
			const auto town = towns.find(id);
			return town != towns.end() && registry.Valid(town->second)
			           ? registry.TryGet<ecs::components::TownImpression>(town->second)
			           : nullptr;
		}();
		if (impression != nullptr)
		{
			for (size_t i = 0; i < magic::town_belief::k_PlayerCount; ++i)
			{
				const auto other = static_cast<PlayerNames>(i);
				magic::town_belief::Set(impression->belief, other, other == command.player ? command.amount : 0.0f);
			}
		}
		Log(fmt::format("{:.1f}s: the land's town {} believes {:.2f} in the player{}", _seconds, command.value, command.amount,
		                impression != nullptr ? "" : " (no such town)"));
		return true;
	}
	if (command.kind == Kind::EndGuidanceWaits)
	{
		if (Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().EndWaits();
		}
		Log(fmt::format("{:.1f}s: the guidance's waits are over", _seconds));
		return true;
	}
	if (command.kind == Kind::RunHelpMessage)
	{
		const bool started =
		    Locator::helpMessageSystem::has_value() && Locator::helpMessageSystem::value().RunMessageSet(command.value);
		Log(fmt::format("{:.1f}s: the help message set {} {}", _seconds, command.value, started ? "runs" : "doesn't run"));
		return true;
	}
	if (command.kind == Kind::DestroyTemple)
	{
		// The scenario's temple, or the land's temple of the god of the slot when the scenario made no temples
		std::optional<entt::entity> temple;
		if (command.value < _temples.size())
		{
			temple = _temples.at(command.value);
		}
		else if (const auto god = player_slots::PlayerOfSlot(command.value); _temples.empty() && god.has_value())
		{
			Locator::entitiesRegistry::value().Each<const ecs::components::Temple>(
			    [&temple, god](entt::entity entity, const ecs::components::Temple& each) {
				    if (each.owner == *god)
				    {
					    temple = entity;
				    }
			    });
		}
		const bool known = temple.has_value();
		if (known && Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(*temple);
		}
		Log(fmt::format("{:.1f}s: temple {} {}", _seconds, command.value, known ? "starts being destroyed" : "isn't there"));
		return true;
	}
	if (command.kind == Kind::LookAtTempleScroll)
	{
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LookAtScroll(command.value);
		}
		Log(fmt::format("{:.1f}s: the temple's camera looks at scroll {}", _seconds, command.value));
		return true;
	}
	if (command.kind == Kind::LookAtSaveGamePicture)
	{
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LookAtSaveGamePicture(static_cast<uint32_t>(command.value));
		}
		Log(fmt::format("{:.1f}s: the temple's camera looks at the picture of save slot {}", _seconds, command.value));
		return true;
	}
	if (command.kind == Kind::ReadTip)
	{
		// A sign far under the land, tapped once by the player
		const auto text = static_cast<uint32_t>(command.value);
		const auto category = static_cast<uint32_t>(std::lround(command.amount));
		const bool read = Locator::scriptHighlightSystem::has_value();
		if (read)
		{
			auto& highlights = Locator::scriptHighlightSystem::value();
			const auto sign =
			    highlights.Create(static_cast<uint32_t>(HighlightInfo::DidYouKnowSign), {0.0f, -1000.0f, 0.0f}, 0);
			highlights.SetProperties(sign, text, category);
			highlights.Tap(sign, true);
		}
		Log(fmt::format("{:.1f}s: tip {} of category {} {}", _seconds, text, category, read ? "read" : "not read"));
		return true;
	}
	if (command.kind == Kind::RunText)
	{
		// Not alone on its line and not waiting for a click, as the lessons' texts are shown
		const bool shown = Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().IsStarted() &&
		                   Locator::helpTextSystem::value().RunText(false, static_cast<uint32_t>(command.value), 0);
		Log(fmt::format("{:.1f}s: help text {} {}", _seconds, command.value, shown ? "shown" : "not shown"));
		return true;
	}
	if (command.kind == Kind::PlayHandDemo)
	{
		if (Locator::handDemoSystem::has_value())
		{
			Locator::handDemoSystem::value().Play(command.handDemo, 0, false, false);
		}
		Log(fmt::format("{:.1f}s: the hand demonstration {} plays", _seconds, command.handDemo));
		return true;
	}
	if (command.kind == Kind::FlyToBookmark)
	{
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 focus {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const glm::vec3 origin = focus + glm::vec3(0.0f, 50.0f, -80.0f);
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().SetBookmark(static_cast<size_t>(command.value), focus, std::nullopt, origin);
		}
		Locator::camera::value().GetModel().SetFlight(origin, focus, camera_fly::k_BookmarkRise);
		Log(fmt::format("{:.1f}s: the camera flies to bookmark {} at ({:.0f}, {:.0f}, {:.0f})", _seconds, command.value + 1,
		                focus.x, focus.y, focus.z));
		return true;
	}
	if (command.kind == Kind::TabToTown)
	{
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().FlyToNextTown(std::nullopt);
		}
		Log(fmt::format("{:.1f}s: the player presses the Tab key", _seconds));
		return true;
	}
	if (command.kind == Kind::SetInterfaceLevel)
	{
		if (Locator::cameraHelpSystem::has_value())
		{
			auto& help = Locator::cameraHelpSystem::value().Get();
			const bool known = help.SetInterfaceLevel(static_cast<int32_t>(command.value));
			if (known && Locator::playerSystem::has_value())
			{
				script::interface_levels::Apply(
				    Locator::entitiesRegistry::value(),
				    Locator::playerSystem::value().FindPlayer(PlayerNames::PLAYER_ONE).value_or(entt::null),
				    static_cast<int32_t>(command.value));
			}
			Log(fmt::format("{:.1f}s: the interface level is {}{}, the hand reaches {:.0f}", _seconds, command.value,
			                known ? "" : " (a level the game does not know)", help.handReach));
		}
		return true;
	}
	if (command.kind == Kind::PlayIntroFilm)
	{
		if (Locator::videoSystem::has_value())
		{
			auto& videos = Locator::videoSystem::value();
			videos.Play("Data/INTRO.bik");
			videos.ScheduleIntro();
		}
		Log(fmt::format("{:.1f}s: the intro film plays", _seconds));
		return true;
	}
	if (command.kind == Kind::PlayFallingSpellFilm)
	{
		// The script's film starts and the script's fade goes at once
		if (Locator::videoSystem::has_value())
		{
			Locator::videoSystem::value().StartFallingSpell();
		}
		if (Locator::cinematicDirectorSystem::has_value())
		{
			Locator::cinematicDirectorSystem::value().FadeBackToNormal(0);
		}
		Log(fmt::format("{:.1f}s: the falling spell's film starts", _seconds));
		return true;
	}
	if (command.kind == Kind::WideScreen)
	{
		// Held by a script, as the land's scripts hold them, under a number no script task has: a task of that number
		// stopping would take the bars away with it
		constexpr uint32_t k_ScriptOwner = std::numeric_limits<uint32_t>::max();
		if (Locator::cinematicDirectorSystem::has_value())
		{
			Locator::cinematicDirectorSystem::value().SetWideScreen(command.value != 0, k_ScriptOwner);
		}
		Log(fmt::format("{:.1f}s: the cinema bars slide {}", _seconds, command.value != 0 ? "in" : "out"));
		return true;
	}
	// With a value of 1, the scenario's object catches fire, with or without a creature. With a value of 2, it catches
	// at the most heat of its fire, so that it burns for a time.
	if (command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2))
	{
		const auto burning = ObjectAt(command.object);
		if (Locator::fireSystem::has_value() && burning.has_value())
		{
			Locator::fireSystem::value().SetOnFire(*burning, command.value == 2 ? 1.0f : 0.0f);
		}
		Log(fmt::format("{:.1f}s: object {} {}", _seconds, command.object,
		                Locator::fireSystem::has_value() && burning.has_value() ? "alight" : "no fire"));
		return true;
	}
	return false;
}

void Runner::GiveSceneryCommand(const Command& command)
{
	const auto object = ObjectAt(command.object);
	if (!object.has_value() || !Locator::animatedStaticSystem::has_value())
	{
		Log(fmt::format("{:.1f}s: no such object", _seconds));
		return;
	}
	auto& scenery = Locator::animatedStaticSystem::value();
	if (command.kind == Kind::SetOpenClose)
	{
		const bool done = scenery.SetOpenState(*object, static_cast<int32_t>(command.value));
		Log(fmt::format("{:.1f}s: {} object {}{}", _seconds, command.value == 1 ? "opened" : "closed", command.object,
		                done ? "" : ", which isn't animated scenery"));
		return;
	}
	// As the hand gives it: laid in the plinth, the stone is used up, leaving its ghost
	const auto plinth = ObjectAt(command.value);
	if (plinth.has_value() && scenery.LayGateStone(*plinth, *object))
	{
		ecs::world_objects::LeaveGhost(*object);
		ecs::world_objects::Remove(*object);
		Log(fmt::format("{:.1f}s: object {} laid in the plinth, now worth {}", _seconds, command.object,
		                scenery.GateStoneValue(*plinth).value_or(0)));
		return;
	}
	Log(fmt::format("{:.1f}s: object {} isn't taken by object {}", _seconds, command.object, command.value));
}

std::string Runner::PlayBoatFilm(const Command& command) const
{
	if (!Locator::boatLaunchSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return "no boat film";
	}
	const auto dock = MapPoint(_middle, command.point);
	// The sailing clip starts the boat about 67 metres west and 59 south of where it is played from: played from there,
	// the boat sets off from the lake's north-east corner and crosses it to the south west
	constexpr glm::vec2 k_SailingClipStart {-67.3f, -59.4f};
	const auto corner = flat_land::k_LakeCentre + flat_land::k_LakeHalfExtent;
	const auto sailing = corner - k_SailingClipStart;
	const ecs::systems::BoatLaunchSystemInterface::Places places {
	    .dock = {dock.x, Locator::terrainSystem::value().GetHeightAt(dock), dock.y},
	    .sailingStart = {sailing.x, 0.0f, sailing.y},
	};
	Locator::boatLaunchSystem::value().Play(places);
	return fmt::format("the explorers' boat film plays from ({:.0f}, {:.0f})", dock.x, dock.y);
}

std::string Runner::RecordChallenge(const Command& command) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return "no land";
	}
	// The camera is put over the place first, and the picture is of the frame drawn from there
	const auto at = MapPoint(_middle, command.point);
	const auto focus = glm::vec3(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
	const auto position = focus + glm::vec3(0.0f, 50.0f, 70.0f);
	auto& camera = Locator::camera::value();
	camera.SetOrigin(position);
	camera.SetFocus(focus);
	script::challenge_snapshots::Snapshot snapshot;
	snapshot.challenge = static_cast<uint32_t>(command.value);
	snapshot.reminderScript = "";
	snapshot.success = command.amount;
	snapshot.alignment = command.alignment;
	snapshot.position = position;
	snapshot.focus = focus;
	snapshot.quest = command.ctrl;
	// The title is the number of a help text. The dedicated server has no game interface, so it reads the names from
	// the same text scripts. Thus the server and the game record the same title.
	const auto* shown = _stage != nullptr ? _stage->GetTexts() : nullptr;
	const auto loaded = shown == nullptr ? std::optional(gui::LoadGameTexts()) : std::nullopt;
	const auto& texts = shown != nullptr ? *shown : *loaded;
	if (const auto number = HelpTextNumber(texts.GetHelpNames(), command.title); number.has_value())
	{
		snapshot.title = *number;
	}
	chlapi::CHLApi::RecordChallenge(snapshot, script::challenge_snapshots::Call::Start);
	return fmt::format("challenge {} ({}), {:.0f}% done{}", command.value, command.title, command.amount * 100.0f,
	                   command.ctrl ? ", a quest" : "");
}

std::string Runner::GiveAdvisorCommand(const Command& command) const
{
	if (!Locator::advisorSystem::has_value())
	{
		return "no advisors";
	}
	auto& control = Locator::advisorSystem::value().GetController();
	// The scripts name the good advisor 1 and the evil one 2
	const int32_t type = command.value == 0 ? 1 : 2;
	const auto who = command.value == 0 ? "the good advisor" : "the evil advisor";
	using Action = Command::AdvisorAction;
	switch (command.advisor)
	{
	case Action::Out:
		control.SpiritEject(type, false);
		return fmt::format("{} comes out", who);
	case Action::Appear:
		control.SpiritEject(type, true);
		return fmt::format("{} appears", who);
	case Action::Home:
		control.SpiritHome(type, false);
		return fmt::format("{} goes home", who);
	case Action::Vanish:
		control.SpiritHome(type, true);
		return fmt::format("{} vanishes", who);
	case Action::Cling:
		control.SpiritCling(type, command.point.x, command.point.y);
		return fmt::format("{} clings at ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::Fly:
		control.SpiritFly(type, command.point.x, command.point.y);
		return fmt::format("{} flies to ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::PointOnScreen:
	{
		const auto& screen = control.GetScreen();
		control.SpiritScreenPoint(type, glm::ivec2(static_cast<int32_t>(static_cast<float>(screen.width) * command.point.x),
		                                           static_cast<int32_t>(static_cast<float>(screen.height) * command.point.y)));
		return fmt::format("{} points at ({:.2f}, {:.2f}) on the screen", who, command.point.x, command.point.y);
	}
	case Action::PointAtLand:
	case Action::LookAtLand:
	{
		const auto at = MapPoint(_middle, command.point);
		const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const glm::vec3 position(at.x, height, at.y);
		if (command.advisor == Action::LookAtLand)
		{
			control.SpiritLookAtPosition(type, position);
			return fmt::format("{} looks at ({:.0f}, {:.0f})", who, at.x, at.y);
		}
		control.SpiritPointPosition(type, position, command.gentle);
		return fmt::format("{} points at ({:.0f}, {:.0f}){}", who, at.x, at.y, command.gentle ? " out in the world" : "");
	}
	case Action::PlayAnim:
		control.SpiritPlayAnim(type, command.point.x, command.point.y, command.anim, command.amount);
		return fmt::format("{} plays {}", who, help::spirits::AnimName(command.anim));
	case Action::Feel:
		control.Dude(type == 1 ? help::spirits::k_GoodDude : help::spirits::k_EvilDude).SetEmotion(command.anim, 1.0f);
		return fmt::format("{} feels {}", who, help::spirits::EmotionName(command.anim));
	}
	return "";
}
