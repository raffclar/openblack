/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the cameras that only a saved game of the original game gives: the follow camera with the
// values of a file, on a creature or on another thing, with its heading turned with its thing or with offsets, the follow
// camera that turns with its thing, the free camera, and the world camera that can go below the land

#include <cstddef>

#include <memory>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CreatureFollow.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FollowHeadingCameraModel.h"
#include "Camera/FreeCameraModel.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/FollowHeadingThing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "Locator.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The view of the follow cameras of the scenarios: a little more than a quarter turn round, looking down at about 23
/// degrees, 60 away
constexpr float k_Heading = 1.8f;
constexpr float k_Pitch = 0.4f;
constexpr float k_Distance = 60.0f;
/// The turning camera turns 0.005 of an angle round the creature in each frame: a full turn in about 1257 frames
constexpr float k_TurnSpeed = 0.005f;
/// The free camera of the scenario starts at this speed, and looks at a point this far in front of it and this far
/// below it
constexpr float k_FreeSpeed = 2.0f;
constexpr float k_FreeLookAhead = 100.0f;
constexpr float k_FreeLookDown = 10.0f;
/// The eye of the camera that can go below the land starts this far below it
constexpr float k_BelowLandDepth = 2.0f;
/// The values of the follow camera for a heading that turns with its thing, and for offsets
constexpr size_t k_RelativeHeading = 1;
constexpr size_t k_Offsets = 2;
/// The offsets of the follow camera: it looks at a point 1 above the place of its thing, from 10 to the side, 6 up and
/// 10 back
constexpr glm::vec3 k_FocusOffset {0.0f, 1.0f, 0.0f};
constexpr glm::vec3 k_EyeOffset {10.0f, 6.0f, -10.0f};

/// The follow cameras on a villager look at it from nearer, so that the villager shows well
constexpr float k_ThingDistance = 20.0f;

/// A villager that walks between two points, which a follow camera follows
ObjectSetup Walker()
{
	return {.type = VillagerInfo::CelticHousewifeFemale,
	        .offset = {20.0f, 40.0f},
	        .walkTo = glm::vec2 {20.0f, 52.0f},
	        .walkAfterSeconds = 2.0f,
	        .walkRepeatSeconds = 14.0f};
}

CreatureSetup Player(glm::vec2 offset)
{
	return {.label = "yours",
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .owner = PlayerNames::PLAYER_ONE,
	        .needs = {.energy = 0.8f, .exhaustion = 0.3f, .dehydration = 0.0f, .poo = 0.0f, .life = 0.9f}};
}

Command Walk(glm::vec2 point, float delay)
{
	return {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = delay, .point = point};
}

Command Saved(Kind kind, float delay, float amount = 0.0f)
{
	return {.kind = kind, .creature = 0, .delaySeconds = delay, .amount = amount};
}

/// The follow camera of a saved game on the scenario's first object, with the values of the follow camera
Command SavedOnObject(float delay, size_t values)
{
	return {.kind = Kind::SavedFollowCamera, .creature = 0, .delaySeconds = delay, .value = values, .with = size_t {0}};
}

/// The creature walks round a square, so that it turns at each corner
std::vector<Command> Square(Command first)
{
	return {std::move(first), Walk({40.0f, 40.0f}, 0.5f), Walk({40.0f, 100.0f}, 5.0f), Walk({0.0f, 100.0f}, 5.0f),
	        Walk({0.0f, 40.0f}, 5.0f)};
}

} // namespace

