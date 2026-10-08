/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdlib>

#include <memory>

#include <L3DFile.h>
#include <entt/entity/entity.hpp>

#include "3D/HandAnimator.h"
#include "Audio/Engine/SamplePlay.h"
#include "Common/Zoomer.h"
#include "ECS/HandCreature.h"
#include "ECS/PotResource.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "Input/GamePackets.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class HandSystem final: public HandSystemInterface
{
public:
	/// HandTurn.cpp: the packet handlers go with the system (game_packets::ClearHandlers)
	~HandSystem();
	bool Initialize() noexcept override;
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override;
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override;

	void Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
	           std::chrono::microseconds dt) noexcept override;
	void Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept override;
	std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
	                                            std::optional<glm::vec3> land, bool gripping,
	                                            std::chrono::microseconds dt) noexcept override;
	/// The held object while it is available (IsInteractable): a deleted one is not held any more (ValidateHands)
	[[nodiscard]] std::optional<entt::entity> GetHeldObject() const noexcept override
	{
		return _held && Interactable(*_held) ? _held : std::nullopt;
	}
	/// PlaceObjectInMagicHand for callers that already ran the object's InterfaceSetInMagicHand: only the hand's
	/// part, which plays no pick-up sound
	void PlaceObjectInMagicHand(entt::entity entity) noexcept override { PickUp(entity, false); }
	[[nodiscard]] std::optional<PlayerNames> GetSourceOwner() const noexcept override { return _sourceOwner; }
	// HandSpellSeed.cpp
	[[nodiscard]] bool IsHandReadyForObject() const noexcept override;
	void ForceDropHeld() noexcept override;
	void EndAction() noexcept override;
	void GetSpellInfo(glm::vec3& interfacePos, glm::vec3& handPos, glm::vec3& cameraForward,
	                  glm::vec3& velocity) const noexcept override;
	[[nodiscard]] float GetHandScale() const noexcept override { return _handScale; }
	[[nodiscard]] glm::mat4 GetHandMatrix() const noexcept override;
	void UpdateInterfaceHandState() noexcept override { _interfaceHandState = InterfaceHandState(); }
	bool OfferScreenObject(uint32_t id, float depth) noexcept override;
	[[nodiscard]] std::optional<uint32_t> ScreenObjectUnderHand() const noexcept override { return _screenObject; }
	[[nodiscard]] std::optional<uint32_t> DraggedScreenObject() const noexcept override
	{
		// the bubble's own test: the interface hand state of the last turn is 0x1D (not 0x1E, the one in the
		// citadel)
		return _interfaceHandState == 0x1D ? _screenObject : std::nullopt;
	}
	[[nodiscard]] int32_t GetInterfaceHandState() const noexcept override { return _interfaceHandState; }
	/// HandTurn.cpp: the hand's part of the interface's processing, once a turn at the turn's start
	void ProcessTurn() noexcept override;
	void GameTurnUpdate() noexcept override;
	[[nodiscard]] std::optional<entt::entity> GetRenderHandObject() const noexcept override
	{
		return _renderHandHeld && Interactable(*_renderHandHeld) ? _renderHandHeld : std::nullopt;
	}
	void SetThrowBlock(const std::array<float, 12>& block) noexcept override;
	/// A hand demo record's hand position and angles, valid until the next per-frame write of the render hand's
	/// state (Place): the seed senders copy them as they are
	std::optional<std::pair<glm::vec3, glm::vec3>> _recordedHandPose;
	void ResetTurnState() noexcept override;
	[[nodiscard]] glm::vec3 GetTurnHandVelocity() const noexcept override { return _turnVelocity; }
	[[nodiscard]] entt::entity GetClickedObject() const noexcept override;
	void ClearClicked() noexcept override;
	void RememberTapped(entt::entity object) noexcept override;
	[[nodiscard]] bool PositionClicked(const glm::vec3& position, float radius) const noexcept override;
	void ClearClickedPosition() noexcept override;
	void SetHandReach(float metres) noexcept override { _handReach = metres <= 1800.0f ? metres : 1800.0f; }
	[[nodiscard]] float GetHandReach() const noexcept override { return _handReach; }
	[[nodiscard]] std::vector<entt::entity> GetThrownObjects() const noexcept override;
	[[nodiscard]] const std::vector<glm::mat4>* GetBoneMatrices() const noexcept override;
	[[nodiscard]] std::vector<std::string> GetAnimationNames() const noexcept override;
	[[nodiscard]] const std::string& GetCurrentAnimation() const noexcept override;
	void SetAnimationOverride(const std::string& name) noexcept override;
	void StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept override;

