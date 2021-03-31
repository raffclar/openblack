/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "Common/MeshLookup.h"
#include "Entities/Components/Temple.h"

#include <3D/CameraPath.h>
#include <glm/vec3.hpp>
#include <map>

namespace openblack
{
class TempleStructure
{
public:
	enum class TempleOutdoorPart
	{
		EvilTemple,
		WorshipSite,
		Entrance,
		TempleState1,
		TempleState2,
		TempleState3,
		TempleState4,
		TempleState5,
		TempleState6,
		TempleState7,
		TempleState8,
		TempleState9,
		TempleState10,
		TempleState11,
		TempleState12,
		TempleState13
	};

	enum class TempleIndoorPart
	{
		Challenge,
		ChallengeLO,
		ChallengeDome,
		ChallengeFloor,
		ChallengeFloorLO,
		CreatureCave,
		CreatureCaveLO,
		CreatureCaveWater,
		CreatureCaveWaterLO,
		Credits,
		CreditsLO,
		CreditsDome,
		CreditsFloor,
		CreditsFloorLO,
		Main,
		MainLO,
		MainFloor,
		MainFloorLO,
		MainWater,
		MainWaterLO,
		Movement,
		Multi,
		MultiLO,
		MultiDome,
		MultiFloor,
		MultiFloorLO,
		Options,
		OptionsLO,
		OptionsDome,
		OptionsFloor,
		OptionsFloorLO,
		SaveGame,
		SaveGameLO,
		SaveGameDome,
		SaveGameFloor,
		SaveGameFloorLO,
	};

	enum class TempleRoom
	{
		Challenge,
		CreatureCave,
		Credits,
		Main,
		Multi,
		Options,
		SaveGame,
		Count
	};

	bool LoadOutsideTempleMeshes();
	bool LoadInteriorTempleAssets();
	MeshId GetTempleMeshId(entities::components::Temple temple);
	MeshId GetTempleWorshipSiteMeshId(entities::components::TempleWorshipSite site);
	std::vector<MeshId> GetInteriorTempleMeshIds();
	void EnterTemple();
	void ExitTemple();
	void MoveToRoom(TempleRoom room);
	static const std::string& GetRoomName(TempleRoom room);
	bool IsInsideTemple() const { return _insideTemple; }
private:
	bool _insideTemple;
	glm::vec3 _lastPlayerPosition;
	glm::vec3 _lastPlayerRotation;
	glm::vec3 _templePosition;
	std::map<TempleOutdoorPart, MeshId> _outsideMeshes;
	std::map<TempleIndoorPart, MeshId> _insideMeshes;
	std::unordered_map<TempleRoom, std::unique_ptr<CameraPath>> _insideCameraPaths;
};
}
