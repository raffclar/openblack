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

#include "ECS/Systems/VillageTotemSystemInterface.h"
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

private:
	/// The totem's own share is set: it eases there, its moving sound starting
	void SetTotemShare(entt::entity totem, float share);
	/// The town a totem's town centre belongs to, none without one
	[[nodiscard]] entt::entity TownOf(entt::entity totem) const;
	/// The plinth and its icon are put where the share stands them
	void Place(entt::entity totem);
	/// Where the hand holds the gripped totem
	[[nodiscard]] float GripY(entt::entity totem) const;

	std::unique_ptr<village_totem::WorldInterface> _world;
	entt::entity _gripped {entt::null};
};

} // namespace openblack::ecs::systems