void testbed_scenarios::AddSavedCameraScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "saves.camera_follow",
	    .name = "Follow camera of a saved game",
	    .facet = Facet::CreatureMode,
	    .description = "The follow camera of a saved game takes the camera after a second: heading 1.8, pitch 0.4, distance "
	                   "60, the whole glide time. The player's tiger then walks round a square.",
	    .expected = "The camera glides to the tiger over two seconds and looks at its middle from its right side, 60 "
	                "away, then follows it round the square a second behind. The status panel shows, as in Creature Mode.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .commands = Square(Saved(Kind::SavedFollowCamera, 1.0f)),
	});

	all.push_back({
	    .id = "saves.camera_follow_thing",
	    .name = "Follow camera of a saved game on a villager",
	    .facet = Facet::CreatureMode,
	    .description = "The follow camera of a saved game takes the camera after a second on a villager, not on a creature: "
	                   "heading 1.8, pitch 0.4, distance 20. The villager walks away and back again.",
	    .expected = "The camera glides to the villager and looks at the middle of its body from 20 away, then follows it "
	                "a second behind as it walks. No status panel shows, because the thing is not a creature.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .objects = {Walker()},
	    .commands = {SavedOnObject(1.0f, 0)},
	});

	all.push_back({
	    .id = "saves.camera_follow_turns",
	    .name = "Follow camera of a saved game that turns with its thing",
	    .facet = Facet::CreatureMode,
	    .description = "A follow camera of a saved game with the heading that turns with its thing, as a script's camera "
	                   "is kept, follows a villager that walks away and back again.",
	    .expected = "The camera stays at the same side of the villager as it walks. When the villager turns round, the "
	                "camera swings round it, a second behind, to the same side of its new way.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .objects = {Walker()},
	    .commands = {SavedOnObject(1.0f, k_RelativeHeading)},
	});

	all.push_back({
	    .id = "saves.camera_follow_creature_turns",
	    .name = "Follow camera of a saved game that turns with the creature",
	    .facet = Facet::CreatureMode,
	    .description = "A follow camera of a saved game with the heading that turns with its thing follows the player's "
	                   "tiger round a square. The walk of a creature never changes its game angle.",
	    .expected = "The camera does not turn with the tiger: it looks from a heading a quarter turn on from the heading of "
	                "the file, the same in the whole walk round the square.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .commands =
	        Square(Command {.kind = Kind::SavedFollowCamera, .creature = 0, .delaySeconds = 1.0f, .value = k_RelativeHeading}),
	});

	all.push_back({
	    .id = "saves.camera_follow_offsets",
	    .name = "Follow camera of a saved game with offsets",
	    .facet = Facet::CreatureMode,
	    .description = "A follow camera of a saved game keeps offsets from a villager: it looks at a point 1 above the "
	                   "villager from 10 to the side, 6 up and 10 back. The villager walks away and back again.",
	    .expected = "The camera keeps the same place relative to the villager and does not turn when the villager turns. "
	                "It follows a second behind. The cursor keys with Shift stop the offsets, and the camera then turns "
	                "round the villager.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .objects = {Walker()},
	    .commands = {SavedOnObject(1.0f, k_Offsets)},
	});

	all.push_back({
	    .id = "saves.camera_turning",
	    .name = "Turning follow camera of a saved game",
	    .facet = Facet::CreatureMode,
	    .description = "The follow camera that turns with its thing, as only a saved game gives it, takes the camera after a "
	                   "second on the player's tiger: pitch 0.4, distance 60, a turn of 0.005 in each frame. The tiger "
	                   "then walks round a square.",
	    .expected = "The camera looks at the middle of the tiger and goes slowly round it, a full turn in about 1257 "
	                "frames, from about 60 away and 23 degrees above. When the tiger turns at a corner, the circle turns "
	                "with it. The camera glides after its place, two seconds behind at the start and one second after "
	                "two seconds, so it cuts inside the circle a little.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .commands = Square(Saved(Kind::SavedTurningCamera, 1.0f)),
	});

	all.push_back({
	    .id = "saves.camera_free",
	    .name = "Free camera of a saved game",
	    .facet = Facet::CreatureMode,
	    .description = "The free camera, as only a saved game gives it, takes the camera after four seconds, flying "
	                   "forwards at a speed of 2. It looks towards the tiger, a little down.",
	    .expected = "The camera flies straight along its view at once, with no glide, 10 a frame at the start, and slows "
	                "down to a stop in about a second (the speed falls to e to the power of -5 each second). It does not "
	                "stop at the land: it can go into a hill. A grip of the land with the mouse at the side of the screen "
	                "speeds it up and turns it; the cursor keys move it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .commands = {Saved(Kind::SavedFreeCamera, 4.0f, k_FreeSpeed)},
	});

	all.push_back({
	    .id = "saves.camera_below_land",
	    .name = "World camera of a saved game below the land",
	    .facet = Facet::CreatureMode,
	    .description = "A saved game lets the world camera go below the land. The eye is put 2 below the "
	                   "land in front of the player's tiger, looking at the tiger. This happens after four seconds.",
	    .expected = "The push of the camera off the ground lifts the eye to 3 above the land, as without the saved game, "
	                "and moves the point that it looks at back. The saved game takes away only the floor 3 above the land "
	                "at the start of each frame and the lift of the drawn eye to 1 above the land. The camera stays still "
	                "near the ground, with the tiger in front of it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Player({0.0f, 40.0f})},
	    .commands = {Saved(Kind::SavedBelowLandCamera, 4.0f, k_BelowLandDepth)},
	});
}

