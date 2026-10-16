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
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Creature/CreatureWebPage.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureWebPageSystem final: public CreatureWebPageSystemInterface
{
public:
	/// The player whose profile folder gets the files, none when no folder gets them
	using PlayerOfFiles = std::function<std::optional<std::u16string>()>;

	explicit CreatureWebPageSystem(PlayerOfFiles player);

	void ProcessTurn() override;
	void WritePage(entt::entity creature) override;
	[[nodiscard]] bool AnyPictureWaiting() const override;
	void CollectPictures(const Screens& screens) override;
	[[nodiscard]] std::span<const std::u16string> GetCaptions() const override;

private:
	/// A picture of the screen asked for, and where its file goes
	struct Waiting
	{
		std::string name;
		std::u16string player;
		uint32_t slot;
	};

	PlayerOfFiles _player;
	/// The captions belong to the slots of the files, not to a creature
	std::array<std::u16string, creature_web_page::k_MostPictures> _captions {};
	std::vector<Waiting> _waiting;
	uint32_t _asked {0};
};

} // namespace openblack::ecs::systems
