/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"

/// Fakes of the creature services that write each turn or frame call into one shared log, in the order they are made,
/// with the arguments they get. Every other method is a stub that does nothing. The tests hold the fakes and read their
/// log; the locator only takes them in.
namespace openblack::test::creature_loop_fakes
{
using namespace openblack::ecs::systems;

/// One call: the service and method, and the numbers it was given
struct Call
{
	std::string name;
	std::vector<float> args;

	bool operator==(const Call&) const = default;
};
using CallLog = std::vector<Call>;

/// Writes the calls of one fake into the shared log
class Recorder
{
public:
	explicit Recorder(CallLog& log)
	    : _log(&log)
	{
	}

protected:
	void Record(std::string name, std::vector<float> args = {}) const
	{
		_log->push_back({.name = std::move(name), .args = std::move(args)});
	}

private:
	CallLog* _log;
};

/// A frame's game time as the services take it, in milliseconds
inline float Ms(std::chrono::duration<float, std::milli> gameTime)
{
	return gameTime.count();
}

class FakePhysiology final: public CreaturePhysiologySystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void ProcessTurn() override { Record("physiology.ProcessTurn"); }
	void Update(float seconds) override { Record("physiology.Update", {seconds}); }
	[[nodiscard]] std::span<const PukeDrop> GetPukeDrops() const override { return {}; }
	void Eat(entt::entity, float) override {}
	void Drink(entt::entity) override {}
	void Poo(entt::entity) override {}
	void Puke(entt::entity) override {}
	void WakeFromFaint(entt::entity) override {}
	void FinishAction(entt::entity, std::string_view) override {}
	void ModifyStrength(entt::entity, float) override {}
	void SetTimeScale(float) override {}
	[[nodiscard]] float GetTimeScale() const override { return 1.0f; }
	void SetFaintingEnabled(bool) override {}
	[[nodiscard]] bool IsFaintingEnabled() const override { return true; }
};

class FakeAnimation final: public CreatureAnimationSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void ProcessTurn() override { Record("animation.ProcessTurn"); }
	void Update(std::chrono::duration<float, std::milli> gameTime) override { Record("animation.Update", {Ms(gameTime)}); }
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t) override { return std::nullopt; }
};

class FakeSkin final: public CreatureSkinSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void Update() override { Record("skin.Update"); }
	void ProcessTurn() override { Record("skin.ProcessTurn"); }
	void SetTattoo(entt::entity, size_t, const creature_tattoo::Slot&) override {}
	void AddWound(entt::entity, const creature_marks::Mark&) override {}
	void AddBlood(entt::entity, const creature_marks::Mark&) override {}
	void Heal(entt::entity, uint32_t) override {}
};

class FakeHair final: public CreatureHairSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void Update(std::chrono::duration<float, std::milli> gameTime) override { Record("hair.Update", {Ms(gameTime)}); }
	[[nodiscard]] bool IsShown() const override { return true; }
	void SetShown(bool) override {}
};

class FakeAudio final: public CreatureAudioSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void Update(std::chrono::duration<float, std::milli> gameTime) override { Record("audio.Update", {Ms(gameTime)}); }
	[[nodiscard]] bool IsMuted() const override { return false; }
	void SetMuted(bool) override {}
	[[nodiscard]] bool AreOtherVoicesEnabled() const override { return true; }
	void SetOtherVoicesEnabled(bool) override {}
	void Play(entt::entity, const creature_audio::SoundEvent&) override {}
};

class FakeFootprints final: public FootprintSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void Update(std::chrono::duration<float, std::milli> gameTime) override { Record("footprints.Update", {Ms(gameTime)}); }
	void Step(entt::entity) override {}
	void Reset() override { Record("footprints.Reset"); }
	[[nodiscard]] std::span<const creature_footprints::Footprint> GetPrints() const override { return {}; }
	[[nodiscard]] size_t GetDroppedCount() const override { return 0; }
	[[nodiscard]] bool IsShown() const override { return true; }
	void SetShown(bool) override {}
	[[nodiscard]] std::optional<bool> GetAprilFoolsOverride() const override { return std::nullopt; }
	void SetAprilFoolsOverride(std::optional<bool>) override {}
	[[nodiscard]] bool IsAprilFools() const override { return false; }
};

class FakeLocomotion final: public CreatureLocomotionSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void ProcessTurn() override { Record("locomotion.ProcessTurn"); }
	void Update(float turnFraction) override { Record("locomotion.Update", {turnFraction}); }
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Busy; }
	MoveResult LeadTo(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	void Stop(entt::entity) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return false; }
	[[nodiscard]] bool IsValidPosition(glm::vec2, float) const override { return false; }
};

class FakeMind final: public CreatureMindSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void ProcessTurn() override { Record("mind.ProcessTurn"); }
	void PlanTurn() override { Record("mind.PlanTurn"); }
	void LearnTurn() override { Record("mind.LearnTurn"); }
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	/// The player, the deed, the point and the object (-1 for none)
	void PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object) override
	{
		Record("mind.PlayerDid", {static_cast<float>(player), static_cast<float>(deed), point.x, point.y, point.z,
		                          object.has_value() ? static_cast<float>(entt::to_integral(*object)) : -1.0f});
	}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t, std::optional<bool>) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, std::optional<creature_face::Request>, float) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
};