std::string testbed_scenarios::GiveSavedCameraCommand(entt::entity creature, const Command& command)
{
	if (!Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return "no camera";
	}
	auto& camera = Locator::camera::value();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(creature);
	if (transform == nullptr)
	{
		return "no thing";
	}
	switch (command.kind)
	{
	case Kind::SavedFollowCamera:
	{
		if (!Locator::creatureModeSystem::has_value())
		{
			return "no creature mode";
		}
		const creature_follow::Loaded loaded {
		    .view = {.yaw = k_Heading,
		             .pitch = k_Pitch,
		             .distance = registry.AllOf<ecs::components::Creature>(creature) ? k_Distance : k_ThingDistance},
		    .timeScale = 1.0f,
		    .autoReposition = true,
		    .relativeHeading = (command.value & k_RelativeHeading) != 0,
		    .offsets = (command.value & k_Offsets) != 0
		                   ? std::optional(creature_follow::Offsets {.focus = k_FocusOffset, .eye = k_EyeOffset})
		                   : std::nullopt,
		};
		return Locator::creatureModeSystem::value().EnterLoaded(creature, loaded) ? "" : "the camera can't be taken";
	}
	case Kind::SavedTurningCamera:
		camera.SetModel(std::make_unique<FollowHeadingCameraModel>(
		    ecs::FollowHeadingThing(creature),
		    follow_heading::Settings {.heading = 0.0f, .pitch = k_Pitch, .distance = k_Distance, .headingSpeed = k_TurnSpeed},
		    camera.HeldFollowHeading(), camera.GetOrigin(), camera.GetFocus()));
		return "";
	case Kind::SavedFreeCamera:
	{
		// The camera looks along the land, a little down, towards the creature, so that the flight stays above it
		const auto origin = camera.GetOrigin();
		const glm::vec2 along = glm::xz(transform->position) - glm::xz(origin);
		const glm::vec2 way = glm::length(along) > 0.0f ? glm::normalize(along) : glm::vec2(0.0f, 1.0f);
		const glm::vec3 focus = origin + glm::vec3(way.x * k_FreeLookAhead, -k_FreeLookDown, way.y * k_FreeLookAhead);
		camera.SetModel(std::make_unique<FreeCameraModel>(origin, focus, free_camera::State {.speed = command.amount}));
		camera.SetFocus(focus);
		return "";
	}
	case Kind::SavedBelowLandCamera:
	{
		camera.SetEyeMayGoBelowLand(true);
		const auto focus = transform->position;
		glm::vec3 eye = focus + glm::vec3(0.0f, 0.0f, -30.0f);
		if (Locator::terrainSystem::has_value())
		{
			eye.y = Locator::terrainSystem::value().GetHeightAt(glm::xz(eye)) - command.amount;
		}
		camera.SetModel(std::make_unique<DefaultWorldCameraModel>(eye, focus));
		camera.SetOrigin(eye).SetFocus(focus);
		return fmt::format("eye at height {:.1f}", eye.y);
	}
	default:
		return {};
	}
}
