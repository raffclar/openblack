/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Entities/Components/Transform.h"
#include "Entities/Components/Mesh.h"
#include "Entities/Components/Temple.h"
#include "Entities/Registry.h"
#include "TempleStructure.h"
#include "3D/Camera.h"
#include "3D/MeshLocator.h"
#include "Game.h"

#include <array>
#include <glm/gtx/euler_angles.hpp>

using namespace openblack;

using Outdoors = TempleStructure::TempleOutdoorPart;
std::map<std::string_view, Outdoors> OutsideMeshes {
	{"b_first_temple_l3d", Outdoors::EvilTemple},
	{"b_worship_l3d", Outdoors::WorshipSite},
	{"entrance_l3d", Outdoors::Entrance},
	{"b_temple00_l3d", Outdoors::TempleState1},
	{"b_temple01_l3d", Outdoors::TempleState1},
	{"b_temple02_l3d", Outdoors::TempleState1},
	{"b_temple03_l3d", Outdoors::TempleState1},
	{"b_temple04_l3d", Outdoors::TempleState1},
	{"b_temple10_l3d", Outdoors::TempleState1},
	{"b_temple11_l3d", Outdoors::TempleState1},
	{"b_temple12_l3d", Outdoors::TempleState1},
	{"b_temple13_l3d", Outdoors::TempleState1},
	{"b_temple14_l3d", Outdoors::TempleState1},
	{"b_temple20_l3d", Outdoors::TempleState1},
	{"b_temple21_l3d", Outdoors::TempleState1},
	{"b_temple22_l3d", Outdoors::TempleState1},
	{"b_temple23_l3d", Outdoors::TempleState1},
	{"b_temple24_l3d", Outdoors::TempleState1},
};

using Indoors = TempleStructure::TempleIndoorPart;
std::map<std::string_view, Indoors> InsideMeshes {
	{"challenge_l3d", Indoors::Challenge},
	{"challengelo_l3d", Indoors::ChallengeLO},
	{"challengedome_l3d", Indoors::ChallengeDome},
	{"challengefloor_l3d", Indoors::ChallengeFloor},
	{"challengefloorlo_l3d", Indoors::ChallengeFloorLO},
	{"creature_l3d", Indoors::CreatureCave},
	{"creaturelo_l3d", Indoors::CreatureCaveLO},
	{"creaturewater_l3d", Indoors::CreatureCaveWater},
	{"creaturewaterlo_l3d", Indoors::CreatureCaveWaterLO},
	{"credits_l3d", Indoors::Credits},
	{"creditslo_l3d", Indoors::CreditsLO},
	{"creditsdome_l3d", Indoors::CreditsDome},
	{"creditsfloor_l3d", Indoors::CreditsFloor},
	{"creditsfloorlo_l3d", Indoors::CreditsFloorLO},
	{"main_l3d", Indoors::Main},
	{"mainlo_l3d", Indoors::MainLO},
	{"mainfloor_l3d", Indoors::MainFloor},
	{"mainfloorlo_l3d", Indoors::MainFloorLO},
	{"mainwater_l3d", Indoors::MainWater},
	{"mainwaterlo_l3d", Indoors::MainWaterLO},
	{"movement_l3d", Indoors::Movement},
	{"multi_l3d", Indoors::Multi},
	{"multilo_l3d", Indoors::MultiLO},
	{"multidome_l3d", Indoors::MultiDome},
	{"multifloor_l3d", Indoors::MultiFloor},
	{"multifloorlo_l3d", Indoors::MultiFloorLO},
	{"options_l3d", Indoors::Options},
	{"optionslo_l3d", Indoors::OptionsLO},
	{"optionsdome_l3d", Indoors::OptionsDome},
	{"optionsfloor_l3d", Indoors::OptionsFloor},
	{"optionsfloorlo_l3d", Indoors::OptionsFloorLO},
	{"savegame_l3d", Indoors::SaveGame},
	{"savegamelo_l3d", Indoors::SaveGameLO},
	{"savegamedome_l3d", Indoors::SaveGameDome},
	{"savegamefloor_l3d", Indoors::SaveGameFloor},
	{"savegamefloorlo_l3d", Indoors::SaveGameFloorLO},
};

