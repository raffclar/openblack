/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <numbers>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"

/// The camera's fight view: watching two creatures fight in an arena. It flies in to a side of the arena, then keeps
/// both fighters side on in the middle of the view, easing round as they move, as far back as their sizes and the gap
/// between them need. The player may turn and tilt it, and zoom it in and out; zoomed far enough out it leaves the fight.
namespace openblack::fight_view
{
/// It starts this many of the first creature's radii further back than the gap between the fighters needs
constexpr float k_StartScale = 5.5f;
/// It starts looking at the fight from a quarter turn round from the line between the fighters: side on
constexpr float k_StartHeadingOffset = std::numbers::pi_v<float> / 2.0f;
/// It starts looking down this far, in radians, and keeps between these
constexpr float k_StartPitch = 0.408407032f;
constexpr float k_MinPitch = 0.241660982f;
constexpr float k_MaxPitch = 1.32913542f;
/// Zoomed out past this scale it leaves the fight, and the scale stays within it
constexpr float k_MaxScale = 40.0f;
/// A zoom moves it back this share of the zoom's distance
constexpr float k_ZoomShare = 0.3f;
/// It looks at the middle of the fighters raised by this share of their heights added together
constexpr float k_HeightShare = 0.25f;
/// Its heading and where it looks ease to where they should be over this many seconds
constexpr float k_EaseSeconds = 5.0f;
/// The fighters' line turns the view only once they are this far apart along either axis
constexpr float k_TurnApart = 0.01f;
/// It turns by half a turn for the mouse's movement across the whole screen
constexpr float k_TurnPerScreen = std::numbers::pi_v<float>;
/// It tilts by this for each unit of tilt input
constexpr float k_TiltPerInput = 0.002f;
/// The fight view stays this long once the fight is over
constexpr float k_EndSoonSeconds = 3.0f;
/// Looking at an arena from within its radius this long starts the fight view
constexpr float k_LookSeconds = 1.0f;

/// A zoom's distance, from the camera's zoom input and its height above where it looks: zooming speeds up with height
[[nodiscard]] float ZoomDistance(float zoomInput, float heightAboveFocus);

/// The direction of a line across the land, as the game measures it
[[nodiscard]] float AngleOf(glm::vec2 line);
/// An angle brought within half a turn either way
[[nodiscard]] float Wrapped(float angle);
/// A point a distance from another at a heading and pitch
[[nodiscard]] glm::vec3 PointFrom(const glm::vec3& from, float distance, float heading, float pitch);
struct HeadingPitch
{
	float heading;
	float pitch;
};
/// The heading and pitch from a point looking at another
[[nodiscard]] HeadingPitch HeadingAndPitch(const glm::vec3& origin, const glm::vec3& focus);

using GroundHeight = std::function<float(glm::vec2)>;

/// The heading round a point that sees it best: of 32 headings from the one given, the one with the most land below the
/// point out to the distance, favouring headings near the one given. The pitch eases towards the lean of the land there
/// and is kept between an eighth and a third of a half turn.
[[nodiscard]] float BestHeading(float heading, float distance, const glm::vec3& focus, float& pitch, const GroundHeight& ground,
                                const glm::vec3& landNormal);
struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};
/// Where the camera should go to look from one point at another: as far as it is, eased to between 25 and 50 away, at
/// the best heading round the point
[[nodiscard]] View Suggest(const glm::vec3& from, const glm::vec3& focus, const GroundHeight& ground,
                           const glm::vec3& landNormal);
/// Where the camera flies to as the fight view starts: from the side of the arena its radius away and half that up,
/// looking at its middle
[[nodiscard]] View Start(glm::vec2 arenaCentre, float arenaRadius, float ground, const GroundHeight& groundAt,
                         const glm::vec3& landNormal);

/// Whether a camera has strayed too far from an arena to watch its fight, by a scale of its radius: its eye more than
/// 3.2 radii away and where it looks more than 4.2, or where it looks more than 6
[[nodiscard]] bool TooFar(glm::vec2 eye, glm::vec2 looking, glm::vec2 arenaCentre, float arenaRadius, float scale);
/// Whether a camera looks at an arena from within it: its eye and where it looks both within its radius
[[nodiscard]] bool WithinArena(glm::vec2 eye, glm::vec2 looking, glm::vec2 arenaCentre, float arenaRadius);