class FakeObjectAction final: public CreatureObjectActionSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void ProcessTurn() override { Record("objectAction.ProcessTurn"); }
	void UpdateDraw(float turnFraction) override { Record("objectAction.UpdateDraw", {turnFraction}); }
	void UpdateHeldDraw() override { Record("objectAction.UpdateHeldDraw"); }
	bool PickUp(entt::entity, entt::entity) override { return false; }
	bool PutDown(entt::entity) override { return false; }
	bool Discard(entt::entity) override { return false; }
	bool Lob(entt::entity) override { return false; }
	bool EatHeld(entt::entity) override { return false; }
	bool Keep(entt::entity, size_t) override { return false; }
	bool Throw(entt::entity, const glm::vec3&) override { return false; }
	bool Destroy(entt::entity, entt::entity) override { return false; }
	bool PointAt(entt::entity, const glm::vec3&) override { return false; }
	void Cancel(entt::entity) override {}
	void Drop(entt::entity) override {}
	[[nodiscard]] State GetState(entt::entity) const override { return State::Idle; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }
};

class FakeFight final: public CreatureFightSystemInterface, Recorder
{
public:
	using Recorder::Recorder;
	void ProcessTurn() override { Record("fight.ProcessTurn"); }
	void Update(float turnFraction, float frameMs) override { Record("fight.Update", {turnFraction, frameMs}); }
	StartResult StartFight(entt::entity, entt::entity) override { return StartResult::NoOpponent; }
	void AbortFight(entt::entity) override {}
	[[nodiscard]] bool IsFighting(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity) const override { return std::nullopt; }
	bool QueueMove(entt::entity, const creature_fight::Move&, bool) override { return false; }
	void ReleaseCharge(entt::entity, float) override {}
	void SetAutoFighting(entt::entity, bool) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity) const override { return false; }
	bool Press(const glm::vec3&, const glm::vec3&) override { return false; }
	void Release() override {}
	[[nodiscard]] bool IsPressed() const override { return false; }
	void KnockOut(entt::entity) override {}
	void KillPermanently(entt::entity) override {}
	void Resurrect(entt::entity) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetAngerStartsFights(bool) override {}
	[[nodiscard]] bool GetAngerStartsFights() const override { return false; }
	void SetCameraWatches(bool) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
};

/// An entity as a logged number
inline float Id(entt::entity entity)
{
	return static_cast<float>(entt::to_integral(entity));
}

/// The leash service: the turn and frame calls and the leash commands are logged with their arguments; the player's
/// creature and its leash state are what the test sets
class FakeLeash final: public LeashSystemInterface, Recorder
{
public:
	using Recorder::Recorder;

	std::optional<entt::entity> playersCreature;
	bool leashable {false};
	bool leashed {false};
	std::optional<entt::entity> tiedTo;
	LeashType type {LeashType::None};
	LeashType picked {LeashType::None};

	void ProcessTurn() override { Record("leash.ProcessTurn"); }
	void Update(float seconds) override { Record("leash.Update", {seconds}); }
	[[nodiscard]] bool Knows(entt::entity, LeashType) const override { return false; }
	void SetKnown(entt::entity, LeashType, bool) override {}
	[[nodiscard]] bool IsLeashable(entt::entity) const override { return leashable; }
	bool SetLeashable(entt::entity, bool) override { return false; }
	void SetOwner(entt::entity, PlayerNames) override {}
	void ClaimOnArrival(entt::entity) override {}
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames, entt::entity, LeashType) const override
	{
		return creature_leash::Refusal::None;
	}
	[[nodiscard]] std::optional<Refused> LastRefusal(PlayerNames /*player*/) const override { return std::nullopt; }
	bool PutOn(entt::entity, LeashType) override { return false; }
	void TakeOff(entt::entity creature) override { Record("leash.TakeOff", {Id(creature)}); }
	bool Toggle(entt::entity creature) override
	{
		Record("leash.Toggle", {Id(creature)});
		return true;
	}
	bool ChangeType(entt::entity, LeashType) override { return false; }
	bool TieTo(entt::entity creature, entt::entity object) override
	{
		Record("leash.TieTo", {Id(creature), Id(object)});
		return true;
	}
	void UntieToHand(entt::entity creature) override { Record("leash.UntieToHand", {Id(creature)}); }
	void SetWorks(entt::entity creature, bool works) override { Record("leash.SetWorks", {Id(creature), works ? 1.0f : 0.0f}); }
	void ConfineToHome(entt::entity, float) override {}
	void ClearConfinement(entt::entity) override {}
	void SetHome(entt::entity, const glm::vec3&) override {}
	[[nodiscard]] bool FreeOfHome(entt::entity) const override { return true; }
	[[nodiscard]] bool IsLeashed(entt::entity) const override { return leashed; }
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity) const override { return tiedTo; }
	[[nodiscard]] LeashType TypeOf(entt::entity) const override { return type; }
	[[nodiscard]] LeashType Picked(entt::entity) const override { return picked; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames) const override { return playersCreature; }
	bool PressKey(PlayerNames player, creature_leash::LeashKey key) override
	{
		Record("leash.PressKey", {static_cast<float>(player), static_cast<float>(key)});
		return true;
	}
	bool TapCreature(PlayerNames, entt::entity) override { return false; }
	bool TakeOffHeldLeash(PlayerNames player) override
	{
		Record("leash.TakeOffHeldLeash", {static_cast<float>(player)});
		return true;
	}
};
} // namespace openblack::test::creature_loop_fakes
