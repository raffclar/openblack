/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Systems/FootballSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct FootballPitch;
}

namespace openblack::ecs::systems
{

class FootballSystem final: public FootballSystemInterface
{
public:
	void StartGame(bool addOnSwitch, bool multiplayer) override;
	[[nodiscard]] bool IsOn() const override;
	void ProcessTown(entt::entity town) override;
	[[nodiscard]] std::optional<float> RelaxationDesire(entt::entity town, entt::entity villager) const override;
	bool Join(entt::entity town, entt::entity villager) override;
	void Leave(entt::entity villager) override;
	[[nodiscard]] entt::entity PitchOf(entt::entity villager) const override;
	entt::entity BallOf(entt::entity pitch) override;
	uint32_t PlayRole(components::LivingAction& action) override;
	void RestartDeadBall(entt::entity villager) override;
	[[nodiscard]] std::optional<football_rules::PlayState> PlayStateOf(entt::entity pitch) const override;

private:
	/// A goal of the goal model, in the model's own space
	struct GoalBox
	{
		glm::vec3 centre {0.0f};
		glm::vec3 halfSize {0.0f};
	};

	/// The goals of the goal model, read once they are first needed
	const std::vector<GoalBox>& Goals();
	/// A goal's centre on the land, in metres. The centre spot when the goal model has no such goal.
	glm::vec3 GoalPosition(entt::entity pitch, uint32_t goal);
	/// A pitch that now works becomes its town's pitch, with a ball
	void MakeFunctional(entt::entity town, entt::entity pitch);
	/// One turn of a town's pitch
	void ProcessPitch(entt::entity town, entt::entity pitch);
	/// The ball in play: a goal, and whether the ball is on the pitch
	bool ProcessBall(entt::entity pitch, components::FootballPitch& football, entt::entity ball);
	void ScoreGoal(components::FootballPitch& football, uint32_t goal);
	void RankTeams(entt::entity pitch, components::FootballPitch& football);
	void GiveRoles(components::FootballPitch& football, size_t side);
	void ChooseTasks(entt::entity pitch, const components::FootballPitch& football, size_t side);
	void ChooseTask(entt::entity pitch, entt::entity villager);
	void StopMatch(entt::entity town, entt::entity pitch, components::FootballPitch& football);
	void SendPlayersHome(components::FootballPitch& football);
	void ForgetGone(components::FootballPitch& football);

	/// A player's own place on the pitch
	glm::vec3 PlaceOf(entt::entity pitch, entt::entity villager);
	/// A point that a kick aims at moves by a random amount that grows with the distance from the player
	glm::vec3 Perturb(glm::vec3 target, entt::entity villager);
	void Kick(entt::entity pitch, entt::entity ball, glm::vec3 target, float speed, bool canScore);

	void DoTask(entt::entity pitch, entt::entity villager, VillagerStates role, uint8_t task);
	void GoToBall(entt::entity pitch, entt::entity villager, VillagerStates role);
	void Pass(entt::entity pitch, entt::entity villager, VillagerStates role);
	void RunForPass(entt::entity pitch, entt::entity villager);
	void IdleOutfield(entt::entity pitch, entt::entity villager);
	void GoalkeeperLook(entt::entity pitch, entt::entity villager);
	[[nodiscard]] bool Passes(entt::entity pitch, entt::entity villager, football_rules::Test test);
	[[nodiscard]] float Priority(entt::entity pitch, entt::entity villager, football_rules::Role role, uint8_t task);

	bool _on {false};
	std::optional<std::vector<GoalBox>> _goals;
	/// The match of only one pitch moves on in each game turn: the first pitch that the turn reaches
	std::optional<uint32_t> _lastMatchTurn;
};

} // namespace openblack::ecs::systems
