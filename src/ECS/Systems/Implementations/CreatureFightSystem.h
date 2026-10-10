/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <random>

#include "Common/RandomNumberManager.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureFightSystem final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override;
	void Reset() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;

	StartResult StartFight(entt::entity creature, entt::entity opponent) override;
	void Withdraw(entt::entity creature) override;
	void AbortFight(entt::entity creature) override;
	[[nodiscard]] bool IsFighting(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity creature) const override;

	bool QueueMove(entt::entity creature, const creature_fight::Move& move, bool replace) override;
	void ReleaseCharge(entt::entity creature, float heldMs) override;
	void SetAutoFighting(entt::entity creature, bool autoFight) override;
	[[nodiscard]] bool IsAutoFighting(entt::entity creature) const override;
	[[nodiscard]] creature_fight::FightAction CurrentFightAction(entt::entity creature) const override;
	[[nodiscard]] uint32_t QueuedBlows(entt::entity creature) const override;
	[[nodiscard]] std::optional<FoundArena> FindOrMakeArena(const glm::vec3& point, entt::entity creature, entt::entity other,
	                                                        float within) override;

	[[nodiscard]] std::optional<entt::entity> PlayersFighter() const override;
	bool Press(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, creature_fight::Button button, uint32_t milliseconds,
	           uint32_t turn) override;
	void Release(uint32_t milliseconds, uint32_t turn) override;
	[[nodiscard]] bool IsPressed() const override { return _pressed.has_value(); }
	[[nodiscard]] std::optional<creature_fight::Tip> HandTip(std::optional<entt::entity> under) const override;
	bool GestureSpecialMove() override;
	bool GestureSpell(MagicType type) override;

	[[nodiscard]] bool IsBlocking(entt::entity creature) const override;
	void Recoil(entt::entity creature) override;
	void KnockOut(entt::entity creature) override;
	void ForceFaint(entt::entity creature) override;
	void KillPermanently(entt::entity creature) override;
	void Resurrect(entt::entity creature) override;
	[[nodiscard]] bool IsKnockedOut(entt::entity creature) const override;

	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override;

	void SetAngerStartsFights(bool enabled) override { _angerStartsFights = enabled; }
	[[nodiscard]] bool GetAngerStartsFights() const override { return _angerStartsFights; }
	void SetCameraWatches(bool enabled) override { _cameraWatches = enabled; }
	[[nodiscard]] bool GetCameraWatches() const override { return _cameraWatches; }
	[[nodiscard]] bool IsCameraOnFight() const final { return _view.has_value(); }
	void SetFightExit(bool allowed) override { _fightExit = allowed; }
	[[nodiscard]] bool GetFightExit() const override { return _fightExit; }

private:
	/// The turn's parts: fights picked by angry creatures and started by the leash, the stages before and after the
	/// duel, the duel's moves, and the creatures knocked out
	void StartFightsFromMinds();
	void ProcessStages();
	void ProcessDuels();
	void ProcessKnockedOut();
	/// Makes the move at the front of a fighter's queue, if it can
	void CheckQueue(entt::entity creature);
	/// A blow at a band: struck, or a step taken towards where it would land
	/// The spell a fighter casts as its cast's start ends: an attacking one at the opponent, a defending one on itself
	void CastFightSpell(entt::entity creature, MagicType type, entt::entity opponent);
	void AttemptBlow(entt::entity creature, creature_fight::Band band, float speed);
	/// A fighter's action landing on its opponent this frame, if it does
	void TestHit(entt::entity creature);
	/// One creature beat the other: the loser faints and the winner shows off
	void Win(entt::entity winner, entt::entity loser);
	/// The fight ends for a creature: its life pays for it and it learns from it
	void EndFightFor(entt::entity creature, bool won);
	void BeginDuel(entt::entity creature);
	void MeasureBlows(entt::entity creature);
	/// Faints and lies out cold, to be taken home later
	void Faint(entt::entity creature);
	/// The camera's fight view: started by looking at an arena with a fight on, it follows the fight and lingers a little
	/// after it, unless the player zooms out of it
	void UpdateView(float seconds);
	void TryStartView(entt::entity first, entt::entity second, const creature_fight::Arena& arena);
	void EndView();
	/// Leaves the fight for good, its mind taking over again
	void Leave(entt::entity creature);
	/// Whether the line from the player's fighter to its opponent meets the opponent's body, as gestures need
	[[nodiscard]] bool SeesOpponent(entt::entity creature) const;

	bool _angerStartsFights {true};
	bool _cameraWatches {true};
	/// The player's creature a press is charging a blow for, and when it was pressed
	struct Pressed
	{
		entt::entity creature;
		uint32_t milliseconds;
		uint32_t turn;
	};
	std::optional<Pressed> _pressed;
	/// The fight the camera watches: the creature that made the arena and the other, and once the fight is over how
	/// long the view lingers
	struct Watched
	{
		entt::entity first;
		entt::entity second;
		std::optional<float> lingerSeconds;
	};
	std::optional<Watched> _view;
	/// How long the camera has looked at an arena from within it
	float _lookSeconds {0.0f};
	/// Whether the player may leave the fight view, and it ends by itself (scripts may forbid it)
	bool _fightExit {true};
	RandomStreamSource _random {RandomStream::CreatureFight};
};

} // namespace openblack::ecs::systems
