/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlaygroundTripSystem.h"

#include <cstdint>

#include <exception>
#include <string>
#include <string_view>
#include <tuple>

#include <SaveSlots.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Camera/CameraDrag.h"
#include "Camera/CameraHelp.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "LHVM.h"
#include "LandChange.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptStart.h"

using namespace openblack::ecs::systems;

namespace
{

/// The interface level at which the player can do everything
constexpr int32_t k_NormalInterface = 0;

void StartScript(std::string_view name)
{
	if (openblack::Locator::vm::has_value())
	{
		openblack::Locator::vm::value().StartScript(std::string(name), openblack::script::start::StartableScripts());
	}
}

void StopEveryScript()
{
	if (openblack::Locator::vm::has_value())
	{
		openblack::Locator::vm::value().StopAllTasks();
	}
}

/// The player's records of challenges go, and the things that they held are no longer kept alive by them
void ClearChallengeRecords()
{
	using openblack::Locator;
	const openblack::ecs::challenge_records::References references {
	    .add = {},
	    .remove =
	        [](entt::entity thing) {
		        if (Locator::scriptObjects::has_value())
		        {
			        Locator::scriptObjects::value().RemoveReference(thing);
		        }
	        },
	};
	openblack::ecs::challenge_records::Clear(Locator::entitiesRegistry::value(), references);
}

/// The scripts let go of what they held of the game when their lesson ends. The tasks that stopped gave back the camera,
/// the dialogue and the game's speed already.
void LetScriptsGo()
{
	using openblack::Locator;
	// The villagers that a script drew in high detail are drawn as usual again, and the camera draws nothing close to it
	if (Locator::highDetailSystem::has_value())
	{
		std::ignore = Locator::highDetailSystem::value().ReleaseAll();
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		Locator::cinematicDirectorSystem::value().SetCloseClipping(false);
	}
	Locator::introSystem::value().ReleaseAll();
	ClearChallengeRecords();
	Locator::dialogueControlSystem::value().Reset();
	auto& help = Locator::cameraHelpSystem::value().Get();
	help.Enable(openblack::camera_drag::k_DefaultFeatures, ~0u);
	help.handReach = openblack::camera_help::k_DefaultHandReach;
	help.cameraKeys = true;
	help.placeKeys = true;
	Locator::handDemoSystem::value().Reset();
	// The camera zones that a script set go
	if (Locator::cameraZoneSystem::has_value())
	{
		Locator::cameraZoneSystem::value().ClearZones();
	}
	std::ignore = openblack::chlapi::CHLApi::ApplyInterfaceLevel(k_NormalInterface);
	// The advisors stop the cheer of the camera's turn
	if (Locator::turnCheerSystem::has_value())
	{
		Locator::turnCheerSystem::value().Stop();
	}
}

} // namespace

void PlaygroundTripSystem::EndTurn()
{
	if (!playground_trip::TakeEnd(_lessons))
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lesson of the Gods' Playground ends");
	// Every thing that a script made goes, and every other thing that a script held goes back into the game
	if (Locator::scriptObjects::has_value())
	{
		Locator::scriptObjects::value().Reset();
	}
	StopEveryScript();
	LetScriptsGo();
	StartScript(playground_trip::k_PracticeScript);
	std::ignore = chlapi::CHLApi::ApplyInterfaceLevel(k_NormalInterface);
}

bool PlaygroundTripSystem::SaveBeforeTrip()
{
	return Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().InstantSave(gamesave::k_ExitSlot);
}

bool PlaygroundTripSystem::Travel()
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player goes to the Gods' Playground");
	StopEveryScript();
	const auto& fileSystem = Locator::filesystem::value();
	try
	{
		const auto path = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(playground_trip::k_LandScript));
		// The land loads as the story's next land does, with every script stopped first. Nothing shows while it loads.
		if (!Locator::landSystem::value().LoadMap(path, loading::LoadingClock::Mode::None, land_change::Kind::Story))
		{
			return false;
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The Gods' Playground did not load: {}", e.what());
		return false;
	}
	_influenceEverywhere = true;
	playground_trip::StartLesson(_lessons);
	StartScript(playground_trip::k_LessonsScript);
	return true;
}

bool PlaygroundTripSystem::Leave()
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player leaves the Gods' Playground");
	std::ignore = chlapi::CHLApi::ApplyInterfaceLevel(k_NormalInterface);
	_influenceEverywhere = false;
	playground_trip::Leave(_lessons);
	// With no saved game in the slot, nothing loads and the game stays paused
	return Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().Load(gamesave::k_ExitSlot);
}

void PlaygroundTripSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _influenceEverywhere);
	reflection::Save(out, _lessons.endAsked);
}

bool PlaygroundTripSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _influenceEverywhere);
	reflection::Load(in, _lessons.endAsked);
	return true;
}
