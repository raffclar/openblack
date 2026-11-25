/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/DrawnFrame.h"

using namespace openblack;

namespace
{
std::unique_ptr<Camera> MovedCamera()
{
	auto camera = std::make_unique<Camera>(glm::vec3(1200.0f, 30.0f, 1400.0f));
	camera->SetOrigin(glm::vec3(1100.0f, 180.0f, 1250.0f));
	camera->SetProjectionMatrixPerspective(60.0f, 16.0f / 9.0f, 1.0f);
	camera->SetShake(glm::vec3(0.5f, 0.0f, 0.25f), glm::vec3(0.0f, 0.5f, 0.0f));
	return camera;
}
} // namespace

TEST(DrawnFrame, TheDrawnCopyOfTheCameraShowsTheSameView)
{
	const auto moved = MovedCamera();
	const auto& camera = *moved;
	const auto copy = camera.DrawnCopy();
	ASSERT_NE(copy, nullptr);
	EXPECT_EQ(copy->GetViewMatrix(Camera::Interpolation::Current), camera.GetViewMatrix(Camera::Interpolation::Current));
	EXPECT_EQ(copy->GetViewMatrix(Camera::Interpolation::Drawn), camera.GetViewMatrix(Camera::Interpolation::Drawn));
	EXPECT_EQ(copy->GetProjectionMatrix(), camera.GetProjectionMatrix());
	EXPECT_EQ(copy->GetViewProjectionMatrix(), camera.GetViewProjectionMatrix());
	EXPECT_EQ(copy->GetOrigin(), camera.GetOrigin());
	EXPECT_EQ(copy->GetFocus(), camera.GetFocus());
	EXPECT_EQ(copy->GetForward(), camera.GetForward());
	EXPECT_EQ(copy->GetNearClip(), camera.GetNearClip());
}

TEST(DrawnFrame, TheDrawnCopyOfTheCameraDoesNotFollowTheCamera)
{
	const auto moved = MovedCamera();
	auto& camera = *moved;
	const auto copy = camera.DrawnCopy();
	const auto view = copy->GetViewMatrix(Camera::Interpolation::Current);
	camera.SetOrigin(glm::vec3(900.0f, 250.0f, 900.0f));
	camera.SetShake(glm::vec3(0.0f), glm::vec3(0.0f));
	EXPECT_NE(camera.GetViewMatrix(Camera::Interpolation::Current), view);
	EXPECT_EQ(copy->GetViewMatrix(Camera::Interpolation::Current), view);
}

TEST(DrawnFrame, ABoneRunGivesItsMatrices)
{
	graphics::DrawnFrame frame;
	frame.bones = {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(1.0f)),
	               glm::translate(glm::mat4(1.0f), glm::vec3(2.0f))};
	const auto run = frame.BonesOf({.first = 1, .count = 2});
	ASSERT_EQ(run.size(), 2U);
	EXPECT_EQ(run[0], frame.bones[1]);
	EXPECT_EQ(run[1], frame.bones[2]);
	EXPECT_TRUE(frame.BonesOf({}).empty());
}

TEST(DrawnFrame, TheModelsChangedOnlyWhenTheCountOfChangesMoved)
{
	graphics::DrawnFrame frame;
	// A frame with no models finds no change
	EXPECT_FALSE(frame.ModelsChanged(5));
	const ecs::systems::RenderContext models;
	frame.models = &models;
	frame.modelWrites = 5;
	EXPECT_FALSE(frame.ModelsChanged(5));
	EXPECT_TRUE(frame.ModelsChanged(6));
}

TEST(DrawnFrame, TheEffectsGiveOnlyTheSymbolsOfBeliefOfThisFrame)
{
	graphics::DrawnFrame frame;
	auto& effects = frame.effects;
	// The frames of earlier frames keep their memory, but the draw does not read them
	effects.townBeliefs.resize(3);
	effects.townBeliefs[0].centre = entt::entity {7};
	effects.townBeliefs[1].centre = entt::entity {9};
	effects.townBeliefCount = 2;
	const auto shown = effects.TownBeliefs();
	ASSERT_EQ(shown.size(), 2U);
	EXPECT_EQ(shown[0].centre, entt::entity {7});
	EXPECT_EQ(shown[1].centre, entt::entity {9});
	effects.townBeliefCount = 0;
	EXPECT_TRUE(effects.TownBeliefs().empty());
}

TEST(DrawnFrame, ASpriteShowsOnlyInTheViewsThatTheFillGaveIt)
{
	graphics::DrawnSprite sprite;
	EXPECT_TRUE(sprite.ShownIn(false));
	EXPECT_TRUE(sprite.ShownIn(true));
	// A glow of a temple room that the temple does not draw whole shows only in the floor of the main room
	sprite.inMainView = false;
	EXPECT_FALSE(sprite.ShownIn(false));
	EXPECT_TRUE(sprite.ShownIn(true));
	sprite.inMainView = true;
	sprite.inReflection = false;
	EXPECT_TRUE(sprite.ShownIn(false));
	EXPECT_FALSE(sprite.ShownIn(true));
}

TEST(DrawnFrame, TheSeaMirrorsACreatureOnlyFromTheSituationThatTheFillKept)
{
	graphics::DrawnCreatures::Creature creature;
	// A creature with no situation, as a copy or a frame with no land, is not mirrored
	EXPECT_FALSE(creature.MirroredAt(0.0f));
	creature.mirror = graphics::creature_reflection::Situation {
	    .primaryCreature = true,
	    .onBlock = true,
	    .blockInView = true,
	    .blockDistance = 0.0f,
	    .height = 0.0f,
	    .fizz = 0.0f,
	};
	// The draw gives the height over the water of each mirror
	EXPECT_TRUE(creature.MirroredAt(0.0f));
	EXPECT_FALSE(creature.MirroredAt(graphics::creature_reflection::k_HighestStand));
	creature.mirror->primaryCreature = false;
	EXPECT_FALSE(creature.MirroredAt(0.0f));
}

TEST(DrawnFrame, TheTempleCopyDrawsOnlyTheRoomsThatTheFillMarked)
{
	graphics::DrawnTemple temple;
	EXPECT_FALSE(temple.IsRoomDrawn(TempleRoom::Main));
	temple.roomsDrawn.at(static_cast<size_t>(TempleRoom::CreatureCave)) = true;
	EXPECT_TRUE(temple.IsRoomDrawn(TempleRoom::CreatureCave));
	EXPECT_FALSE(temple.IsRoomDrawn(TempleRoom::Main));
	EXPECT_EQ(graphics::DrawnTemple::k_RoomCount, static_cast<size_t>(TempleRoom::Unknown) + 1);
}

TEST(DrawnFrame, TheDrawnHandIsTheLeftHandOfThePlayerOfThisComputer)
{
	graphics::DrawnFrame::Hands hands;
	EXPECT_EQ(hands.Drawn(), entt::entity {entt::null});
	EXPECT_EQ(hands.ownPlayer, PlayerNames::PLAYER_ONE);
	hands.own = {entt::entity {7}, entt::entity {8}};
	EXPECT_EQ(hands.Drawn(), entt::entity {7});
	EXPECT_FALSE(hands.waterGlowCentre.has_value());
}
