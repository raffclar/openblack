/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/BoatFilmBoat.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Registry.h"

using namespace openblack;
using openblack::ecs::components::BoatFilmPart;
using openblack::ecs::components::DrawnMatrix;
using openblack::ecs::components::HiddenByState;

namespace
{
/// Make a part of the boat film at a place, as the film makes each part
entt::entity MakePart(ecs::Registry& registry, BoatFilmPart::Role role, const glm::vec3& place, bool hidden)
{
	const auto entity = registry.Create();
	registry.Assign<BoatFilmPart>(entity, role, static_cast<uint8_t>(0), 0, false);
	registry.Assign<DrawnMatrix>(entity, glm::translate(glm::mat4(1.0f), place));
	if (hidden)
	{
		registry.Assign<HiddenByState>(entity);
	}
	return entity;
}
} // namespace

TEST(BoatFilmBoat, NoFilmGivesNoBoat)
{
	ecs::Registry registry;
	EXPECT_FALSE(ecs::boat_film::ShownBoatMatrix(registry).has_value());
}

TEST(BoatFilmBoat, TheSailingBoatGivesItsMatrixWhileTheLaunchBoatIsHidden)
{
	ecs::Registry registry;
	// The film makes the launch parts first, then the sailing parts. The launch stage ended, so its boat is hidden.
	MakePart(registry, BoatFilmPart::Role::Boat, {100.0f, 0.0f, 200.0f}, true);
	MakePart(registry, BoatFilmPart::Role::Boat, {300.0f, 0.0f, 400.0f}, false);
	MakePart(registry, BoatFilmPart::Role::DeckPart, {301.0f, 2.0f, 401.0f}, false);
	const auto boat = ecs::boat_film::ShownBoatMatrix(registry);
	ASSERT_TRUE(boat.has_value());
	EXPECT_FLOAT_EQ((*boat)[3].x, 300.0f);
	EXPECT_FLOAT_EQ((*boat)[3].z, 400.0f);
}

TEST(BoatFilmBoat, TheLaunchBoatGivesItsMatrixWhileTheSailingBoatIsHidden)
{
	ecs::Registry registry;
	MakePart(registry, BoatFilmPart::Role::Boat, {100.0f, 0.0f, 200.0f}, false);
	MakePart(registry, BoatFilmPart::Role::LaunchSailor, {101.0f, 0.0f, 201.0f}, false);
	MakePart(registry, BoatFilmPart::Role::Boat, {300.0f, 0.0f, 400.0f}, true);
	const auto boat = ecs::boat_film::ShownBoatMatrix(registry);
	ASSERT_TRUE(boat.has_value());
	EXPECT_FLOAT_EQ((*boat)[3].x, 100.0f);
	EXPECT_FLOAT_EQ((*boat)[3].z, 200.0f);
}

TEST(BoatFilmBoat, OnlyHiddenBoatsGiveNoBoat)
{
	ecs::Registry registry;
	MakePart(registry, BoatFilmPart::Role::Boat, {100.0f, 0.0f, 200.0f}, true);
	MakePart(registry, BoatFilmPart::Role::Boat, {300.0f, 0.0f, 400.0f}, true);
	EXPECT_FALSE(ecs::boat_film::ShownBoatMatrix(registry).has_value());
}
