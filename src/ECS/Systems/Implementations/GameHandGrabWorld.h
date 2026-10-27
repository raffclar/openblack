/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "Hand/HandGrabWorld.h"

namespace openblack::ecs::systems
{

/// The god hand's world in the game: the things on the map and the systems a pick-up or a release reaches
class GameHandGrabWorld final: public hand_grab::HandGrabWorldInterface
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] entt::entity Hand() const override;
	[[nodiscard]] PlayerNames HandPlayer() const override;
	void SetActingPlayer(std::optional<PlayerNames> player) override { _acting = player; }
	[[nodiscard]] bool IsOtherPlayersHand(entt::entity hand) const override;
	[[nodiscard]] glm::vec3 AsMapPlace(glm::vec3 point) const override;
	[[nodiscard]] std::optional<glm::vec3> ReportedCollidePoint() const override;
	[[nodiscard]] std::optional<PlayerNames> GetActingPlayer() const override { return _acting; }
	[[nodiscard]] std::optional<PlayerNames> OwnHandPlayer() const override;
	[[nodiscard]] bool IsNetworkGame() const override;

	[[nodiscard]] std::optional<entt::entity> ObjectUnderCursor() const override;
	[[nodiscard]] std::optional<entt::entity> ObjectNearHand() const override;
	[[nodiscard]] std::optional<glm::vec3> PickedPoint() const override;
	[[nodiscard]] bool InInfluence(PlayerNames player, glm::vec3 point) const override;
	[[nodiscard]] bool HandInInfluence(PlayerNames player, glm::vec3 hand) const override;
	void HeldThingUsedOnLand(PlayerNames player) override;
	[[nodiscard]] bool InBounds(glm::vec3 point) const override;
	[[nodiscard]] glm::vec3 LandNormalAt(glm::vec3 point) const override;

	[[nodiscard]] Size SizeOf(entt::entity object) const override;
	[[nodiscard]] float WeightOf(entt::entity object) const override;
	[[nodiscard]] float LifeOf(entt::entity object) const override;
	[[nodiscard]] bool IsFlying(entt::entity object) const override;
	[[nodiscard]] bool SpeciesAllowsPickUp(entt::entity animal) const override;
	[[nodiscard]] MobileStaticInfo StaticKindOf(MobileStaticInfo type) const override;
	[[nodiscard]] MeshId StaticMeshOf(MobileStaticInfo type) const override;
	[[nodiscard]] bool IsLoosePot(PotInfo type) const override;
	[[nodiscard]] bool IsOfRockMaterial(entt::entity object) const override;
	[[nodiscard]] bool IsSexuallyActive(entt::entity villager) const override;
	[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity object) const override;

	void LeavePhysicsAndMap(entt::entity object) override;
	void CreateReaction(const ReactionRequest& request) override;
	void RemoveReactions(entt::entity initiator, Reaction type) override;
	void FireStartedMoving(entt::entity object, bool inHand) override;
	void CatchFirefly(entt::entity object) override;
	void CountHelpEvent(uint32_t event) override;
	void ForgetGesturePath() override;
	void HeatHeld(entt::entity object) override;
	void HeldTurn(entt::entity object) override;
	void PlaySample(uint32_t sample, glm::vec3 position) override;
	bool TapThing(entt::entity object, glm::vec3 handPoint, PlayerNames player) override;
	void GlobeIntoHand(entt::entity globe, PlayerNames player) override;
	void ScaffoldIntoHand(entt::entity scaffold, PlayerNames player) override;
	[[nodiscard]] std::optional<creature_giving::Verdict> GiveVerdict(entt::entity target, entt::entity held) const override;
	void SayCantPickUp(entt::entity creature) override;
	bool GiveTo(entt::entity creature, entt::entity held, bool leftHand) override;
	[[nodiscard]] std::optional<Taking> TakingOf(entt::entity creature) const override;
	[[nodiscard]] std::optional<glm::vec3> TakePointOf(entt::entity creature, glm::vec3 hand) const override;
	[[nodiscard]] bool StillTaking(entt::entity creature) const override;
	[[nodiscard]] float CreatureSizeOf(entt::entity creature) const override;
	void CallOffGiving(entt::entity creature) override;
	[[nodiscard]] bool RightHanded() const override;
	[[nodiscard]] bool HoldsLooseLeash() const override;
	bool LeashTap(entt::entity object) override;
	/// Whether the player's leash takes a tap on the thing as an order to the creature, in place of the thing's own tap
	[[nodiscard]] static bool LeashTakesTap(entt::entity object, PlayerNames player);
	[[nodiscard]] uint32_t LocalRandom(uint32_t count) override;
	void SendCommand(player_commands::Action action) override;
	void VillagerIntoHand(entt::entity villager) override;
	void AnimalIntoOwnFlock(entt::entity animal) override;
	void ArtefactTaken(entt::entity object, PlayerNames player) override;
	void TreeUprooted(PlayerNames player, entt::entity tree) override;
	std::optional<entt::entity> TreeFromBigForest(entt::entity bigForest) override;
	void LeaveRootsHole(entt::entity tree, const glm::vec3& place, const glm::mat3& rotation, float scale) override;
	void RemakeDrawList(uint32_t frames) override;
	void RemovePotReaction(entt::entity pot) override;
	void SetUpPotReaction(entt::entity pot, PlayerNames player) override;
	[[nodiscard]] Pose PoseOf(entt::entity object) const override;
	[[nodiscard]] glm::vec3 ReportedHandPlace() const override;
	[[nodiscard]] glm::vec3 ReportedPlaceOf(entt::entity hand) const override;
	[[nodiscard]] std::vector<PlayerNames> OtherHandPlayers() const override;
	void SetPose(entt::entity object, const Pose& pose) override;
	void DrawPulledPose(entt::entity object, const Pose& pose) override;
	void ForgetPulledPose(entt::entity object) override;
	[[nodiscard]] glm::mat3 UnstretchedAxes(entt::entity object, const glm::mat3& axes) const override;

	[[nodiscard]] player_commands::ThrowStart ThrowStartOf(entt::entity object, bool alignToSlope) override;
	void StartThrowFrom(entt::entity object, const player_commands::ThrowStart& start) override;
	std::optional<uint32_t> PourPot(entt::entity pot, PlayerNames player) override;
	[[nodiscard]] std::optional<PotFacts> PotFactsOf(entt::entity pot) const override;
	[[nodiscard]] hand_grab::ScoopFacts ScoopFactsOf(PotInfo handful) const override;
	uint32_t TakeFromPile(entt::entity pile, uint32_t amount) override;
	[[nodiscard]] std::optional<FieldFacts> FieldFactsOf(entt::entity field) const override;
	void TakeFromField(entt::entity field, uint32_t amount) override;
	void NoteTakenAt(glm::vec3 point) override;
	void NoteTakenFromOwnerOf(entt::entity thing) override;
	[[nodiscard]] std::optional<fish_farm::Type> FishFarmOf(entt::entity farm) const override;
	uint32_t TakeFromFishFarm(entt::entity farm, uint32_t amount) override;
	void ResizePot(entt::entity pot) override;
	[[nodiscard]] entt::entity MakeHandful(PotInfo type, glm::vec3 position, uint32_t amount, bool poisoned) override;
	[[nodiscard]] std::optional<uint32_t> StartScoopStream(ResourceType resource, glm::vec3 source, bool poisoned) override;
	[[nodiscard]] std::optional<uint32_t> StartFishScoopStream(glm::vec3 source) override;
	void StopScoopStream(uint32_t stream) override;
	void PinCursor(bool pinned) override;
	void MoveScoopStream(uint32_t stream, glm::vec3 hand) override;
	[[nodiscard]] entt::entity PlayScoopSound(entt::entity playing, ResourceType resource, glm::vec3 hand, float ramp) override;
	void StopScoopSound() override;
	[[nodiscard]] float LandHeightAt(glm::vec3 point) const override;
	[[nodiscard]] bool StoresResource(entt::entity store, ResourceType resource) const override;
	uint32_t AddToStore(entt::entity store, ResourceType resource, uint32_t amount, bool poisoned) override;
	bool TakeIntoStore(entt::entity store, entt::entity object) override;
	[[nodiscard]] bool TakesWhole(entt::entity target, entt::entity held) const override;
	bool LayGateStone(entt::entity plinth, entt::entity stone) override;
	[[nodiscard]] bool JoinsScaffold(entt::entity target, entt::entity held) const override;
	void JoinScaffold(entt::entity target, entt::entity held) override;
	void PourAt(ResourceType resource, glm::vec3 point, uint32_t amount, PlayerNames player, bool poisoned) override;
	void UseUp(entt::entity object) override;
	altar_offering::Answer OfferAtAltar(entt::entity altar, entt::entity held) override;
	void HelpEvent(uint32_t event) override;
	FromHandResult LetGoFromHand(entt::entity object, const FromHand& release) override;
	[[nodiscard]] bool PlayerHasNoWindResistance(PlayerNames player) const override;
	[[nodiscard]] std::optional<BodyFacts> BodyOf(entt::entity object) const override;
	void TwistBody(entt::entity object, glm::vec3 torque) override;
	void DropDrag(entt::entity object) override;

private:
	/// Where the resource in the local player's hand came from
	void SetHandTookFrom(PlayerNames from);

	/// The player of the command that the hand does now, none outside the commands
	std::optional<PlayerNames> _acting;
};

} // namespace openblack::ecs::systems