std::map<std::string_view, Indoors> InsideCameraPaths
{
	{"challenge", Indoors::Challenge},
	{"creature", Indoors::CreatureCave},
	{"credits", Indoors::Credits},
	{"main", Indoors::Main},
	{"multi", Indoors::Multi},
	{"options", Indoors::Options},
	{"savegame", Indoors::SaveGame},
};

using Room = TempleStructure::TempleRoom;
std::map<Room, std::string> RoomNames
{
	{Room::Main, "Main Hall"},
	{Room::Challenge, "Challenge Room"},
	{Room::CreatureCave, "Creature Cave"},
	{Room::Multi, "Future Room"},
	{Room::Credits, "Multiplayer Room"},
	{Room::Options, "Options Room"},
	{Room::SaveGame, "Save Game Room"}
};


bool TempleStructure::LoadOutsideTempleMeshes()
{
	MeshLocator locator;
	auto meshDir = Game::instance()->GetFileSystem().DataPath() / "Citadel/OutsideMeshes";

	if (!locator.LoadLooseMeshFiles(meshDir))
	{
		return false;
	}

	auto& meshes = locator.GetMeshes();

	if (meshes.size() == 0)
	{
		return false;
	}

	for (auto& [meshName, meshId] : meshes)
	{
		auto templePart = OutsideMeshes.find(meshName)->second;
		_outsideMeshes.emplace(templePart, meshId);
	}

	return true;
}

bool TempleStructure::LoadInteriorTempleAssets()
{
	MeshLocator locator;
	auto assetDir = Game::instance()->GetFileSystem().DataPath() / "Citadel/engine";

	// The temple interior rooms, floors, ceilings and other assets
	if (!locator.LoadLooseMeshFiles(assetDir))
	{
		return false;
	}

	// These camera paths guide the player from temple rooms' doors to a location within the rooms
	auto& fs = Game::instance()->GetFileSystem();
	auto camFilesPaths = fs.GetAllFilePaths(assetDir.string(), ".cam", true);

	for (auto& camFilePath : camFilesPaths)
	{

		auto fileName = camFilePath.stem().string();
		auto templeAsset = InsideCameraPaths.find(fileName)->second;
		TempleRoom cameraPathRoom;

		switch (templeAsset)
		{
		case Indoors::Challenge:
			cameraPathRoom = TempleRoom::Challenge;
			break;
		case Indoors::CreatureCave:
			cameraPathRoom = TempleRoom::CreatureCave;
			break;
		case Indoors::Credits:
			cameraPathRoom = TempleRoom::Credits;
			break;
		case Indoors::Main:
			cameraPathRoom = TempleRoom::Main;
			break;
		case Indoors::Multi:
			cameraPathRoom = TempleRoom::Multi;
			break;
		case Indoors::Options:
			cameraPathRoom = TempleRoom::Options;
			break;
		case Indoors::SaveGame:
			cameraPathRoom = TempleRoom::SaveGame;
			break;
		default:
			break;
		}

		_insideCameraPaths[cameraPathRoom] = std::make_unique<CameraPath>();
		_insideCameraPaths[cameraPathRoom]->LoadFromFile(camFilePath);
	}

	auto& meshes = locator.GetMeshes();

	if (meshes.size() == 0)
	{
		return false;
	}

	for (auto& [meshName, meshId] : meshes)
	{
		auto templePart = InsideMeshes.find(meshName)->second;

		// LO suffix has unknown meaning
		// The Movement mesh might be used by villagers as they wander through the interior
		switch (templePart)
		{
		case TempleIndoorPart::ChallengeLO:
		case TempleIndoorPart::ChallengeFloorLO:
		case TempleIndoorPart::MainLO:
		case TempleIndoorPart::MainWaterLO:
		case TempleIndoorPart::MainFloorLO:
		case TempleIndoorPart::CreatureCaveLO:
		case TempleIndoorPart::CreatureCaveWaterLO:
		case TempleIndoorPart::CreditsLO:
		case TempleIndoorPart::CreditsFloorLO:
		case TempleIndoorPart::SaveGameLO:
		case TempleIndoorPart::SaveGameFloorLO:
		case TempleIndoorPart::OptionsLO:
		case TempleIndoorPart::OptionsFloorLO:
		case TempleIndoorPart::MultiLO:
		case TempleIndoorPart::MultiFloorLO:
		case TempleIndoorPart::Movement:
			continue;
		default:
			break;
		}

		_insideMeshes.emplace(templePart, meshId);
	}

	return true;
}

