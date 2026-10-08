/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingPos.h"

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/MapCells.h"
#include "ECS/SeaCells.h"

namespace openblack::ecs::living
{

bool InBounds(glm::vec2 position)
{
	// the cell's unsigned high words inside the map, not the land's extent
	return sea_cells::InBounds(glm::vec3(position.x, 0.0f, position.y));
}

bool Collides(glm::vec2 position, uint32_t collideType)
{
	// every type off the map, else the landscape (0x10 off the game map, 1 water, 2 land) with 4 for a FIELD and 0x20
	// for a FOREST_TREE in the cell's fixed list. Fixed objects' own footprints do not count (bit 8 never comes from
	// here)
	return (map_cells::Collide(map_coords::FromMetres(position)) & collideType) != 0;
}

glm::vec2 CalcRandomPos(glm::vec2 centre, float rMin, float rMax, uint32_t collideType, glm::vec2 own,
                        const PosTest& validForTurnAngle, const PosTest& validForMapCell)
{
	// range = rMax - rMin (float), once
	const float range = rMax - rMin;
	// two random points, each followed by a 25-cell spiral from it
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		// a = GameFloatRand(2 pi); r = GameFloatRand(range) + rMin, always called (GameFloatRand itself gives 0 for 0)
		const float a = game_random::GameFloatRand(glm::two_pi<float>());
		const float r = game_random::GameFloatRand(range) + rMin;
		// the point at angle a and distance r: the centre's x and z go to metres, cos(a) r is added and the sum goes back
		// to 16.16; the spiral then walks those map coordinates (in bounds, collision, the two tests). (openblack) the
		// centre arrives in metres, as openblack keeps positions, and becomes map coordinates here; the original takes the
		// map coordinates themselves, so a centre that was not one already may be a unit apart (Quantise is not
		// idempotent)
		map_coords::MapCoords coords = map_coords::FromMetres(centre);
		gutils::AddDistanceFromAngle(coords, a, r);
		map_coords::Spiral spiral;
		for (int i = 0; i < 25; ++i)
		{
			const glm::vec2 p = map_coords::ToMetres(coords);
			if (InBounds(p) && !Collides(p, collideType) && validForTurnAngle(p) && validForMapCell(p))
			{
				return p;
			}
			// the step on the high words only (the sub-cell fraction kept)
			map_coords::AddCells(coords, spiral.Next());
		}
	}
	// the centre if it passes the turn test; else its own position plus a step that a shift makes 0, so its own
	// position (the turn test there is not used)
	return validForTurnAngle(centre) ? centre : own;
}

} // namespace openblack::ecs::living
