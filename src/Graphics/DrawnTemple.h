/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/Light.h"
#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleFog.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleLight.h"
#include "3D/TempleMap.h"
#include "3D/TempleMapSpells.h"
#include "3D/TempleRoom.h"

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

namespace openblack::graphics
{
class Texture2D;
struct BeamMesh;

/// What the passes read of the temple. The steps of the frame fill it on the game's thread, only while the player is
/// in the temple. Outside the temple, all of it stays at its empty values.
struct DrawnTemple
{
	/// How many rooms the temple has, with the room that is not known
	static constexpr size_t k_RoomCount = static_cast<size_t>(TempleRoom::Unknown) + 1;

	glm::vec3 position {0.0f};
	TempleLight light {};
	TempleFog fog {};
	/// Whether the temple draws each room whole, by the number of the room
	std::array<bool, k_RoomCount> roomsDrawn {};
	/// How long the camera walked to the door out of the temple, while it does
	std::optional<float> wayOutSeconds;
	uint32_t visits {0};
	float poolTime {0.0f};
	float mapMarkerTurn {0.0f};
	/// The words in the rooms, in triangles of the font's texture
	std::vector<OrientedTextVertex> text;
	const Texture2D* textTexture {nullptr};
	/// The island over the pool of the main room, in triangles
	std::vector<OrientedTextVertex> map;
	std::vector<TempleMapMarker> mapMarkers;
	std::vector<TempleMapSpellSeed> mapSpellSeeds;
	std::optional<TempleMapInfluence> mapInfluence;
	std::vector<TempleCaveTrophy> caveTrophies;
	std::vector<TempleCaveSeed> caveSeeds;
	/// The turn of each joint of the doors of the main room
	TempleDoors::Joints doorJoints {};

	/// A mist of a room that the temple draws
	struct Mist
	{
		std::shared_ptr<const BeamMesh> dome;
		glm::vec2 uvOffset {0.0f};
		glm::vec4 colour {1.0f};
		glm::vec3 position {0.0f};
		glm::vec3 scale {1.0f};
	};
	/// The cone of a spot light of a room
	struct Cone
	{
		LightCone cone;
		TempleRoom room {TempleRoom::Unknown};
	};
	/// The model of a room, which has the lights that its windows shed
	struct RoomModel
	{
		entt::id_type mesh {0};
		TempleRoom room {TempleRoom::Unknown};
		glm::mat4 model {1.0f};
	};
	/// The mists of the rooms that the temple draws, in the order of the things of the rooms
	std::vector<Mist> mists;
	/// The cones of the spot lights of all the rooms, in the order of the things of the rooms
	std::vector<Cone> cones;
	/// The models of the rooms, in the order of the things of the rooms
	std::vector<RoomModel> roomModels;
	/// The place of the main room, where the exit glow shows
	glm::mat4 mainRoomModel {1.0f};

	[[nodiscard]] bool IsRoomDrawn(TempleRoom room) const { return roomsDrawn.at(static_cast<size_t>(room)); }
	/// Copies the mists, the cones of the spot lights and the models of the rooms from the things of the rooms. Set
	/// `roomsDrawn` first: the copy keeps only the mists of the rooms that the temple draws.
	void CopyRoomParts(const ecs::Registry& rooms);
	/// Removes the copies of the things of the rooms
	void ClearRoomParts();
};

} // namespace openblack::graphics
