/******************************************************************************
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
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"

/// Recording fakes of the creature systems and the hand, for the tests of the systems that call them. The tests put
/// them in the locator; they are never read back through it.
namespace openblack::test::creature_fakes
{
using namespace openblack::ecs::systems;

/// The player's hands: the left one's entity and where it is
class FakeHand final: public HandSystemInterface
{
public:
	entt::entity left {entt::null};
	std::optional<glm::vec3> position;

	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override
	{
		return {left, entt::null};
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override
	{
		return {position, std::nullopt};
	}
	void Place(std::optional<glm::vec3>, glm::vec3, bool, std::chrono::microseconds) noexcept override {}
	[[nodiscard]] std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3&, const glm::vec3&, std::optional<glm::vec3>,
	                                                          bool, std::chrono::microseconds) noexcept override
	{
		return std::nullopt;
	}
	void Update(std::chrono::microseconds, glm::vec2, bool, bool) noexcept override {}
	[[nodiscard]] std::optional<entt::entity> GetHeldObject() const noexcept override { return std::nullopt; }
	void PlaceObjectInMagicHand(entt::entity) noexcept override {}
	[[nodiscard]] std::optional<PlayerNames> GetSourceOwner() const noexcept override { return std::nullopt; }
	[[nodiscard]] bool IsHandReadyForObject() const noexcept override { return false; }
	void ForceDropHeld() noexcept override {}
	void EndAction() noexcept override {}
	void GetSpellInfo(glm::vec3&, glm::vec3&, glm::vec3&, glm::vec3&) const noexcept override {}
	[[nodiscard]] float GetHandScale() const noexcept override { return 1.0f; }
	[[nodiscard]] glm::mat4 GetHandMatrix() const noexcept override { return glm::mat4(1.0f); }
	void SetHandReach(float) noexcept override {}
	[[nodiscard]] float GetHandReach() const noexcept override { return 0.0f; }
	[[nodiscard]] entt::entity GetClickedObject() const noexcept override { return entt::null; }
	void ClearClicked() noexcept override {}
	void RememberTapped(entt::entity) noexcept override {}
	[[nodiscard]] bool PositionClicked(const glm::vec3&, float) const noexcept override { return false; }
	void ClearClickedPosition() noexcept override {}
	void ProcessTurn() noexcept override {}
	void GameTurnUpdate() noexcept override {}
	[[nodiscard]] std::optional<entt::entity> GetRenderHandObject() const noexcept override { return std::nullopt; }
	void SetThrowBlock(const std::array<float, 12>&) noexcept override {}
	void ResetTurnState() noexcept override {}
	[[nodiscard]] glm::vec3 GetTurnHandVelocity() const noexcept override { return glm::vec3(0.0f); }
	[[nodiscard]] int32_t GetInterfaceHandState() const noexcept override { return 0; }
	void UpdateInterfaceHandState() noexcept override {}
	[[nodiscard]] bool OfferScreenObject(uint32_t, float) noexcept override { return false; }
	[[nodiscard]] std::optional<uint32_t> ScreenObjectUnderHand() const noexcept override { return std::nullopt; }
	[[nodiscard]] std::optional<uint32_t> DraggedScreenObject() const noexcept override { return std::nullopt; }
	[[nodiscard]] std::vector<entt::entity> GetThrownObjects() const noexcept override { return {}; }
	[[nodiscard]] const std::vector<glm::mat4>* GetBoneMatrices() const noexcept override { return nullptr; }
	[[nodiscard]] std::vector<std::string> GetAnimationNames() const noexcept override { return {}; }
	[[nodiscard]] const std::string& GetCurrentAnimation() const noexcept override { return _animation; }
	void SetAnimationOverride(const std::string&) noexcept override {}
	void StartFixedPosAnimation(const std::string&, glm::vec3) noexcept override {}

private:
	std::string _animation;
};

/// Every animation lasts as long as set (nothing when unset), and a bone is where it is set to be
class FakeAnimation final: public CreatureAnimationSystemInterface
{
public:
	std::map<size_t, float> durations;
	std::optional<glm::vec3> bone;

	void ProcessTurn() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return bone;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t animation) override
	{
		const auto found = durations.find(animation);
		return found != durations.end() ? std::optional(found->second) : std::nullopt;
	}
};

/// The moves asked for
class FakeLocomotion final: public CreatureLocomotionSystemInterface
{
public:
	struct Asked
	{
		entt::entity creature;
		glm::vec2 point;
		float value;
		bool lead;
	};
	std::vector<Asked> moves;
	std::vector<entt::entity> stopped;
	bool moving {false};

	void ProcessTurn() override {}
	void Update(float) override {}
	MoveResult MoveTo(entt::entity creature, glm::vec2 point, Pace, float, float maxDistance) override
	{
		moves.push_back({.creature = creature, .point = point, .value = maxDistance, .lead = false});
		return MoveResult::Started;
	}
	MoveResult LeadTo(entt::entity creature, glm::vec2 point, float pull, float) override
	{
		moves.push_back({.creature = creature, .point = point, .value = pull, .lead = true});
		return MoveResult::Started;
	}
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Started; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Started; }
	MoveResult FleeFrom(entt::entity, glm::vec2) override { return MoveResult::Started; }
	bool TurnToFace(entt::entity, glm::vec2) override { return true; }
	void Stop(entt::entity creature) override { stopped.push_back(creature); }
	[[nodiscard]] bool IsMoving(entt::entity) const override { return moving; }
	[[nodiscard]] bool IsValidPosition(glm::vec2, float) const override { return true; }
};

/// The marks put on the skins
class FakeSkin final: public CreatureSkinSystemInterface
{
public:
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;

	void Update() override {}
	void ProcessTurn() override {}
	void SetTattoo(entt::entity, size_t, const creature_tattoo::Slot&) override {}
	void AddWound(entt::entity, const creature_marks::Mark& wound) override { wounds.push_back(wound); }
	void AddBlood(entt::entity, const creature_marks::Mark& drop) override { blood.push_back(drop); }
	void Heal(entt::entity, uint32_t) override {}
};

/// What the minds were told
class FakeMind final: public CreatureMindSystemInterface
{
public:
	std::vector<std::pair<entt::entity, float>> feedback;
	std::vector<size_t> forced;
	std::vector<std::pair<entt::entity, bool>> fought;
	bool forceResult {true};

	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(PlayerNames, size_t, const glm::vec3&, std::optional<entt::entity>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t, std::optional<bool>) override { return true; }
	bool PlayGesture(entt::entity, size_t) override { return true; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return true; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity creature, float value) override { feedback.emplace_back(creature, value); }
	bool ForceAction(entt::entity, size_t animation, bool, std::optional<creature_face::Request>, float) override
	{
		forced.push_back(animation);
		return forceResult;
	}
	bool Sleep(entt::entity) override { return true; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return true; }
	bool Drink(entt::entity) override { return true; }
	bool Poo(entt::entity) override { return true; }
	bool Puke(entt::entity) override { return true; }
	bool Faint(entt::entity) override { return true; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity creature, bool won) override { fought.emplace_back(creature, won); }
};

/// Hands that do nothing
class FakeObjectAction final: public CreatureObjectActionSystemInterface
{
public:
	std::vector<entt::entity> cancelled;

	void ProcessTurn() override {}
	void UpdateDraw(float) override {}
	void UpdateHeldDraw() override {}
	bool PickUp(entt::entity, entt::entity) override { return false; }
	bool PutDown(entt::entity) override { return false; }
	bool Discard(entt::entity) override { return false; }
	bool Lob(entt::entity) override { return false; }
	bool EatHeld(entt::entity) override { return false; }
	bool Keep(entt::entity, size_t) override { return false; }
	bool Throw(entt::entity, const glm::vec3&) override { return false; }
	bool Destroy(entt::entity, entt::entity) override { return false; }
	bool PointAt(entt::entity, const glm::vec3&) override { return false; }
	void Cancel(entt::entity creature) override { cancelled.push_back(creature); }
	void Drop(entt::entity) override {}
	[[nodiscard]] State GetState(entt::entity) const override { return State::Idle; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }
};
} // namespace openblack::test::creature_fakes
