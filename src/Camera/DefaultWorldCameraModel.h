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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "CameraHelpEvents.h"
#include "CameraLimits.h"
#include "CameraModel.h"
#include "CameraPan.h"
#include "ClearView.h"
#include "Common/Zoomer.h"
#include "DoubleClickFlight.h"
#include "PushScroll.h"

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
	/// Flies to a place, as a bookmark or a key does. The flight ends at the fence when the place is outside it, and
	/// wooshes only when it starts more than 150 from its end. Its half way point rises by the rise times the distance
	/// across.
	void SetFlight(glm::vec3 origin, glm::vec3 focus, float rise) final;
	/// Flies back to where it was, as when the camera is given back after the hand lets go of a creature: as SetFlight
	/// with a rise of 0.1, wooshing only when asked
	void FlyBack(glm::vec3 origin, glm::vec3 focus, bool woosh);
	void PlaceAt(glm::vec3 origin, glm::vec3 focus) final;
	/// Finds again how far ahead the land is from the view that the camera shows, as a new press of the mouse does.
	/// The single tap of the temple key looks that far ahead. Returns the distance.
	float RefreshClickDistance(const Camera& camera);
	[[nodiscard]] glm::vec3 GetTargetOrigin() const final;
	[[nodiscard]] glm::vec3 GetTargetFocus() const final;
	[[nodiscard]] Asked GetAsked() const final
	{
		return {.turn = _rotateAroundDelta.y,
		        .tilt = _rotateAroundDelta.x,
		        .zoom = _rotateAroundDelta.z,
		        .controlsUsed = _controlsUsed,
		        .landGripped = _landGripped,
		        .landHeld = _landHeld,
		        .dragTurn = _dragTurn,
		        .dragTilt = _dragTilt};
	}
	/// Whether the player moved the camera in the last frame: a control of the camera, or a drag of the mouse of more
	/// than two pixels while the land is gripped or the camera turns round the hand. Such a move stops a follow of a
	/// bookmark.
	[[nodiscard]] bool MovedByPlayer() const { return camera_help::events::IsMoving(_helpControls); }
	/// One of the four wooshes of a flight
	static void Woosh();
	[[nodiscard]] HandCues GetHandCues() const final;

private:
	void UpdateCameraInterpolationValues(const Camera& camera);
	void UpdateRaycastHitPoints(const Camera& camera);
	void UpdateFocusDistance();
	/// What a drag of the land does, from where it was pressed and how the mouse moves: pans, turns round the edge or
	/// tilts. Gives the mode the camera takes for it.
	[[nodiscard]] Mode HandleDrag(bool held);
	/// The help's profile counts the camera controls used
	static void SendHelpEvents(camera_help::events::EventSet events);
	/// Push scrolling for the frame: the speed moves and turns the camera, fades, then takes the frame's push
	void HandlePushScroll(std::chrono::microseconds dt, bool buttonsFree, bool gripping);
	/// Notes what the helper icons by the hand are picked from, once the controls are handled
	void UpdateIconFrame(bool onFight);
	/// Ctrl and Shift held: the clear view starts its orbit, follows the thing under the hand, and eases the camera
	void UpdateClearView(glm::vec3 originAtFrameStart, glm::vec3 focusAtFrameStart);
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

	/// Updates the model's focus point parameters after a change in position or focus point of view
	void UpdateFocusPointInteractionParameters(glm::vec3 origin, glm::vec3 focus, const Camera& camera);
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
	/// A distance of 50 counts as one more hit.
	[[nodiscard]] float GetVerticalLineInverseDistanceWeighingRayCast(const Camera& camera, glm::vec3 origin) const;

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
	                        const Camera& camera);
	/// Puts the camera back inside the zones' fence; whether it was outside
	bool KeepInsideFence(const camera_zones::Zones& zones, glm::vec3 originAtFrameStart, glm::vec3 listener);
	/// Whether the player is using any of the camera's controls this frame
	[[nodiscard]] bool AnyControlHeld() const;

	[[nodiscard]] glm::vec3 GetTargetForwardVector() const;
	[[nodiscard]] glm::vec3 GetTargetForwardUnitVector() const;
	[[nodiscard]] glm::vec3 ProjectPointOnForwardVector(float distanceFromOrigin) const;

	[[nodiscard]] std::optional<CameraInterpolationUpdateInfo> ComputeUpdateReturnInfo(LimitsMoved moved,
	                                                                                   std::chrono::microseconds t);

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
	/// What the player did with the controls this frame, as GetAsked gives it
	bool _controlsUsed {false};
	bool _landGripped {false};
	bool _landHeld {false};
	float _dragTurn {0.0f};
	float _dragTilt {0.0f};
	/// The self-tilting camera goes back over where it stood at the start of the frame: it tilted itself, or nothing was
	/// asked of it
	bool _keepsToItsPlace {true};
	camera_drag::TwoButtonTurn _twoButtonTurn;
	/// How far the clear view of Ctrl and Shift held together has come, easing in and out over half a second
	Zoomer _clearView;
	/// Ctrl and Shift are held, and the clear view starts in this frame (its ease was not yet in)
	bool _clearViewHeld {false};
	bool _clearViewStarts {false};
	/// The orbit that the clear view eases to, the point on the land that it looks at, and the thing under the hand
	/// that the point follows
	std::optional<clear_view::Orbit> _clearOrbit;
	glm::vec3 _clearCentre {0.0f};
	std::optional<entt::entity> _clearObject;
	/// The player's camera controls this frame, and the help events they make while the controls are handled
	camera_help::events::ControlsFrame _helpControls;
	camera_help::events::EventSet _helpEvents;
	/// What the helper icons by the hand are picked from this frame
	hand_tricons::WorldCameraFrame _iconFrame;
	/// Push scrolling: the mouse as the push reads it, the camera's speed across, forwards and round, the push's clock,
	/// and what the speed moves and turns the camera by this frame
	camera_push_scroll::Pointer _pushPointer;
	glm::vec3 _pushSpeed {0.0f};
	camera_push_scroll::MillisecondClock _pushClock;
	glm::vec3 _pushMove {0.0f};
	float _pushTurn {0.0f};
	/// Time spent handling the controls, for timing the start of a drag
	std::chrono::microseconds _controlsTime {std::chrono::microseconds::zero()};

	// For unit testing
	friend TestDefaultCameraModel;
	friend TestDefaultCameraModel_single_line_Test;
};
} // namespace openblack