/// A fighter as the view frames it: where it is, its radius and its height
struct Fighter
{
	glm::vec3 position;
	float radius;
	float height;
};
/// What the player did to the view this frame: turned it across (as the camera's rotate input), tilted it, and zoomed it
/// by a distance
struct Input
{
	float turn {0.0f};
	float tilt {0.0f};
	float zoom {0.0f};
	float screenWidth {1.0f};
};

/// The view following a fight
class Tracker
{
public:
	/// Follows the fighters for a frame, the first of them the one that made the arena: where the camera goes, and
	/// whether the player zoomed out of the fight
	struct Step
	{
		View view;
		bool zoomedOut;
	};
	Step Follow(const Fighter& first, const Fighter& second, const Input& input, float seconds);

	[[nodiscard]] float GetScale() const { return _scale; }
	[[nodiscard]] float GetPitch() const { return _pitch; }
	[[nodiscard]] float GetHeadingOffset() const { return _headingOffset; }
	/// The next frame puts its heading and where it looks where they should be at once, as when the view comes back
	void Resnap() { _snap = true; }

private:
	float _headingOffset {k_StartHeadingOffset};
	float _pitch {k_StartPitch};
	float _scale {k_StartScale};
	Zoomer _heading;
	Zoomer3 _focus;
	/// The first frame puts it where it should be at once
	bool _snap {true};
};

/// The camera itself goes to the view on curves, sent again each frame. Each curve takes 5 seconds when the camera's
/// model has just taken control. This time falls evenly to 2 seconds over the first 2 seconds of the model's time, and
/// then stays at 2 seconds.
constexpr float k_BlendFirstScale = 2.0f;
constexpr float k_BlendLastScale = 0.8f;
constexpr float k_BlendFallSeconds = 2.0f;
constexpr float k_BlendSecondsPerScale = 2.5f;
/// The seconds of the camera's curve to the view, from the seconds since the camera's model took control
[[nodiscard]] float BlendSeconds(float modelSeconds);

/// A flight to the view first goes to a point on the way. The camera starts to that point on a curve of 0.9 of the
/// flight's 1.5 seconds. After more than half the flight on that curve, it goes to the view on a curve of 1.5 seconds,
/// and the view's own curves then continue from there.
constexpr float k_FlightSeconds = 1.5f;
constexpr float k_WayShare = 0.9f;
constexpr float k_OnWayShare = 0.5f;
/// The point on the way is half way between the camera and where it flies to. It is raised by a share of the distance
/// across the land between them (none at the start of the view, a tenth when the view comes back), and stays at least
/// this high above the land.
constexpr float k_StartWayRise = 0.0f;
constexpr float k_ReturnWayRise = 0.1f;
constexpr float k_WayAboveLand = 10.0f;
[[nodiscard]] glm::vec3 WayPoint(const glm::vec3& from, const glm::vec3& to, float rise, const GroundHeight& ground);

/// The camera itself, chasing the view the fight wants: where it is and how fast it moves, as it flies to the view and then
/// follows it
class Chase
{
public:
	/// Starts from where the camera is and how fast it moves, and flies to the view through the point on the way
	void Fly(const View& from, const View& speed, const View& to, const glm::vec3& way);
	/// Moves on for a step of the camera. The fight's view is sent again each frame after the flight, on a curve of the
	/// blend's seconds. With no view, the camera continues along its curves.
	View Step(const std::optional<View>& wanted, float modelSeconds, float seconds);

	[[nodiscard]] View GetValue() const { return {.origin = _origin.GetValue(), .focus = _focus.GetValue()}; }
	[[nodiscard]] View GetSpeed() const { return {.origin = _origin.GetSpeed(), .focus = _focus.GetSpeed()}; }
	/// Where the camera was when the last step started
	[[nodiscard]] const View& GetHeld() const { return _held; }
	[[nodiscard]] bool Flying() const { return _flight.has_value(); }

private:
	struct Flight
	{
		View to;
		glm::vec3 way;
		bool wayGiven {false};
	};
	Zoomer3 _origin;
	Zoomer3 _focus;
	View _held {};
	std::optional<Flight> _flight;
};

} // namespace openblack::fight_view
