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

#include <optional>

#include <glm/vec2.hpp>

#include "CameraHelpEvents.h"
#include "CameraLimits.h"
#include "CameraModel.h"
#include "CameraPan.h"
#include "Common/Zoomer.h"
#include "DoubleClickFlight.h"
#include "PlanetNavigation.h"

class TestDefaultCameraModel;
class TestDefaultCameraModel_single_line_Test;

namespace openblack
{
class DefaultWorldCameraModel final: public CameraModel
{
	enum class Mode : std::uint8_t
	{
		Cartesian,
		Polar,
		ArcBall,
		DraggingLandscape,
		FlyingToPoint,
	};

public:
	DefaultWorldCameraModel();
	/// A camera taking over from another one keeps its pose: it looks from origin at focus, as it was left
	DefaultWorldCameraModel(glm::vec3 origin, glm::vec3 focus);
	~DefaultWorldCameraModel() final;

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) final;
	void HandleActions(std::chrono::microseconds dt) final;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) final;
	/// Flies back to where it was, as when the camera is given back after the hand lets go of a creature: as SetFlight,
	/// wooshing only when asked
	void FlyBack(glm::vec3 origin, glm::vec3 focus, bool woosh);
	void PlaceAt(glm::vec3 origin, glm::vec3 focus) final;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const final;
	[[nodiscard]] glm::vec3 GetTargetFocus() const final;
	[[nodiscard]] Asked GetAsked() const final
	{
		return {.turn = _rotateAroundDelta.y, .tilt = _rotateAroundDelta.x, .zoom = _rotateAroundDelta.z};
	}
	[[nodiscard]] std::chrono::seconds GetIdleTime() const final;
	[[nodiscard]] HandCues GetHandCues() const final;
	/// EXPERIMENT (not for merge): over the planet the camera goes round the world's ball, away from the land
	[[nodiscard]] bool HeldInsideTheWorld() const final { return !_overPlanet; }
	/// EXPERIMENT (not for merge): far round the planet the camera has the frame of the point where it came down
	[[nodiscard]] glm::quat PlanetFrame() const final;
	[[nodiscard]] glm::vec3 ToWorldFrame(glm::vec3 point) const final;
	[[nodiscard]] glm::vec3 FromWorldFrame(glm::vec3 point) const final;

