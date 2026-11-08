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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <GestureFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/GestureEventsInterface.h"
#include "Enums.h"
#include "Gestures/GestureHints.h"
#include "Gestures/GestureMatcher.h"
#include "Gestures/GestureRecorder.h"
#include "Gestures/GestureRequests.h"
#include "Gestures/GestureTrailBuilder.h"

namespace openblack::ecs::systems
{

/// The gestures the player draws with the hand. Once a frame the places of the pointer go into the hand's path, the
/// gestures the hand is waiting for are worked out from what it holds and the player's creature's leash (see
/// gesture::Requests), and the path is matched against them with the game's templates. A gesture for a miracle becomes
/// a GestureEvent for the miracles to take; the leash gestures put the leash on, open the leash picker, change the
/// leash and shake it off through the leash system.
class GestureSystemInterface: public GestureEventsInterface
{
public:
	/// How the screen meets the land this frame
	struct View
	{
		/// The window's size in pixels
		glm::vec2 screenSize {0.0f};
		/// The land (or the sea) under a point of the screen, in pixels; none over the sky
		std::function<std::optional<glm::vec3>(glm::vec2 pixel)> landAt;
		/// The point under a point of the screen at the same distance in front of the camera as another point
		std::function<glm::vec3(glm::vec2 pixel, glm::vec3 sameDepthAs)> atDepthOf;
		/// Which way is right on the screen, in the world
		glm::vec3 cameraRight {1.0f, 0.0f, 0.0f};
		/// Where the camera's eye is. The path is forgotten when it has moved since the last frame, unless the camera
		/// is shaking or lets the hand draw on
		glm::vec3 cameraEye {0.0f};
		/// Which way the camera looks
		glm::vec3 cameraForward {0.0f, 0.0f, 1.0f};
		/// Where a recognised gesture's trail is laid under a pixel of the screen (see gesture::TrailView)
		std::function<glm::vec3(glm::ivec2 pixel)> trailPointUnder;
	};

	/// What a frame tells the gestures
	struct Frame
	{
		float seconds {0.0f};
		/// The places of the pointer that the interface got in this frame, in pixels from the top left, in the sequence
		/// that they came. The path takes a sample at each place.
		std::vector<glm::vec2> places;
		/// Whether the hand draws over the world: not over a debug window, and not in a cut scene. While it does not, the
		/// hand forgets the path and the gesture step does nothing. In the temple, the hand forgets the path and does no pass.
		bool overWorld {true};
		/// Whether the hand of this computer holds a creature, or holds a field, a fish farm or a pile while it takes from
		/// it. While it does, the hand forgets the path and the gesture step does nothing.
		bool handLocked {false};
		/// Whether a front-end box shows: the game's menu, the question at the start of a new game, or the tattoo editor.
		/// While it shows, the hand forgets the path and the gesture step does nothing.
		bool frontEndBox {false};
		/// Whether the Action button is held, which readies a storm or shield to be sized with a circle
		bool actionHeld {false};
		PlayerNames player {PlayerNames::PLAYER_ONE};
		View view;
		/// Whether a camera shake is going on anywhere, near the camera or not
		bool cameraShaking {false};
		/// Whether the camera lets the hand draw on while it moves: the fight view while it shows a fight in its arena, and
		/// the camera that follows the creature in Creature Mode
		bool cameraLetsHandDraw {false};
		/// Whether the Move button is held while the hand directs the fight of the player's creature. The path is
		/// forgotten, and nothing is recognised, while it is.
		bool moveHeldInFight {false};
		/// The messages of the mouse buttons in this frame: a press and a release of the Action button, a press and a
		/// release of the Move button, and a double click of the left button. Each kind sends one message at most. The
		/// interface does its gesture step once after each message, or once when no button sent a message.
		uint32_t buttonMessages {0};
	};

	/// The last gesture recognised, for the debug window
	struct Recognised
	{
		gesture::Request request;
		gesture::Match match;
		/// Where it was drawn on the screen the path is measured on
		gesture::ScreenBox box;
		/// What the miracles were told of it, if anything
		std::optional<GestureEvent> event;
		/// Seconds since
		float age {0.0f};
		/// Counts the gestures recognised, telling one from the next
		uint32_t number {0};
	};

