/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillageTotemWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class VillageTotemSystem final: public VillageTotemSystemInterface
{
public:
	/// On the game's world
	VillageTotemSystem();
	explicit VillageTotemSystem(std::unique_ptr<village_totem::WorldInterface> world);
	~VillageTotemSystem() override;

	void AddToPlayer(entt::entity totem) override;
	void SetTownShare(entt::entity town, float share) override;
	void Update(float gameMilliseconds) override;

	[[nodiscard]] std::optional<entt::entity> TotemOf(entt::entity picked) const override;
	bool Grip(entt::entity totem, PlayerNames player) override;
	void Slide(float upPixels, float screenHeight) override;
	void LetGo() override;
	[[nodiscard]] std::optional<entt::entity> GetGripped() const override;
	[[nodiscard]] std::optional<HandHold> GetHandHold() const override;
	[[nodiscard]] std::optional<ShareToolTip> TakeShareToolTip(PlayerNames player) override;

private:
	/// The totem's own share is set: it eases there, its moving sound starting
	void SetTotemShare(entt::entity totem, float share);
	/// The town a totem's town centre belongs to, none without one
	[[nodiscard]] entt::entity TownOf(entt::entity totem) const;
	/// The plinth and its icon are put where the share stands them
	void Place(entt::entity totem);
	/// The see-through second totem is put at the other share, made as the two come apart and taken away as they meet
	void PlaceGhost(entt::entity totem, std::optional<float> share);
	/// The totem's player sees the share it is drawn at by the hand's tooltip as it moves
	void NoteToolTip(entt::entity totem, const village_totem::Shown& shown);
	/// Takes the totem and its second totem away
	void Remove(entt::entity totem);
	/// Where the totem stands, its rise aside: where the living react to it and its numbers float from
	[[nodiscard]] glm::vec3 StandingAt(entt::entity totem) const;
	/// Where the hand holds the gripped totem
	[[nodiscard]] float GripY(entt::entity totem) const;

	std::unique_ptr<village_totem::WorldInterface> _world;
	entt::entity _gripped {entt::null};
	/// The latest tooltip of each player's totems, until asked
	std::unordered_map<PlayerNames, ShareToolTip> _toolTips;
};

} // namespace openblack::ecs::systems
