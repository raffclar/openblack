/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/GestureSystem.h"
#include "Gestures/GesturePaths.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
GestureSystemInterface::Frame Frame(float seconds)
{
	// The interface got one place of the pointer in the frame
	GestureSystemInterface::Frame frame {
	    .seconds = seconds, .places = {{100.0f, 100.0f}}, .view = {.screenSize = {1024.0f, 768.0f}}};
	// A flat land under the whole screen
	frame.view.landAt = [](glm::vec2 pixel) { return std::optional<glm::vec3>(glm::vec3(pixel.x, 0.0f, pixel.y)); };
	return frame;
}

/// The length of a frame at sixty frames a second
constexpr float k_FrameSeconds = 1.0f / 60.0f;
} // namespace

TEST(GestureSystem, HandsOutEachEventOnce)
{
	GestureSystem system;
	system.Inject({.kind = GestureEvent::Kind::Circle, .gesture = GestureType::Circle, .radius = 20.0f});
	system.Inject({.kind = GestureEvent::Kind::PowerUp, .powerUpLevel = 1});
	const auto events = system.TakeEvents();
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].kind, GestureEvent::Kind::Circle);
	EXPECT_FLOAT_EQ(events[0].radius, 20.0f);
	EXPECT_EQ(events[1].powerUpLevel, 1);
	EXPECT_TRUE(system.TakeEvents().empty());
	system.Inject({.kind = GestureEvent::Kind::Scribble});
	system.Reset();
	EXPECT_TRUE(system.TakeEvents().empty());
}

TEST(GestureSystem, TakesASampleAtEachPlaceOfThePointer)
{
	GestureSystem system;
	// A frame with three places takes three samples, in the sequence that they came
	auto frame = Frame(k_FrameSeconds);
	frame.places = {{100.0f, 100.0f}, {140.0f, 100.0f}, {180.0f, 100.0f}};
	system.Update(frame);
	ASSERT_EQ(system.GetPath().Count(), 3u);
	// On a screen 768 high the path uses its pixels
	EXPECT_EQ(system.GetPath().At(0).screen, glm::vec2(100.0f, 100.0f));
	EXPECT_EQ(system.GetPath().At(2).screen, glm::vec2(180.0f, 100.0f));
	// A frame without a place takes no sample
	auto empty = Frame(k_FrameSeconds);
	empty.places.clear();
	system.Update(empty);
	EXPECT_EQ(system.GetPath().Count(), 3u);

	// Off the world, the path is forgotten
	auto away = Frame(0.1f);
	away.overWorld = false;
	system.Update(away);
	EXPECT_TRUE(system.GetPath().Empty());
}

TEST(GestureSystem, OnAWindowOfAnotherHeightThePlacesGoToTheScreenThatThePathUses)
{
	GestureSystem system;
	auto frame = Frame(k_FrameSeconds);
	frame.view.screenSize = {2048.0f, 1536.0f};
	frame.places = {{400.0f, 300.0f}};
	system.Update(frame);
	ASSERT_EQ(system.GetPath().Count(), 1u);
	EXPECT_EQ(system.GetPath().At(0).screen, glm::vec2(200.0f, 150.0f));
}

TEST(GestureSystem, AMiraclesFirstTurnInTheHandStartsThePathAgain)
{
	GestureSystem system;
	for (int i = 0; i < 10; ++i)
	{
		auto frame = Frame(k_FrameSeconds);
		frame.places = {{100.0f + static_cast<float>(i * 10), 100.0f}};
		system.Update(frame);
	}
	ASSERT_FALSE(system.GetPath().Empty());
	system.SeedFirstTurnInHand();
	EXPECT_TRUE(system.GetPath().Empty());
	// The next update takes a sample at the last place of the pointer first, then at the places of its frame
	auto next = Frame(k_FrameSeconds);
	next.places = {{300.0f, 120.0f}};
	system.Update(next);
	ASSERT_EQ(system.GetPath().Count(), 2u);
	EXPECT_EQ(system.GetPath().At(0).screen, glm::vec2(190.0f, 100.0f));
	EXPECT_EQ(system.GetPath().At(1).screen, glm::vec2(300.0f, 120.0f));
	// With no leash picker open, the hand shows no effect of a selection
	EXPECT_FALSE(system.IsSelecting());
}

TEST(GestureSystem, WhileTheGameIsPausedThePathTakesNoSamples)
{
	GestureSystem system;
	auto frame = Frame(k_FrameSeconds);
	frame.places = {{100.0f, 100.0f}, {110.0f, 100.0f}};
	system.Update(frame);
	ASSERT_EQ(system.GetPath().Count(), 2u);
	auto paused = Frame(k_FrameSeconds);
	paused.paused = true;
	paused.places = {{120.0f, 100.0f}, {130.0f, 100.0f}};
	system.Update(paused);
	EXPECT_EQ(system.GetPath().Count(), 2u);
	// The path stays, and continues when the game runs again
	auto running = Frame(k_FrameSeconds);
	running.places = {{140.0f, 100.0f}};
	system.Update(running);
	ASSERT_EQ(system.GetPath().Count(), 3u);
	EXPECT_EQ(system.GetPath().At(2).screen, glm::vec2(140.0f, 100.0f));
}