	/// Once a frame: the frame's pass of the interface. It samples the places that came since the last pass, then it does
	/// the gesture step once for each message of the buttons.
	virtual void Update(const Frame& frame) = 0;
	/// Once a turn, before the game logic of the turn: the turn's pass of the interface. It samples the places that came
	/// since the last pass, then it does the gesture step once. The times of the gestures do not count down in this pass.
	virtual void InterfaceTurn(const Frame& /*frame*/) {}
	/// Loads the game's gesture templates, if they are not loaded. The first update also loads them.
	virtual void LoadTemplates() {}
	/// A path drawn as if by the hand, for the testbed: the points are on the screen that the path uses (768 pixels
	/// high). Each sample takes the next point in place of the place of the pointer, with the Action button held
	/// throughout or not. The hand that draws the path moves the mouse, so the places come as quickly as the mouse
	/// sends them.
	virtual void DrawPath(std::vector<glm::vec2> path, bool holdingAction) = 0;
	/// Whether a path given to DrawPath is still being drawn
	[[nodiscard]] virtual bool IsDrawingPath() const = 0;
	/// The next point of a path given to DrawPath, on the screen the path is measured on, where the hand drawing it is;
	/// none once it is drawn
	[[nodiscard]] virtual std::optional<glm::vec2> GetDrawingPoint() const = 0;
	/// Forgets the hand's path
	virtual void ForgetPath() = 0;
	/// A miracle seed in the hand does its first turn there: the hand forgets its path and starts its power-ups for the
	/// seed, with the power-up level of the seed as the one asked for. From then on this land, the chain follows the hand
	/// while it holds any miracle.
	virtual void SeedFirstTurnInHand() { ForgetPath(); }
	/// The Action button armed the miracle in the hand, which the hand casts by its gesture when the button comes up. In
	/// the pass of this frame, after the samples of the places, the hand forgets its path and takes one sample at the last
	/// place of the pointer.
	virtual void RestartPathAtArmedPress() { ForgetPath(); }
	/// A press of a button directs the fight of the player's creature. In the pass of this frame, after the samples of
	/// the places, the hand forgets its path.
	virtual void ForgetPathAtPress() { ForgetPath(); }

	/// The templates the paths are matched against, empty until the game's file is loaded
	[[nodiscard]] virtual std::span<const gestures::GestureTemplate> GetTemplates() const = 0;
	[[nodiscard]] virtual const gesture::GestureRecorder& GetPath() const = 0;
	/// The screen's width over its height, which the templates' aspect tests are measured with
	[[nodiscard]] virtual float GetScreenAspect() const = 0;
	/// What the hand waits for now, in order
	[[nodiscard]] virtual const std::vector<gesture::Request>& GetRequests() const = 0;
	[[nodiscard]] virtual std::optional<Recognised> GetLastRecognised() const = 0;
	/// Whether the leash picker is open, offering the creature's other leashes as gestures
	[[nodiscard]] virtual bool IsLeashPickerOpen() const = 0;
	/// Whether the miracle selection is open, offering the miracles of the player's worship site icons as gestures
	[[nodiscard]] virtual bool IsSelectionOpen() const { return false; }
	/// The key that opens the miracle selection: with the hand ready and the selection closed, the selection opens for
	/// the player's own miracles. Whether it opened.
	virtual bool OpenSelectionByKey(PlayerNames /*player*/) { return false; }
	/// The key that calls the last miracle again: with the hand ready and the selection closed, the icon of the last
	/// miracle that came into the hand gives it again. Whether the icon did something.
	virtual bool RepeatMiracleByKey(PlayerNames /*player*/) { return false; }
	/// Whether the hand shows the effect of a selection, round the hand: while the miracle selection or the leash picker
	/// is open
	[[nodiscard]] virtual bool IsSelecting() const { return false; }
	/// Whether a circle drawn for the storm or shield in the hand is remembered, and for how much longer
	[[nodiscard]] virtual float GetCircleSecondsLeft() const = 0;
	/// The gesture of the power-up that the hand asked for the seed in it, none when it asked for none
	[[nodiscard]] virtual GestureType GetAskedPowerUp() const { return GestureType::None; }
	/// The hand casts the seed in it with a power-up asked for: the hand starts its power-ups again, so it forgets the
	/// power-up asked for and the circle remembered
	virtual void PoweredUpCast() {}
	/// Whether the hand is drawing a gesture that the chain behind it shows: while it holds a miracle that it can power up
	/// or a miracle that a circle sizes, or while it draws a gesture of the open miracle selection. Once a miracle did its
	/// first turn in the hand on this land, or the selection opened once on this land, the chain follows every miracle in
	/// the hand.
	[[nodiscard]] virtual bool IsGesturing() const = 0;
	/// The row of hints of the gestures the hand waits for, at the bottom right of the screen
	[[nodiscard]] virtual const gesture_hints::Row& GetHints() const = 0;
	/// Once a frame, by the game's time: the hints fade while the advisors' words show, and while they show their dots
	/// run and their collars turn
	virtual void StepHints(float seconds, bool wordsShowing) = 0;
	/// A gesture's shape shown on the land as if it had just been drawn and recognised, without anyone drawing it (a
	/// reward showing the gesture of a power-up): its square, so many metres across, laid on the land from a corner
	/// towards the east and south. The gesture of another player is heard at the place of that player's hand; none for a
	/// gesture of this computer's player.
	virtual void ShowGestureOnLand(GestureType /*gesture*/, glm::vec3 /*corner*/, float /*size*/,
	                               std::optional<glm::vec3> /*heardAt*/)
	{
	}
	/// A gesture's shape from its symbol file, or the circle's shape when the gesture has no file
	[[nodiscard]] virtual std::optional<gesture::TrailSymbol> GetSymbol(GestureType /*gesture*/) const { return std::nullopt; }
};

} // namespace openblack::ecs::systems
