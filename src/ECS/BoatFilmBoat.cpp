/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BoatFilmBoat.h"

#include <entt/entity/registry.hpp>

#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Registry.h"

namespace openblack::ecs::boat_film
{

std::optional<glm::mat4> ShownBoatMatrix(const Registry& drawn)
{
	using components::BoatFilmPart;
	std::optional<glm::mat4> boat;
	drawn.Each<const BoatFilmPart, const components::DrawnMatrix>(
	    [&boat](const BoatFilmPart& part, const components::DrawnMatrix& matrix) {
		    if (part.role == BoatFilmPart::Role::Boat)
		    {
			    boat = matrix.model;
		    }
	    },
	    entt::exclude<components::HiddenByState>);
	return boat;
}

} // namespace openblack::ecs::boat_film