private:
	void LoadAnimations() noexcept;
	void LoadGeometry() noexcept;
	[[nodiscard]] std::optional<entt::entity> FindObjectUnderHand() const noexcept;
	/// `genericPickupSounds` false when the PlaceInHand path already played them at the send (GenericPickupSounds)
	void PickUp(entt::entity entity, bool genericPickupSounds = true,
	            std::optional<glm::vec3> statusPoint = std::nullopt) noexcept;
	/// The local player's interface as the one that puts things down or takes them, with the record of its last pick-up
	[[nodiscard]] pot_resource::Dropper InterfaceStatus() const noexcept
	{
		return {.hasInterface = true, .player = PlayerNames::PLAYER_ONE, .isMyInterface = true, .sourceOwner = _sourceOwner};
	}
	/// The generic pick-up's sounds (the pick-up and a villager's scream)
	void GenericPickupSounds(entt::entity entity, bool inPhysics) noexcept;
	/// HandTurn.cpp: the generic pick-up sends the PlaceInHand packet, action state 7 (WAIT FOR PLACE IN HAND)
	void SendPlaceInHand(entt::entity entity) noexcept;
	/// HandHolding.cpp: the render hand takes the object at the press: the held object and the hold parameters
	void RenderHandPickUp(entt::entity entity) noexcept;
	/// HandHolding.cpp: the render hand lets its object go (no physics; it holds nothing)
	void RenderHandRelease() noexcept;
	/// HandHolding.cpp: while the render hand holds an object the status's hand does not hold yet, its draw-only pose
	void UpdateRenderHandHeldPose() noexcept;
	/// The object the render hand holds, set at the press for a tuggable object, one or two turns before the
	/// PlaceInHand packet puts it in the status's hand (_held)
	std::optional<entt::entity> _renderHandHeld;
	/// The held object's need-sorting bit before the hand set it, put back when the render hand lets it go
	bool _renderHandHadNeedsSorting {false};
	/// HandHolding.cpp: the render hand's pick-up and hold on the object: the need-sorting bit saved and set
	void RenderHandSetHeldFlags(entt::entity entity) noexcept;
	/// The object the render hand let go last, until its ReleaseImpulse packet
	std::optional<entt::entity> _lastReleased;
	/// 180 each HOLDING frame, then down by the frame's game ms
	int32_t _releaseImpulseMs {0};
	/// The required hand position of the last HOLDING / GRAIN frame
	glm::vec3 _requiredHandPosition {0.0f};
	/// The cursor's ground point as the last HOLDING / GRAIN (or Camera) update left it
	glm::vec3 _heldGroundPoint {0.0f};
	/// The object of the render hand's state: the render hand's, else the status's
	[[nodiscard]] std::optional<entt::entity> RenderHeld() const noexcept { return _renderHandHeld ? _renderHandHeld : _held; }
	/// HandTurn.cpp: PlaceObjectInMagicHand, the PlaceInHand packet's handler; re-entered by the forest and the
	/// teleport stone
	void ApplyPlaceInHand(entt::entity object) noexcept;
	/// HandHolding.cpp: a gentle release (zero velocity) through Release
	void Drop() noexcept;
	/// HandHolding.cpp: the held object's ApplyThisToMapCoord: ThrowObjectFromHand (mobile, mobile static and single map
	/// fixed objects: 0x16); a store takes a held object only on the press (HandApplyToObject.cpp) or when its body hits
	/// it. (pending) a building site under the point taking it. Returns the result for HandleApplyResult; 0 with nothing
	/// held
	int Release(glm::vec3 velocity, std::optional<glm::vec3> mapPoint = std::nullopt, bool statusPose = false) noexcept;
	/// HandHolding.cpp: ThrowObjectFromHand(status, dont_replant): physics::from_hand::Throw with the object still in
	/// the hand; it returns 0x16 and HandleApplyResult takes it out (RemoveFirstFromHand). Release passes dont_replant
	/// 0, ForceDropHeld 1 (the ThrowHeld packet, no ApplyThisToMapCoord)
	/// With statusPose, the held object first takes the pose the ThrowData packet carried
	int ThrowObjectFromHand(glm::vec3 velocity, bool dontReplant, bool statusPose = false) noexcept;
	/// HandApplyToObject.cpp: HandleApplyResult(result, held, position) after the held object's apply
	/// (ApplyToObject, ApplyToMapCoord and ThrowHeld packets): 0x16 the hand lets it go (RemoveFirstFromHand), 0x17 it is
	/// put at the position, 0x18 only the status's hand lets it go; returns the result, 0 for 5 and a 0x16 / 0x17 that
	/// did not apply
	int HandleApplyResult(int result, entt::entity held, std::optional<glm::vec3> position) noexcept;
	/// HandTurn.cpp: the render hand's release countdown, each frame: 180 ms after it let its object go, the
	/// ReleaseImpulse packet with the hand's move since (_heldGroundPoint - _requiredHandPosition)
	void UpdateReleaseImpulse() noexcept;
	void UpdateHeldObject() noexcept;
	/// HandResources.cpp: one game turn of scooping from a pile
	bool ProcessInInteractPile() noexcept;
	/// HandTrees.cpp: a forest's InterfaceSetInMagicHand without its nested PlaceObjectInMagicHand: the wood taken and
	/// the new Conifer at the forest
	entt::entity CreateTreeFromForest(entt::entity forest) noexcept;
	/// HandFish.cpp: the splash of gripping the water (StartLandscapeGrip)
	void SplashHand(glm::vec3 point) noexcept;
	/// HandFish.cpp: the sound of gripping the land (StartLandscapeGrip, G_HandGrabLand_01..06)
	void GripLandSound(glm::vec3 point) noexcept;
	/// HandFish.cpp: the action over the water next to a fish starts catching from its farm (FishFarm locked select)
	bool TryPickUpFish(entt::entity farm, glm::vec3 point) noexcept;
	/// HandFish.cpp: FishFarm::ProcessInInteract per game turn; false if the source is not a fish farm
	bool ProcessInInteractFish() noexcept;
	/// HandFish.cpp: the action on a field starts taking its food (its network-friendly start)
	bool TryPickUpField(entt::entity field, std::optional<glm::vec3> point = std::nullopt) noexcept;
	/// HandFish.cpp: a field is valid for a locked select (growth > 0 and food > 1)
	[[nodiscard]] static bool FieldValidForLockedSelect(entt::entity field) noexcept;
	/// HandFish.cpp: FindObjectNearMapCoord's water branch: the FishFarm of a shown fish near the point
	[[nodiscard]] static std::optional<entt::entity> FishFarmUnderHand(glm::vec3 point) noexcept;
	/// HandFish.cpp: a field's ProcessInInteract per game turn; false if the source is not a field
	bool ProcessInInteractField() noexcept;
	/// Test hook OPENBLACK_TEST_SPLASH="x,z": a hand splash there every second
	void UpdateTestSplash(float seconds) noexcept;
	/// Pot::AddResourceToPos: a hand pot put down merges into a same-type pile or store nearby, else a new pile.
	void PutDownHandPot(entt::entity pot) noexcept;
	/// PileResource draw: a pile sinks into the ground as it empties (proportion of its maximum).
	void SinkPile(entt::entity pile) noexcept;
	[[nodiscard]] static PotInfo PotInfoOf(entt::entity entity) noexcept;
	[[nodiscard]] float HeldFill() const noexcept;
	void Replant(entt::entity tree) noexcept;
	/// Tree -> DeadTree. With placeLying it is laid on the ground towards direction; a physics body keeps its pose.
	void MakeDeadTree(entt::entity tree, glm::vec3 direction, bool placeLying = true) noexcept;
	/// The hand's part of the physics system: EndPhysics and ReactToPhysicsImpact of trees, pots and stores.
	void RegisterPhysicsHandlers() noexcept;
	/// The tap handlers (ecs::hand_tap) of the classes whose owners have not registered them yet: rocks, abodes, spell
	/// icons and one-shot orbs, through their public APIs
	void RegisterTapHandlers() noexcept;
	/// HandTurn.cpp: the hand's packet handlers (game_packets::SetHandler)
	void RegisterPacketHandlers() noexcept;
	/// HandTurn.cpp: the release state's ThrowData packet (the throw) and ApplyToMapCoord packet (DropOnMapCoord ->
	/// SendApplyToMapCoord)
	void SendRelease(glm::vec3 velocity) noexcept;
	/// HandSpellSeed.cpp: the ThrowHeld packet's handler (the held object's ThrowObjectFromHand(status, 1))
	void ApplyForceDropHeld() noexcept;
	/// HandTurn.cpp: the Tap packet's handler (InterfaceValidToTap again, InterfaceTap)
	void ApplyTap(entt::entity object) noexcept;
	/// HandTurn.cpp: the first part of the interface's processing: the HandAndCamera / Hand / Camera packets when the
	/// hand or the camera moved, with a throttle
	void SendHandSync() noexcept;
	/// HandTurn.cpp: the first step of the interface status's processing: the synced hand's motion this turn
	void UpdateTurnMovement() noexcept;
	/// HandTurn.cpp: a spell of the list needs continual packets
	[[nodiscard]] static bool HandCastNeedsContinualPackets() noexcept;
	/// The status's map point as a world point
	[[nodiscard]] glm::vec3 SyncMapPoint() const noexcept;
	/// HandSpellSeed.cpp: the hand's live position and the holding velocity, what a seed's ThrowData packet sends
	void LiveHandThrowData(glm::vec3& handPos, glm::vec3& velocity) const noexcept;
	/// HandSpellSeed.cpp: a seed sender's ThrowData packet, the render hand's throw block as it is
	void PushSeedThrowData(glm::vec3 velocity, glm::vec3 handPos) noexcept;
	/// The interface status's synced block (the local player's)
	map_coords::MapCoords _syncMapCoords {};      ///< From HandAndCamera / Hand
	glm::vec3 _syncHand {0.0f};                   ///< From HandAndCamera / Hand
	glm::vec3 _syncCameraPosition {0.0f};         ///< From HandAndCamera / Camera
	glm::vec3 _syncCameraFocus {0.0f};            ///< From HandAndCamera / Camera
	glm::vec3 _turnHand {0.0f};                   ///< The synced hand of this turn (UpdateTurnMovement's end)
	glm::vec3 _turnDelta {0.0f};
	glm::vec3 _stillMotion {0.0f};
	uint32_t _stillTurns {0};
	float _turnSpeed {0.0f};
	float _turnHeading {0.0f};
	float _turnRate {0.0f};
	glm::vec3 _turnVelocity {0.0f};
	float _turnSideAcceleration {0.0f};
	/// SendHandSync's state: what was sent last and the throttle's countdown
	map_coords::MapCoords _sentMapCoords {};
	glm::vec3 _sentHand {0.0f};
	glm::vec3 _sentCameraFocus {0.0f};
	glm::vec3 _sentCameraPosition {0.0f};
	uint8_t _syncCountdown {0};
	/// The action collide's MapCoords, kept while the hand points at nothing
	map_coords::MapCoords _actionCoords {};
	/// HandTurn.cpp: a locked select starts: the StartLockedSelect packet and action state 3
	void SendStartLockedSelect(entt::entity object) noexcept;
	/// HandTurn.cpp: action state 3's end: the EndLockedSelect packet, and the multi pickup stops at once
	void SendEndLockedSelect() noexcept;
	/// HandTurn.cpp: the StartLockedSelect packet's handler (the object's network-friendly start)
	void ApplyStartLockedSelect(entt::entity object) noexcept;
	/// HandTurn.cpp: the EndLockedSelect packet's handler (the object's network-friendly end)
	void ApplyEndLockedSelect(entt::entity object) noexcept;
	/// HandTurn.cpp: the interface status's locked select: the influence and the source's per-turn processing
	void ProcessLockedSelect() noexcept;
	/// HandTurn.cpp: a deleted held object leaves the hand
	void ValidateHands() noexcept;
	/// The object of the action state 3 (the locked select sent with the StartLockedSelect packet, ended with the
	/// EndLockedSelect packet)
	std::optional<entt::entity> _lockedSelectAction;
	/// The multi pickup stopped at the interface: the particles and the looping sound stop while the
	/// EndLockedSelect packet waits for its turn
	bool _lockedSelectStopped {false};
	/// The creature the hand pressed on through a locked select, and how far that is (ECS/HandCreature.h)
	std::optional<entt::entity> _creatureLock;
	hand_creature::LockState _creatureLockState;
	/// The start packet locked the creature for the hand (the object's locked-select flag)
	bool _creatureLocked {false};
	/// The render hand's creature (the CREATURE state's), and the camera ms since the hand began holding it
	std::optional<entt::entity> _creatureInHand;
	hand_creature::Interaction _creatureInteraction;
	/// Where the creature hand placed the hand this frame, in the CREATURE state
	std::optional<CreatureHandSystemInterface::HandPose> _creaturePose;
	/// HandTurn.cpp: whether an empty hand's press on the object takes hold of a creature (creature_hand::TakesPress)
	[[nodiscard]] bool CreatureTakesPress(entt::entity object) const noexcept;
	/// HandTurn.cpp: the creature's locked select, every frame: held once locked, let go at the release, then ended
	void UpdateCreatureLock(bool actionReleased) noexcept;
	/// HandTurn.cpp: the render hand with a creature, every frame (hand_creature::StepFrame): its time with it, the
	/// click and the feedback packets; returns the render hand's state from this frame
	[[nodiscard]] int32_t UpdateCreatureFrame(uint32_t cameraMs) noexcept;
	/// HandTurn.cpp: the render hand leaves the CREATURE state: how the creature was treated, the feedback packet
	void LeaveCreatureState() noexcept;
	/// HandSpellSeed.cpp: the ApplyToMapCoord packet's handler for a seed (the seed's ApplyThisToMapCoord)
	void ApplySeedToMapCoord(const game_packets::Packet& packet) noexcept;
	/// HandSpellSeed.cpp: the ApplyToObject packet's handler for a seed (the seed's ApplyThisToObject)
	void ApplySeedToObject(const game_packets::Packet& packet) noexcept;
	/// The throw velocity the ThrowData packet set
	glm::vec3 _statusThrowVelocity {0.0f};
	/// The position the ThrowData packet set (the held object's at the release, the hand's for a seed)
	glm::vec3 _statusThrowHandPosition {0.0f};
	/// The throw's angular velocity, the angular momentum the ThrowData packet set, always in this port's sign
	/// (PredictRelease's L; a seed's raw h is sent negated, the original's sign being the opposite)
	glm::vec3 _statusThrowAngular {0.0f};
	/// HandAngles (x, y, z), the DecomposeYXZ angles the ThrowData packet set
	glm::vec3 _statusThrowAngles {0.0f};
	/// The hand's angular velocity h (read by the release state and the apply senders). Its writers: 0 at
	/// construction, 0 every frame while the Holding spring works, the grain state's update in state 8; (not ported) the
	/// replay's restore
	glm::vec3 _handAngularVelocity {0.0f};
	/// The grain state's smoothers (state 8, a held spell seed): the last displacement, the smoothed velocity and the
	/// smoothed sideways acceleration; reset when the state is entered
	glm::vec3 _grainDisplacement {0.0f};
	glm::vec3 _grainVelocity {0.0f};
	float _grainSideAcceleration {0.0f};
	bool _grainEntered {false};
	/// HandHolding.cpp: the throw block's per-frame writers (W2 / W3), at the end of Place with the hand's position
	/// before and after this frame's move (the grain update's old, then the Holding move, then new)
	void UpdateThrowBlock(glm::vec3 oldHand, glm::vec3 newHand) noexcept;
	/// SendHandSync's ping ring: the tick count at each own HandAndCamera / Hand / Camera send (32 entries); when an
	/// own sync packet comes back, now - ring[index] is the local interface's ping, 0 until the first one. The release
	/// state reads it
	std::array<uint32_t, 32> _pingRing {};
	uint8_t _pingIndex {0};
	uint32_t _pingMs {0};
	/// SendHandSync and SendApplyToMapCoord: the ring entry of a sync packet
	[[nodiscard]] int32_t NextPingIndex() noexcept;
	/// The ping of an own sync packet as it is processed
	void ReceivePing(int32_t index) noexcept;
	/// HandTurn.cpp: the ThrowData packet of a release (the release state) or of ForceDropHeld: the velocity, and the
	/// held object's position and rotation at the send
	void PushThrowData(glm::vec3 velocity, bool predict) noexcept;
	/// HandTurn.cpp: IsInteractable (IsAvailable; an abode: built > 0)
	[[nodiscard]] static bool Interactable(entt::entity object) noexcept;
	/// HandToolTips.cpp: the interface's hand state for the hand's state now
	[[nodiscard]] int32_t InterfaceHandState() const noexcept;
	/// HandToolTips.cpp: the tooltip of the hand state, once per turn
	void SubmitToolTips() noexcept;
	/// HandToolTips.cpp: the land tooltips
	void SubmitLandToolTips() const noexcept;
	/// HandApplyToObject.cpp: the held object's ValidToApplyThisToObject on the target
	[[nodiscard]] bool HeldValidToApplyTo(entt::entity target) const noexcept;
	/// The interface's in-influence flag: the action position inside the player's influence
	[[nodiscard]] bool InInfluence() const noexcept;
	/// ValidForPlaceInHand for the hovered classes: false for rocks too big to lift and tap-only spell icons
	[[nodiscard]] bool ValidForPlaceInHand(entt::entity object) const noexcept;
	/// SendTap: in the influence (or not InterfaceMustBeInInfluenceForInteraction), the class valid to tap and not
	/// IsCannotBePickedUp -> the Tap packet -> ApplyTap -> InterfaceTap. True when it tapped.
	bool SendTap(entt::entity object) noexcept;
	/// DeleteObjectAndTakeResource: the store takes the tree's wood and the tree is deleted.
	void DepositInStore(entt::entity object, entt::entity store, const pot_resource::Dropper& is) noexcept;
	[[nodiscard]] bool IsHoldingTree() const noexcept;

	// ---- HandSpellSeed.cpp: a spell seed in the hand (the interface's apply states, the seed's interface calls) ----
	/// The interface's action states with a seed: 8/9 apply on release (to the land / an object), 10/11 locked apply
	enum class SeedAction : uint8_t
	{
		None = 0,
		ApplyOnReleaseMap = 8,
		ApplyOnReleaseObject = 9,
		LockedApplyMap = 10,
		LockedApplyObject = 11,
	};
	[[nodiscard]] bool IsHoldingSeed() const noexcept;
	/// ActionPressedHolding, the seed branches
	void SeedActionPressed() noexcept;
	/// States 8..11 every frame: release applies / unlocks
	void UpdateSeedAction(bool actionHeld) noexcept;
	/// BeginApplyOnRelease / EndApplyOnRelease: the buffer reseeded, the G_HANDGESTURE_02 loop
	void BeginApplyOnRelease(SeedAction state) noexcept;
	void EndApplyOnRelease() noexcept;
	/// SendApplyToMapCoord (the seed part) -> the ApplyToMapCoord packet -> the seed's ApplyThisToMapCoord
	int SendSeedApplyToMapCoord() noexcept;
	/// SendApplyToObject (the seed part) -> the ApplyToObject packet -> the seed's ApplyThisToObject
	int SendSeedApplyToObject() noexcept;
	/// FailApply: SPOT_VISUAL 4 (SF_FailedApply) at the point and G_SpellCastFailure
	int FailApply(glm::vec3 point) noexcept;
	/// HandleApplyResult: 0x16 -> the seed leaves the hand, 3 -> consumed
	void HandleSeedApplyResult(int result, entt::entity seed) noexcept;
	/// Every frame: the hold parameters (MAGIC until ready), the seed's own mesh (IsG3DObjectDrawnInHand), its coming in
	/// and out of the hand, and what the gesture system is told (magic::gestures::SetHandStatus)
	void UpdateSeedInHand(bool actionHeld) noexcept;
	/// The seed's InterfaceSetOutMagicHand; the hand stops drawing it (out of the hand, a seed that follows its spell is
	/// drawn by magic::seed::DrawSpells)
	void SeedLeftHand(entt::entity seed) noexcept;
	/// Test hooks OPENBLACK_TEST_CAST / OPENBLACK_TEST_CAST_PATH / OPENBLACK_TEST_THROW_VEL (HandSpellSeed.cpp)
	bool TestCastActionHeld(float seconds, bool actionHeld) noexcept;
	[[nodiscard]] std::optional<glm::vec3> TestCastPathPoint() const noexcept;

	// ---- HandApplyToObject.cpp: the held object applied to the object under the hand, and seeds / stones picked up ----
	/// The seed an object out of the hand gives the hand when it is ValidForPlaceInHand: a spell seed itself, a
	/// teleport stone its spell's seed; entt::null otherwise
	[[nodiscard]] static entt::entity SeedToPlaceInHand(entt::entity object) noexcept;
	/// The generic pick-up (the 225 ms grab) of a seed or a stone: the checks, sound 10 and the PlaceInHand packet with
	/// the object (a stone's seed is found in the handler). False if the object is neither.
	bool SendSeedOrStonePickup(entt::entity object, bool inInfluence) noexcept;
	/// ActionPressedHolding for a held object that is not a seed: when the object under the hand takes it
	/// (ValidToApplyThisToObject, with the influence rule), SendApplyToObject -> the ApplyToObject packet ->
	/// ApplyHeldToObject -> ApplyThisToObject -> HandleApplyResult. False: nothing applied (the press arms the
	/// put down / throw as before)
	bool HeldActionPressedOnObject(bool inInfluence) noexcept;
	/// HandApplyToObject.cpp: the ApplyToObject packet's handler (the checks again, ApplyThisToObject and
	/// HandleApplyResult)
	void ApplyHeldToObject(entt::entity target) noexcept;
	/// RemoveFirstFromHand: the held object leaves the hand without physics (the magic hand's RemoveFromHand:
	/// fire::SetOutMagicHand); the caller places it
	std::optional<entt::entity> RemoveFirstFromHand() noexcept;

	SeedAction _seedAction {SeedAction::None};
	/// the object under the cursor when the apply started (the action collide's object)
	entt::entity _seedTarget {entt::null};
	/// The turn an apply packet (ApplyToObject / ApplyToMapCoord) was sent, 0 when none waits; no other is sent while it
	/// is set
	uint32_t _applySentTurn {0};
	/// The turn the ApplyToObject / ApplyToMapCoord handler ran. Both go back to 0 in SendHandSync once the sent turn
	/// <= the handled turn, and in EndAction
	uint32_t _applyHandledTurn {0};
	/// the seed the hand held last frame (to see it come and go)
	entt::entity _seedInHand {entt::null};
	float _testCastTime {-1.0f};
	/// The original's HOLD_TYPE: what GetHoldType returns for each class.
	enum class HoldType
	{
		None = 0,
		Above = 1,
		Magic = 2,
		Grain = 3,
		Fingers = 4,
		Tree = 5,
		Side = 6,
		Villager = 7,
	};
	/// GetHoldType / GetHoldRadius / GetHoldLoweringMultiplier / GetHeight / IsARootedObject of an entity.
	void ComputeHoldParameters(entt::entity entity) noexcept;
	/// The tug state: a grounded tree is pulled until the hand is weight / 1000 away from the grab point.
	void UpdateTug(float seconds, bool actionHeld) noexcept;
	void Uproot(entt::entity tree) noexcept;
	/// Centre of the closed side grip (between palm and fingertips) in hand model space, for the current pose.
	[[nodiscard]] glm::vec3 GripCentre() const noexcept;
	/// The holding state's mouse sway: rotation of the held object's up about the view and side axes.
	[[nodiscard]] glm::mat3 HeldSway(glm::vec3 at, float extraRoll = 0.0f) const noexcept;
	/// A tree out of the map draws MSH_T_ROOTS at the tree matrix.
	void UpdateRoots(entt::entity tree, bool dying = false) noexcept;
	void DropRoots(entt::entity tree, bool fall) noexcept;
	void UpdateRootsAndPiles(float seconds) noexcept;
	void EmitGripDust(glm::vec3 point) noexcept;
	void UpdateGripDust(float seconds) noexcept;
	/// Environment-variable test hooks (HandDebugHooks.cpp), run once when the landscape exists.
	void RunDebugHooks() noexcept;
	void UpdateTestAbode(float seconds) noexcept;
	struct TestAbode
	{
		entt::entity abode;
		float speed;
		float scale;
		int count;
		float timer;
	};
	std::optional<TestAbode> _testAbode;
	float _testActionDelay {0.0f};
	float _testActionHold {0.0f};
	/// OPENBLACK_HAND_TEST_DROP: where (x, z) and in how many seconds the held object is put down
	std::optional<glm::vec3> _testDropAt;
	float _testDropIn {0.0f};
	float _testMouseMoveIn {-1.0f};
	void UpdatePickupParticles(float seconds, bool emitting) noexcept;
	[[nodiscard]] glm::vec3 ModelPosition(size_t vertex, const std::vector<glm::mat4>& bones) const noexcept;
	/// HandNearObject.cpp: the land behind the hand seen from the camera (found by the landscape draw)
	void UpdatePointBehindHand() noexcept;
	/// HandNearObject.cpp: FindObjectNearMapCoord for the action's point `at`
	[[nodiscard]] std::optional<entt::entity> FindObjectNearMapCoord(glm::vec3 at) const noexcept;
	/// HandMorph.cpp: the morph's base load: Base2, Evil2 and Good2 on the CPU
	void LoadMorphMeshes() noexcept;
	/// HandMorph.cpp: the render hand's preparation for drawing: the texture set, the alignment, the morph update
	void UpdateMorphing() noexcept;
	/// HandMorph.cpp: the morphed texture (a 4444 blend) into _morphTexels
	void MorphTexture() noexcept;
	/// HandMorph.cpp: the morphed vertices into _morphVertices
	void MorphVertices() noexcept;
	/// HandMorph.cpp: the morph's only GPU upload (the hand mesh's skin and sub-meshes)
	void UploadMorph(bool texture, bool vertices) noexcept;
	/// HandFrame.cpp: the hand matrix's rotation (d from the mouse ray, side = d x up; X side, Y side x up, Z -up)
	[[nodiscard]] glm::mat3 HandMatrixRotation(glm::vec3 up) noexcept;
	/// HandFrame.cpp: the empty hand's up (the land normal, three 0.4 s Zoomers)
	glm::vec3 UpdateNormalUp(float seconds) noexcept;

	std::array<entt::entity, 2> _hands;
	std::unique_ptr<HandAnimator> _animator;
	std::string _override;
	/// The play-anim state: the flag StartFixedPosAnimation sets, its clip and point, and the ms since it was entered
	bool _playAnim {false};
	std::string _playAnimClip;
	glm::vec3 _playAnimPoint {0.0f};
	uint32_t _playAnimMs {0};
	// Smoothed directional motion for the L*_lr / L*_fb layers (-1..+1).
	glm::vec2 _motionTarget {0.0f};
	glm::vec2 _motion {0.0f};
	float _motionAge {1.0f};

	// Hand geometry from Hand_Boned_Base2.l3d (vertices live in the space of their bone).
	std::vector<glm::vec3> _vertices;
	std::vector<uint32_t> _vertexBones;
	glm::vec3 _handHeadingBack {0.0f, 0.0f, 1.0f}; ///< HandMatrixRotation's d (H)
	/// HOLDING / GRAIN: the held object's up' (the sway, R1 R2) for its own matrix; nullopt in the other states (the
	/// held object then takes the hand's)
	std::optional<glm::vec3> _heldUp;
	openblack::Zoomer3 _up; ///< The up Zoomers
	glm::vec3 _normalUp {0.0f, 1.0f, 0.0f};
	float _upMouseX {-1e30f}; ///< the mouse's x when the up last took a target
	bool _inNormalState {false};
	/// The rest of the preparation for drawing is skipped: the INVISIBLE state (which also hides: NotDrawn, the render
	/// hand hidden) or the interface inactive
	bool _handHidden {false};
	/// The render hand's state (HAND_STATES: 0 INVISIBLE .. 0xA CITADEL), from RequiredHandState each frame
	int32_t _renderHandState {1};
	/// The hand's place when its state changed (the last hand matrix's), what the 0.13 s blend starts from
	glm::vec3 _stateBlendFromPosition {0.0f};
	/// The render hand's transformed matrices (world space) as last drawn, and their copy at the state change
	std::vector<glm::mat4> _handWorld;
	std::vector<glm::mat4> _stateBlendFromWorld;
	/// HandPlacement.cpp: the state blend in world space (draw data)
	void BlendHandWorld(const glm::mat4& handModel) noexcept;
	/// HandSystem.cpp: the render hand's required state
	[[nodiscard]] int32_t RequiredHandState() const noexcept;
	float _handReach {1800.0f};
	/// HandClicked.cpp: once an interface tick before the action states
	void UpdateTapMemory(bool actionReleased) noexcept;
	entt::entity _clickedObject {entt::null};
	uint32_t _clickedTurn {0};
	glm::vec3 _clickedPosition {0.0f}; ///< Zeroed when cleared
	uint32_t _clickedPositionTurn {0};
	std::optional<glm::vec3> _collidePoint; ///< The action collide's position: the land or object hit
	// the good / evil morph (Morphable): meshes [1] base, [2] evil, [3] good on the CPU, the blend results
	std::unique_ptr<l3d::L3DFile> _morphBase;
	std::unique_ptr<l3d::L3DFile> _morphEvil;
	std::unique_ptr<l3d::L3DFile> _morphGood;
	std::vector<uint16_t> _morphTexels;
	std::vector<l3d::L3DVertex> _morphVertices;
	float _morphApplied {0.0f};                ///< Zeroed when the morph is initialised
	int32_t _morphTextureSet {1};              ///< 1 at construction and when the map is cleared
	std::optional<glm::vec3> _pointBehindHand; ///< Valid while the landscape draw found one
	/// Palm centre (bind pose, model space): held objects sit under it.
	glm::vec3 _palmCenter {0.0f};

	std::optional<entt::entity> _hovered;
	/// The interface's screen-object collide: the pending offer and this frame's; _screenGrip is action state 17
	/// BUBBLE GRIP (the collide frozen)
	std::optional<uint32_t> _screenOffer;
	float _screenOfferDepth {0.0f};
	std::optional<uint32_t> _screenObject;
	float _screenObjectDepth {0.0f};
	bool _screenGrip {false};
	/// The interface collide's distances of the frame's cursor hits (pending: the object's, the land's)
	std::optional<float> _cursorObjectDistance;
	std::optional<float> _cursorLandDistance;
	std::optional<entt::entity> _held;
	/// The object UpdateHeldObject gave a HandDrawPose, to take it off when it leaves the hand
	std::optional<entt::entity> _heldPosed;
	/// Object under the hand while the action button is held, in action state 13 (the grab state)
	std::optional<entt::entity> _pendingPick;
	/// The grab's start: the frame's engine sample time and the turn
	uint32_t _pendingPressMs {0};
	uint32_t _pendingPressTurn {0};
	/// The press's object is free (IN_PHYSICS) or a forest: the 225 ms timer path
	bool _pendingTimer {false};
	/// The tug-like hold of a tuggable object at rest, cleared by the tug state's update; 0 at once for a seed
	bool _pendingTugHold {false};
	/// The tug state's first frame and the state blend timer (s), from the TUG state change
	bool _pendingTugFirst {false};
	float _pendingTugBlend {0.0f};
	bool _pendingTugStarted {false};
	/// HOLD_TYPE_TREE: height of the held tree and its hold radius (0.2 * 2D radius).
	float _heldHeight {0.0f};
	float _holdRadius {0.0f};
	HoldType _holdType {HoldType::None};
	float _loweringMultiplier {0.0f};
	bool _rooted {false};
	/// The holding state: the hand follows its required position with a spring in 10 ms steps.
	bool _springActive {false};
	/// The spring's clock (game ms of every holding frame) and its 10 ms steps, the original's two counters
	uint32_t _springClockMs {0};
	uint32_t _springStepMs {0};
	glm::vec3 _springVelocity {0.0f};
	/// The render hand's mouse smoothing, in 1024-wide reference pixels: drives the held object sway.
	glm::vec2 _mouse {0.0f};
	glm::vec2 _smoothMouse {0.0f};
	glm::vec2 _smoothMouseVelocity {0.0f};
	bool _smoothMouseValid {false};
	/// The hand scale from its distance from the view (1 between 10 and 150 units from the camera).
	float _handScale {1.0f};
	/// Rotation of the held object when it was picked up (the sway is applied on top of it).
	glm::mat3 _heldRotation {1.0f};
	/// Tree being tugged out of the ground, the point where it was grabbed and its planted rotation.
	std::optional<entt::entity> _tug;
	glm::vec3 _tugPoint {0.0f};    ///< The tug anchor: the tree's base, the pivot
	glm::mat3 _tugRotation {1.0f}; ///< as it stood, for a tug let go
	glm::vec3 _tugGrab {0.0f};     ///< where the hand took hold of it
	float _tugDepth {0.0f};        ///< that point's distance along the mouse ray
	glm::vec3 _mouseRayOrigin {0.0f};
	glm::vec3 _mouseRayDirection {0.0f, 0.0f, 1.0f};
	void BeginTug(entt::entity tree) noexcept;
	/// Roots drawn under trees out of the map (tree -> roots entity).
	std::vector<std::pair<entt::entity, entt::entity>> _roots;
	struct FallingRoots
	{
		entt::entity entity;
		float age;
		float startY;
		float groundY;
	};
	std::vector<FallingRoots> _fallingRoots;
	float _heldAltitude {0.0f};
	float _heldTop {0.0f};
	bool _actionWasHeld {false};

	/// Resource being gathered into the hand pile (HandWood / HandFood) while the action is held over it.
	/// The locked select's object (set by the StartLockedSelect handler, cleared by the EndLockedSelect one or when it
	/// ends)
	std::optional<entt::entity> _pickSource;
	/// The player who owned what the hand last picked up: every object put in the hand records the most influential
	/// player where it was; a locked select's start then records its source's player (a pile's, the most influential at
	/// a field, a fish farm's town's owner or none). None until the first pick-up
	std::optional<PlayerNames> _sourceOwner;
	/// Game turns since the locked select started.
	uint32_t _pickTurns {0};
	/// Hand x,z frozen over the pile while scooping (the holding state, locked interact): only the hand's draw;
	/// the influence check reads the synced turn hand (_turnHand). (inferred) the live point under the hand when the
	/// StartLockedSelect packet is applied: the original's lock point was not checked
	glm::vec3 _pickLock {0.0f};
	/// The press that picked the object up is still held: its release does not drop it (state 7).
	bool _pickPressHeld {false};
	/// The locked select is catching fish (_pickSource is a FishFarm)
	bool _pickFish {false};
	/// The locked select is taking food from a field (_pickSource is a Field)
	bool _pickField {false};
	/// Test hook (OPENBLACK_HAND_TEST_FISH): seconds the action button counts as held
	float _testActionSeconds {0.0f};
	/// The multi pick-up's looping G_PICKUPFOOD / G_PICKUPWOOD and its pitch t^2
	std::optional<audio::Channel> _pickupSound;
	uint32_t _toolTipTurn {0};       ///< the game turn the tooltips were last processed
	bool _toolTipScooping {false};   ///< a locked select was scooping last turn (its end forces 0xEEA once more)
	int32_t _interfaceHandState {3}; ///< The interface's hand state (3 when it is reset)
	float _pickupSoundFraction {0.0f};
	/// HandEffects.cpp: starts / re-pitches / stops the multi pick-up loop
	void UpdatePickupSound(bool active) noexcept;
	/// A later press while holding: its release drops / throws (state 12).
	bool _releaseArmed {false};
	/// Hand velocity (world units/s), for throwing on release.
	glm::vec3 _handVelocity {0.0f};
	std::optional<glm::vec3> _lastHeldPosition;
	float _lastDt {0.0f};

	/// SF_GripLandscape particles (dust thrown up when the land is gripped).
	struct DustParticle
	{
		entt::entity entity;
		float age;
		float frame; ///< The atom's frame, from RandomiseInitFrame (frame_anim::ParticleFrameAdvance)
	};
	std::vector<DustParticle> _dust;

	/// SF_MultiPickUpWood / SF_MultiPickUpFood atoms (ER_MultiPickup): pieces rising from the pile into the hand.
	struct PickupParticle
	{
		entt::entity entity;
		float age;
		glm::vec3 start;
		glm::vec3 previous;
		bool mesh;
		float frame {0.0f};     ///< The atom's frame: InitFrame 0, or RandomiseInitFrame (fish)
		float frameRate {0.0f}; ///< FrameRate, negated by RandomiseFrameDirection (fish)
	};
	std::vector<PickupParticle> _pickupParticles;
	/// ER_MultiPickup collection data: atoms owed (EmitRate * time) and atoms emitted.
	float _pickupOwed {0.0f};
	uint32_t _pickupEmitted {0};

	std::optional<glm::vec3> _interactionPoint;
	/// The interface's action collide: the object under the cursor this frame (exact triangle pick).
	std::optional<entt::entity> _cursorObject;
	struct CursorHit
	{
		entt::entity entity;
		float t;
		glm::vec3 boxMin;
		glm::vec3 boxMax;
		glm::vec3 normal {0.0f, 1.0f, 0.0f}; ///< the hit face's world normal
	};
	/// The up target over an object the hand feels, 0.25 d - n + (0, 0.5, 0); nullopt: the land's
	std::optional<glm::vec3> _feelUp;
	/// Nearest object whose triangles the ray origin + t * direction (unit) hits.
	[[nodiscard]] std::optional<CursorHit> PickObjectAlongRay(const glm::vec3& origin,
	                                                          const glm::vec3& direction) const noexcept;

	/// The hand's distance from the camera along the mouse ray.
	openblack::Zoomer _handDistance;
	/// Entering the normal state resets the zoomer to the current distance from view.
	bool _handDistanceValid {false};
	std::optional<glm::vec3> _gripPoint;
	std::optional<glm::vec3> _smoothedPosition;
};
} // namespace openblack::ecs::systems