private:
	void UpdateCameraInterpolationValues(const Camera& camera);
	void UpdateRaycastHitPoints(const Camera& camera);
	void UpdateFocusDistance();
	/// What a drag of the land does, from where it was pressed and how the mouse moves: pans, turns round the edge or
	/// tilts. Gives the mode the camera takes for it.
	[[nodiscard]] Mode HandleDrag(bool held);
	/// The help's profile counts the camera controls used
	static void SendHelpEvents(camera_help::events::EventSet events);
	/// Notes what the helper icons by the hand are picked from, once the controls are handled
	void UpdateIconFrame(bool onFight);
	/// The part of the screen the edge controls are laid out on: a 4:3 screen's worth, measured by the cinema bars'
	/// picture while they are in
	[[nodiscard]] static camera_drag::EdgeArea EdgeArea(glm::ivec2 screenSize);

	void UpdateMode(const Camera& camera, glm::vec3 eulerAngles, float zoomDelta, glm::uvec2 mouseCurrent);
	void UpdateModeCartesian();
	void UpdateModePolar(glm::vec3 eulerAngles, bool recalculatePoint);
	void UpdateModeArcBall(glm::vec3 eulerAngles, glm::u16vec2 mouseCurrent, float xFov);
	void UpdateModeDragging(const Camera& camera, glm::u16vec2 mouseCurrent);
	/// Flies to the thing under the hand, else the land or sea under the cursor, for a double click or land gripped too far
	/// ahead; nothing happens without such a place. Whether it flies.
	bool FlyTo(camera_fly::Reason reason);
	/// One of the four wooshes of a flight
	static void Woosh();

	/// Updates the model's focus point parameters after a change in position or focus point of view
	void UpdateFocusPointInteractionParameters(glm::vec3 origin, glm::vec3 focus, glm::vec3 eulerAngles, const Camera& camera);
	/// Modifies the given Euler angles based on the rotate Around and keyboard Move Deltas for rotation and zoom.
	/// @param eulerAngles A reference representing Euler angles (yaw, pitch, roll) to be adjusted. Roll is always 0.
	void TiltZoom(glm::vec3& eulerAngles, float scalingFactor, float zoomDelta);
	/// How far a unit of zoom input moves the camera, growing with the camera's height above its focus
	[[nodiscard]] float GetZoomScale() const;
	/// Computes the harmonic mean of the distances from a point of origin to a set of points determined by raycasting in screen
	/// space.
	///
	/// The function casts 16 rays from the center of the screen to vertically distributed points on the screen.
	/// The harmonic mean of these distances is then calculated by averaging their reciprocals and taking the reciprocal of that
	/// average.
	///
	/// @return The harmonic mean of the distances from the origin to each hit point.
	[[nodiscard]] float GetVerticalLineInverseDistanceWeighingRayCast(const Camera& camera) const;

	void ComputeDistanceFromBoundY();
	/// The zones the land's scripts have set, or none
	[[nodiscard]] static camera_zones::Zones CurrentZones();
	/// Keeps the camera where it may go, in the game's order: out of the zones' cylinders, off the land and the domes,
	/// inside the fence, above the floor and under the ceiling over where it started the frame, within the disc round the
	/// island, and no further from where it started than a frame allows. Whether it was moved; a fence hit is heard at
	/// `listener`.
	struct LimitsMoved
	{
		/// The camera itself was moved
		bool pushed {false};
		/// What it looks at was moved, or it was brought back inside the disc
		bool focusMoved {false};
		[[nodiscard]] bool Any() const { return pushed || focusMoved; }
	};
	LimitsMoved ApplyLimits(glm::vec3 originAtFrameStart, glm::vec3 listener, std::chrono::microseconds dt, float zoomDelta,
	                        glm::vec3 eulerAngles, const Camera& camera);
	/// Puts the camera back inside the zones' fence; whether it was outside
	bool KeepInsideFence(const camera_zones::Zones& zones, glm::vec3 originAtFrameStart, glm::vec3 listener);
	/// Whether the player is using any of the camera's controls this frame
	[[nodiscard]] bool AnyControlHeld() const;

	[[nodiscard]] glm::vec3 GetTargetForwardVector() const;
	[[nodiscard]] glm::vec3 GetTargetForwardUnitVector() const;
	[[nodiscard]] glm::vec3 ProjectPointOnForwardVector(float distanceFromOrigin) const;

	[[nodiscard]] std::optional<CameraInterpolationUpdateInfo> ComputeUpdateReturnInfo(LimitsMoved moved,
	                                                                                   std::chrono::microseconds t);

	/// EXPERIMENT (not for merge): the camera over the planet. The model works on a camera over the map (the local
	/// camera). The world's camera is the local camera blended to the orbit view by the height, then turned about the
	/// ball's middle by _planetTurn.
	[[nodiscard]] static std::optional<planet_nav::Ball> PlanetBall();
	/// The world's camera for the local camera, none when it is the local camera itself
	[[nodiscard]] std::optional<planet_nav::Pose> PlanetPose() const;
	/// Changes the camera that the frame draws back to the local camera
	void ToLocal(const planet_nav::Ball& ball);
	/// Where the line through a point of the screen meets the curved sea, as the frame draws it, from the kept view
	[[nodiscard]] static std::optional<glm::vec3> CurvedSeaUnder(const Camera& camera, glm::vec2 screenCoord, bool islandFar);
	/// The camera is low down far round the planet, in the frame of the point where it came down (planet_nav::Settle)
	[[nodiscard]] bool IslandFar() const;
	/// The hand grips the planet or the far sea as a drag starts; whether it did
	bool GripPlanet(const Camera& camera);
	/// A drag of the planet or the far sea; whether it was one
	bool DragPlanet(glm::u16vec2 mouseCurrent);
	/// The turn and the zoom over the planet, before the local camera takes them
	void TurnAndZoomOverPlanet(const planet_nav::Ball& ball, float& zoomDelta);

	Mode _mode = Mode::Cartesian;
	Mode _modePrev = _mode;

	// Values from camera state where the camera has interpolated to.
	glm::vec3 _currentOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 _currentFocus = glm::vec3(0.0f, 0.0f, 0.0f);

	// Values from target camera state which the camera may interpolate to. Not the current camera state.
	glm::vec3 _targetOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 _targetFocus = glm::vec3(0.0f, 0.0f, 0.0f);
	float _arcBallRadius = 0.0f;

	std::optional<glm::vec3> _screenSpaceMouseRaycastHit;
	std::optional<glm::vec3> _screenSpaceMouseRaycastHitAtClick;
	std::optional<glm::vec3> _screenSpaceCenterRaycastHit;
	/// The last land or sea the middle of the screen showed, kept while it shows none
	glm::vec3 _lastScreenCentreHit {0.0f};

	// State of input Action
	glm::vec3 _rotateAroundDelta = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec2 _keyBoardMoveDelta = glm::vec2(0.0f, 0.0f);
	std::optional<glm::vec3> _handPosition;

	float _focusDistance = 0.0f;
	float _distanceFromBoundY = 0.0f;

	// Estimate of camera to island geometry
	float _averageIslandDistance = 0.0f;

	// Updated at the start of a click+drag or keyboard input
	// Only useful for interaction.
	float _originFocusDistanceAtInteractionStart = 0.0f;
	glm::vec3 _originToHandPlaneNormal = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 _originAtClick = glm::vec3(0.0f, 0.0f, 0.0f);
	float _alignmentAtInteractionStart = 0.0f;
	glm::vec3 _focusAtClick = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::u16vec2 _mouseAtClick = glm::u16vec2(0.0f, 0.0f);
	std::chrono::microseconds _elapsedTime = std::chrono::microseconds::zero();
	std::optional<FlightPath> _flightPath;

	/// The camera hints where the cursor is, with nothing dragged
	uint32_t _tricons {camera_drag::tricon::k_Idle};
	/// A drag of the land, and where the cursor is held dragging round the edge
	bool _dragging {false};
	camera_drag::DragClassifier _drag;
	camera_drag::RingTracker _ring;
	/// Where the land was gripped as the drag was pressed: the camera then, the cursor, the plane the land is dragged
	/// across, whether there was land under the cursor and how far ahead it was
	struct LandGrip
	{
		glm::vec3 origin;
		glm::vec3 focus;
		glm::u16vec2 cursor;
		camera_pan::GripPlane plane;
		bool land;
		float depth;
	};
	std::optional<LandGrip> _landGrip;
	/// What the camera lets the player do this frame, as the scripts allow and a fight changes it
	uint32_t _features {camera_drag::k_DefaultFeatures};
	/// A drag gripping land too far ahead is given up until the buttons are let go
	bool _dragGivenUp {false};
	/// Land was just gripped too far ahead: the camera flies there this frame
	bool _flyToGrip {false};
	/// The camera set off on a flight this frame
	bool _flewThisFrame {false};
	/// The self-tilting camera eases to its place over a second this frame
	bool _selfTiltEase {false};
	/// Put back inside the fence, the camera ignores its controls until every one of them is let go
	bool _heldBack {false};
	/// The angle the player turned the camera by this frame, in radians, none when the player didn't turn it
	std::optional<float> _turnedThisFrame;
	/// The self-tilting camera goes back over where it stood at the start of the frame: it tilted itself, or nothing was
	/// asked of it
	bool _keepsToItsPlace {true};
	camera_drag::TwoButtonTurn _twoButtonTurn;
	/// How far the clear view of Ctrl and Shift held together has come, easing in and out over half a second
	Zoomer _clearView;
	/// The player's camera controls this frame, and the help events they make while the controls are handled
	camera_help::events::ControlsFrame _helpControls;
	camera_help::events::EventSet _helpEvents;
	/// What the helper icons by the hand are picked from this frame
	hand_tricons::WorldCameraFrame _iconFrame;
	/// Time spent handling the controls, for timing the start of a drag
	std::chrono::microseconds _controlsTime {std::chrono::microseconds::zero()};

	/// EXPERIMENT (not for merge): the turn of the world's camera about the ball's middle
	glm::quat _planetTurn {1.0f, 0.0f, 0.0f, 0.0f};
	/// The share of the orbit view in the last frame
	float _orbitShare {0.0f};
	/// The last frame gave the world's camera, not the local camera
	bool _overPlanet {false};
	/// Far round the planet: the place of the local camera across the ground where it came down (planet_nav::Settle)
	std::optional<glm::vec2> _anchorSpot;
	/// The last frame was far round the planet
	/// The frame that the camera worked in on the last frame (PlanetFrame)
	glm::quat _shownFrame {1.0f, 0.0f, 0.0f, 0.0f};
	/// The local camera as the last frame gave it, and the world's eye that the last frame gave for it. While the
	/// camera's destination stays at that eye, the local camera comes back exactly, with no loss in the turn.
	glm::vec3 _localOrigin {0.0f};
	glm::vec3 _localFocus {0.0f};
	glm::vec3 _worldOrigin {0.0f};
	/// The hand gripped the planet (globe drag) or the far sea (curved drag): the world's camera then and the inverse of
	/// its view and projection, the gripped point (for the globe, a direction from the ball's middle), the turn then
	struct PlanetGrip
	{
		bool globe;
		planet_nav::Pose pose;
		glm::mat4 inverseViewProjection;
		glm::vec3 gripped;
		glm::quat turn;
	};
	std::optional<PlanetGrip> _planetGrip;
	/// The line through the cursor and the middle of the screen meet the curved sea here, where they meet no land
	std::optional<glm::vec3> _curvedMouseHit;

	// For unit testing
	friend TestDefaultCameraModel;
	friend TestDefaultCameraModel_single_line_Test;
};
} // namespace openblack