std::vector<MeshId> TempleStructure::GetInteriorTempleMeshIds()
{
	auto interiorIds = std::vector<MeshId>();

	for (const auto &[_, id]: _insideMeshes)
		interiorIds.emplace_back(id);

	return interiorIds;
}

MeshId TempleStructure::GetTempleMeshId(openblack::entities::components::Temple temple)
{
	return _outsideMeshes.find(TempleOutdoorPart::EvilTemple)->second;
}

MeshId TempleStructure::GetTempleWorshipSiteMeshId(openblack::entities::components::TempleWorshipSite site)
{
	return _outsideMeshes.find(TempleOutdoorPart::WorshipSite)->second;
}

void TempleStructure::EnterTemple()
{
	if (_insideTemple)
		return;

	auto& registry = Game::instance()->GetEntityRegistry();
	auto& camera = Game::instance()->GetCamera();
	auto& config = Game::instance()->GetConfig();
	config.drawIsland = false;
	config.drawSky = false;
	config.drawWater = false;

	// Spawn temple meshes
	auto meshIds =  GetInteriorTempleMeshIds();
	_templePosition = glm::vec3(.0f, -10000.0f, .0f);
	auto rotation = glm::eulerAngleY(glm::radians(0.0f));
	auto scale = glm::vec3(1.0f);

	for (auto& meshId : meshIds)
	{
		auto entity = registry.Create();
		auto roomType = entities::components::TempleIndoorRoom::MainRoom;
		registry.Assign<entities::components::TempleInterior>(entity, roomType);
		registry.Assign<entities::components::Transform>(entity, _templePosition, rotation, scale);
		registry.Assign<entities::components::Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}

	_lastPlayerPosition = camera.GetPosition();
	_lastPlayerRotation = camera.GetRotation();

	// Move the player to the temple
	camera.SetPosition(_templePosition);

	_insideTemple = true;
	MoveToRoom(TempleRoom::Main);
}

void TempleStructure::ExitTemple()
{
	if (!_insideTemple)
		return;

	auto& registry = Game::instance()->GetEntityRegistry();
	auto& camera = Game::instance()->GetCamera();
	auto& config = Game::instance()->GetConfig();

	registry.Destroy<entities::components::TempleInterior>();
	config.drawIsland = true;
	config.drawSky = true;
	config.drawWater = true;
	camera.SetPosition(_lastPlayerPosition);
	camera.SetRotation(_lastPlayerRotation);
	_insideTemple = false;
}

void TempleStructure::MoveToRoom(TempleStructure::TempleRoom room)
{
	if (!_insideTemple)
		return;

	auto& camPath = _insideCameraPaths.find(room)->second;
	Game::instance()->GetCamera().PlayPath(*camPath, _templePosition);
}

const std::string& TempleStructure::GetRoomName(TempleStructure::TempleRoom room)
{
	return RoomNames.find(room)->second;
}
