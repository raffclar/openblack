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

#include "ECS/Systems/ReactionSystemInterface.h"
#include "VillagerReactions.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct LivingReaction;
}

namespace openblack::ecs::systems
{

class ReactionSystem final: public ReactionSystemInterface
{
public:
	uint32_t Create(const Source& source) override;
	void Move(uint32_t id, const glm::vec3& position, const glm::vec3& velocity, float strength) override;
	[[nodiscard]] std::optional<Active> Find(uint32_t id) const override;
	void RemoveFrom(entt::entity initiator) override;
	void RemoveFrom(entt::entity initiator, Reaction type) override;
	void Remove(uint32_t id) override;
	void StopReacting(entt::entity living) override;
	void SetInitiator(uint32_t id, entt::entity initiator) override;
	void SetDance(uint32_t id, entt::entity dance) override;
	[[nodiscard]] bool IsActive(uint32_t id) const override;
	[[nodiscard]] bool HasReaction(entt::entity initiator) const override;
	void ProcessTurn() override;
	void Reset() override;
	void GiveTownBelief(entt::entity town, PlayerNames player, float amount, entt::entity from,
	                    GuidanceAlignment alignment) override;
	void SetLandBalance(Balance which, float balance) override { _landBalances.at(static_cast<size_t>(which)) = balance; }
	[[nodiscard]] float GetLandBalance(Balance which) const override { return _landBalances.at(static_cast<size_t>(which)); }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return _reactions; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3& point) const override;

private:
	/// The reaction is spread to the villagers and creatures in its reach, which may take it up
	void Spread(Active& reaction);
	/// The reaction reaches the living a map cell keeps, in its order: each takes it up, changes to it, or is impressed
	/// by it without reacting, the last the reaction reaches in that cell
	void SpreadInCell(Active& reaction, const std::vector<entt::entity>& mobiles);
	/// How urgent a reaction is to a living thing where it stands now, by its distance to the reaction or one given
	[[nodiscard]] uint32_t PriorityTo(const Active& reaction, entt::entity living, const glm::vec3& at) const;
	[[nodiscard]] uint32_t PriorityTo(const Active& reaction, entt::entity living, const glm::vec3& at, float distance) const;
	/// The turns a living thing waits before taking up a reaction's kind again, by its distance to the reaction's thing
	[[nodiscard]] uint32_t AgainTurns(const Active& reaction, entt::entity living, const glm::vec3& at) const;
	/// The turns a living thing keeps at the reaction it holds, by its distance to what it reacts to now; negative when it
	/// is over at once
	[[nodiscard]] int32_t TurnsToReact(const Active& reaction, entt::entity living, Reaction type) const;
	/// A living thing takes up a reaction: it is impressed by it, and does what it does about it
	/// A living takes up a reaction; one taking it over from another it reacted to counts as reacting already
	void Start(Active& reaction, entt::entity living, components::LivingReaction& state, bool takingOver = false);
	/// A living thing stops reacting, a villager going back to what it was doing when asked
	void Stop(entt::entity living, components::LivingReaction& state, bool resetState);
	/// A turn of each living thing's reaction: it stops once it has reacted its time, or its reaction has gone
	void ProcessLiving();
	/// Each town's turn of belief: what its people were impressed by is believed, rising from its town centre
	void BelieveInTowns();
	/// The reaction's impression on the living thing taking it up
	void Impress(const Active& reaction, entt::entity living, const glm::vec3& at);
	/// Every living thing reacting to it stops, and it goes
	void ShutDown(uint32_t id);

	std::vector<Active> _reactions;
	uint32_t _nextId {1};
	/// The next reaction spread again, in turn
	size_t _cursor {0};
	uint32_t _turn {0};
	/// The things' and the miracles' balances of impressiveness
	std::array<float, 3> _landBalances {1.0f, 1.0f, 1.0f};
	/// When the villagers' voices of belief were last heard
	villager_reactions::BeliefVoice _voice;
};

} // namespace openblack::ecs::systems