TEST(GestureSystem, ADrawnPathIsTakenInPlaceOfTheCursor)
{
	GestureSystem system;
	const auto path = gesture::TraceScribble({500.0f, 300.0f}, 300.0f, 3);
	system.DrawPath(path, false);
	EXPECT_TRUE(system.IsDrawingPath());
	for (size_t i = 0; i < path.size(); ++i)
	{
		system.Update(Frame(k_FrameSeconds));
	}
	EXPECT_FALSE(system.IsDrawingPath());
	ASSERT_EQ(system.GetPath().Count(), path.size());
	EXPECT_EQ(system.GetPath().At(0).screen, path.front());
	// Without the game's templates nothing is recognised
	EXPECT_FALSE(system.GetLastRecognised().has_value());
}

TEST(GestureSystem, NothingIsRecordedUntilTheCursorIsOverTheLand)
{
	GestureSystem system;
	auto frame = Frame(k_FrameSeconds);
	frame.view.landAt = [](glm::vec2 /*pixel*/) { return std::optional<glm::vec3>(); };
	system.Update(frame);
	EXPECT_TRUE(system.GetPath().Empty());
}

TEST(GestureSystem, MovingTheCameraWipesThePathUnlessItShakes)
{
	GestureSystem system;
	int step = 0;
	const auto drawn = [&system, &step](glm::vec3 eye, bool shaking, int samples) {
		for (int i = 0; i < samples; ++i, ++step)
		{
			auto frame = Frame(k_FrameSeconds);
			frame.places = {{100.0f + static_cast<float>(step * 20), 100.0f}};
			frame.view.cameraEye = eye;
			frame.cameraShaking = shaking;
			system.Update(frame);
		}
	};
	drawn({0.0f, 50.0f, 0.0f}, false, 5);
	EXPECT_EQ(system.GetPath().Count(), 5u);
	// The camera moves while it shakes: the path stays and goes on
	drawn({1.0f, 50.0f, 0.0f}, true, 1);
	EXPECT_EQ(system.GetPath().Count(), 6u);
	// It moves without shaking: the path starts again from this frame's point
	drawn({2.0f, 50.0f, 0.0f}, false, 1);
	EXPECT_EQ(system.GetPath().Count(), 1u);
	// Still, it keeps the path
	drawn({2.0f, 50.0f, 0.0f}, false, 3);
	EXPECT_EQ(system.GetPath().Count(), 4u);
}

TEST(GestureSystem, ANewLandKeepsThePathUntilTheCameraMoves)
{
	GestureSystem system;
	int step = 0;
	const auto drawn = [&system, &step](glm::vec3 eye, int samples) {
		for (int i = 0; i < samples; ++i, ++step)
		{
			auto frame = Frame(k_FrameSeconds);
			frame.places = {{100.0f + static_cast<float>(step * 20), 100.0f}};
			frame.view.cameraEye = eye;
			system.Update(frame);
		}
	};
	drawn({0.0f, 50.0f, 0.0f}, 5);
	ASSERT_EQ(system.GetPath().Count(), 5u);
	system.Reset();
	EXPECT_EQ(system.GetPath().Count(), 5u);
	// The camera's first move on the new land wipes it, as any camera move does
	drawn({300.0f, 80.0f, 900.0f}, 1);
	EXPECT_LE(system.GetPath().Count(), 1u);
}

TEST(GestureSystem, ACameraThatLetsTheHandDrawKeepsThePathAsItMoves)
{
	GestureSystem system;
	int step = 0;
	const auto drawn = [&system, &step](glm::vec3 eye, bool letsDraw, int samples) {
		for (int i = 0; i < samples; ++i, ++step)
		{
			auto frame = Frame(k_FrameSeconds);
			frame.places = {{100.0f + static_cast<float>(step * 20), 100.0f}};
			frame.view.cameraEye = eye;
			frame.cameraLetsHandDraw = letsDraw;
			system.Update(frame);
		}
	};
	drawn({0.0f, 50.0f, 0.0f}, true, 3);
	// The fight view or the creature's camera moves each frame: the path goes on
	drawn({1.0f, 50.0f, 0.0f}, true, 1);
	drawn({2.0f, 50.0f, 0.0f}, true, 1);
	EXPECT_EQ(system.GetPath().Count(), 5u);
	// Another camera that moves wipes it
	drawn({3.0f, 50.0f, 0.0f}, false, 1);
	EXPECT_EQ(system.GetPath().Count(), 1u);
}

TEST(GestureSystem, TheMoveButtonHeldInAFightWipesThePath)
{
	GestureSystem system;
	int step = 0;
	const auto drawn = [&system, &step](bool moveHeld, int samples) {
		for (int i = 0; i < samples; ++i, ++step)
		{
			auto frame = Frame(k_FrameSeconds);
			frame.places = {{100.0f + static_cast<float>(step * 20), 100.0f}};
			frame.moveHeldInFight = moveHeld;
			system.Update(frame);
		}
	};
	drawn(false, 4);
	EXPECT_EQ(system.GetPath().Count(), 4u);
	// Held, the path is gone and nothing is added
	drawn(true, 3);
	EXPECT_TRUE(system.GetPath().Empty());
	// Let go, a new path starts
	drawn(false, 2);
	EXPECT_EQ(system.GetPath().Count(), 2u);
}
