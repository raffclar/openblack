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
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

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
	/// The things of the rooms. Only the steps of the frame change them. The passes of the mists, the light shafts and
	/// the exit glow read them while they draw.
	const ecs::Registry* rooms {nullptr};

	[[nodiscard]] bool IsRoomDrawn(TempleRoom room) const { return roomsDrawn.at(static_cast<size_t>(room)); }
};

} // namespace openblack::graphics
