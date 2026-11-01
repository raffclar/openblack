/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
#pragma once

#include "ECS/Systems/GuidanceSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct GuidanceHeard;
}

namespace openblack::ecs::systems
{

class GuidanceSystem final: public GuidanceSystemInterface
{
public:
	void Reset() override;
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

	void ProcessTurn() override;
	bool PlayNow(guidance::Kind kind) override;
	bool RemarkNow(guidance::Kind kind) override;
	void RemarkIfNow(guidance::Kind kind) override;
	void TownAttacked(entt::entity town, PlayerNames attacker, bool byCreature) override;
	void TownAttackCry(entt::entity town, const guidance::TownAttackVoice& voice) override;
	void BeliefVoice(glm::vec3 at, float share, GuidanceAlignment alignment) override;
	void BuildingBroken(entt::entity building, std::optional<PlayerNames> player, float remaining) override;
	void AlignmentTurn(float change) override;
	void SayOneOff(guidance::OneOff oneOff) override;
	void SayText(uint32_t text, guidance::Kind kind) override { Say(text, kind); }
	void KilledVillager(entt::entity villager) override;
	void CreatureWonFight(entt::entity creature) override;
	void WorshipperDied() override;
	void ResourceDropped(PlayerNames giver, glm::vec3 at, guidance::Gift gift) override;
	void LosingVillager(entt::entity villager) override;
	void LowOnPeople(entt::entity town) override;
	void TownChangedHands(entt::entity town, PlayerNames loser, PlayerNames taker) override;
	void LosingBeliefTurn() override;
	void EndWaits() override;

	[[nodiscard]] const std::vector<Remark>& GetRemarks() const override { return _remarks; }
	[[nodiscard]] int32_t GetMoonWait() const override { return _moonWait; }
	[[nodiscard]] const night_whisper::Whisperer& GetNightWhisper() const override { return _whisper; }
	void StageNightWhisper(uint32_t count) override { _whisper.Stage(count); }

private:
	/// The guidance kept on the player at this machine, none without one
	components::GuidanceHeard* Heard();
	/// One of a kind's remarks, picked at random
	void SayRemark(guidance::Kind kind);
	void Say(uint32_t text, guidance::Kind kind);
	void LookAtMoon();
	/// The night whisper of this turn, if there is one. The name of the player is looked up again on each new land.
	void NightWhisper();
	/// Play the town desire voice of this turn, if there is one: the voice of the town or the worship site near the
	/// camera that wants something most
	void TownDesireVoices();

	/// The new land's stamps are still to be drawn
	bool _stampsDue {true};
	/// The moon's wait is kept from land to land, as the game keeps it
	int32_t _moonWait {1};
	std::vector<Remark> _remarks;
	/// The night whisper is a sound of this computer. It keeps its state from the start of the program, and no save keeps
	/// it.
	night_whisper::Whisperer _whisper;
	/// The name of the player is still to be looked up
	bool _nameDue {true};
};

} // namespace openblack::ecs::systems
