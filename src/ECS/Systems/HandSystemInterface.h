/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{
class HandSystemInterface
{
public:
	virtual ~HandSystemInterface() = default;

	enum class Side : uint8_t
	{
		Left,
		Right,
		_Count
	};
	virtual bool Initialize() noexcept = 0;
	[[nodiscard]] virtual std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept = 0;
	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept = 0;

	/// Places the player hand for this frame.
	/// @param groundPoint point of the landscape under the cursor (nullopt when the cursor is in the sky).
	/// @param cameraForward camera view direction; the fingers point along it projected on the ground.
	/// @param gripping true while the land is gripped: the fingertips stay dug into the grab point.
	virtual void Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
	                   std::chrono::microseconds dt) noexcept = 0;

	/// Picks the object under the cursor ray and returns the point on the mouse ray where the hand goes this frame, at
	/// the distance of the surface under the cursor smoothed by the hand distance Zoomer. land is the landscape (or sea)
	/// point under the cursor. gripping: the land is held (camera drag), only the landscape counts.
	[[nodiscard]] virtual std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
	                                                                  std::optional<glm::vec3> land, bool gripping,
	                                                                  std::chrono::microseconds dt) noexcept = 0;
	/// Advances the hand animation (hh.HBN). mouseDelta is the cursor motion in pixels since the last frame.
	/// actionHeld: the action (right) button; pressing it over an object picks it up, releasing drops it.
	virtual void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept = 0;
	/// Object currently held by the player hand, if any.
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldObject() const noexcept = 0;
	/// The hand takes the object (a spell seed from the magic code)
	virtual void PlaceObjectInMagicHand(entt::entity entity) noexcept = 0;
	/// The player who owned what the hand last picked up, recorded at the pick-up (the interface's own record, which
	/// every deposit of that interface carries); none until something records one
	[[nodiscard]] virtual std::optional<PlayerNames> GetSourceOwner() const noexcept = 0;
	/// Space in the hand (and no hand action locked)
	[[nodiscard]] virtual bool IsHandReadyForObject() const noexcept = 0;
	/// The held object leaves the hand with no velocity (packet 0x1D, thrown from the hand with no speed); a spell seed
	/// goes back to its worship site or is deleted
	virtual void ForceDropHeld() noexcept = 0;
	/// The action in progress ends (a seed that becomes ready ends its press)
	virtual void EndAction() noexcept = 0;
	/// For a spell cast from this hand: the point under the hand, the hand, the camera's forward and the hand's velocity
	virtual void GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
	                          glm::vec3& velocity) const noexcept = 0;
	/// The hand's scale (set from its distance to the view)
	[[nodiscard]] virtual float GetHandScale() const noexcept = 0;
	/// The player hand's model position and matrix
	[[nodiscard]] virtual glm::mat4 GetHandMatrix() const noexcept = 0;
	/// The hand's reach (1800 to begin with), at most 1800; SET_INTERFACE_INTERACTION sets it (75 for JUST_GRAB)
	virtual void SetHandReach(float metres) noexcept = 0;
	[[nodiscard]] virtual float GetHandReach() const noexcept = 0;
	/// The object tapped or clicked in the last 15 s (GAME_THING_CLICKED); entt::null when none or it no longer exists
	[[nodiscard]] virtual entt::entity GetClickedObject() const noexcept = 0;
	/// Forgets the clicked object (CLEAR_CLICKED_OBJECT)
	virtual void ClearClicked() noexcept = 0;
	/// The object and the current turn
	virtual void RememberTapped(entt::entity object) noexcept = 0;
	/// POSITION_CLICKED: the distance in metres from the last land tap to `position` <= radius. The tap is the land point
	/// of the last land tap in the last 15 s, (0, 0, 0) once cleared
	[[nodiscard]] virtual bool PositionClicked(const glm::vec3& position, float radius) const noexcept = 0;
	/// CLEAR_CLICKED_POSITION: the tapped point = 0, its turn stays
	virtual void ClearClickedPosition() noexcept = 0;
	/// The interface's hand part, once a turn right after game_packets::DispatchQueuedPackets
	virtual void ProcessTurn() noexcept = 0;
	/// The hand's turn update, its hold part (step 28, magic::ProcessHandTurn): the hand lets go of an object that is
	/// no longer available, and forgets a released one that is gone
	virtual void GameTurnUpdate() noexcept = 0;
	/// The object the render hand holds, from the press until it is thrown; the status's GetHeldObject only from the
	/// 0x13. The draw's hand-held readers (shadows, reflection) ask this one
	[[nodiscard]] virtual std::optional<entt::entity> GetRenderHandObject() const noexcept = 0;
	/// A hand demo's record sets the hand's throw block (velocity, angular velocity, HandPos, angles) before its message
	virtual void SetThrowBlock(const std::array<float, 12>& block) noexcept = 0;
	/// On a new land: the synced hand, the turn motion and the throw data
	virtual void ResetTurnState() noexcept = 0;
	/// The synced hand's velocity, units per second (the landscape's hand wind reads it)
	[[nodiscard]] virtual glm::vec3 GetTurnHandVelocity() const noexcept = 0;
	/// The interface's hand state of the last turn: GET_HAND_STATE 413
	[[nodiscard]] virtual int32_t GetInterfaceHandState() const noexcept = 0;
	/// Once a turn, before the tooltips: the hand state GetInterfaceHandState (GET_HAND_STATE) and the tooltips read
	virtual void UpdateInterfaceHandState() noexcept = 0;
	/// From the «Did you know?» bubble's draw: a screen object under the mouse offers itself with its depth into the
	/// pending collide; the nearest wins (a tie keeps the first). While the hand grips one nothing changes and it
	/// returns true; else true when it was taken
	[[nodiscard]] virtual bool OfferScreenObject(uint32_t id, float depth) noexcept = 0;
	/// The screen object under the hand this frame (last frame's offer)
	[[nodiscard]] virtual std::optional<uint32_t> ScreenObjectUnderHand() const noexcept = 0;
	/// The screen object under the hand while the hand grips it (action state 17 BUBBLE GRIP, hand state 0x1D): the
	/// bubble scrolls with the mouse
	[[nodiscard]] virtual std::optional<uint32_t> DraggedScreenObject() const noexcept = 0;
	/// Objects thrown by the hand that are still in flight (the original's physics objects)
	[[nodiscard]] virtual std::vector<entt::entity> GetThrownObjects() const noexcept = 0;
	/// Animated global bone matrices of the player hand, or nullptr when hh.HBN is not loaded.
	[[nodiscard]] virtual const std::vector<glm::mat4>* GetBoneMatrices() const noexcept = 0;
	[[nodiscard]] virtual std::vector<std::string> GetAnimationNames() const noexcept = 0;
	[[nodiscard]] virtual const std::string& GetCurrentAnimation() const noexcept = 0;
	/// Forces a C node for debugging; an empty name returns control to the gameplay state machine.
	virtual void SetAnimationOverride(const std::string& name) noexcept = 0;
	/// The hand plays `clip` once, pinned upright at `point` (the play-animation state); a call
	/// during the clip moves the point and keeps the time
	virtual void StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept = 0;
};
} // namespace openblack::ecs::systems
