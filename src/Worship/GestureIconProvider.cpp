/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIconProvider.h"

#include <functional>
#include <memory>

#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "PlayerSpellIcons.h"
#include "WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::worship;

namespace
{
/// The interface's player: the human one
PlayerNames InterfacePlayer()
{
	for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
	{
		if (magic::players::IsHuman(static_cast<PlayerNames>(p)))
		{
			return static_cast<PlayerNames>(p);
		}
	}
	return PlayerNames::PLAYER_ONE; // (inferred): the original has no interface status then and does nothing
}

class WorshipIconProvider final: public magic::gestures::IconProvider
{
public:
	[[nodiscard]] bool AnyRequestableIconOfCategory(magic::gestures::Gesture category) const override
	{
		return player::AnyRequestableIconOfCategory(InterfacePlayer(), static_cast<GestureType>(category));
	}

	void ForEachRequestableIcon(magic::gestures::Gesture category,
	                            const std::function<void(int seedType)>& visit) const override
	{
		// The six sites' icons in list order, the category's that are ValidForRequestSpell(status, -1, 1)
		const auto playerName = InterfacePlayer();
		const auto& tables = Locator::infoConstants::value();
		for (const auto icon : player::Icons(playerName))
		{
			const auto seed = icon::SeedTypeOf(icon);
			if (magic::GetSpellSeedInfo(tables, seed).selectionGesture == static_cast<GestureType>(category) &&
			    icon::ValidForRequestSpell(icon, playerName, -1, true))
			{
				visit(static_cast<int>(seed));
			}
		}
	}

	[[nodiscard]] bool IconValidForRequest(int seedType) const override
	{
		return player::IconValidForRequest(InterfacePlayer(), static_cast<SpellSeedType>(seedType));
	}

	void RequestSpell(int seedType) override
	{
		// packet 0x25: IconValidForRequest, then the best icon's request
		const auto playerName = InterfacePlayer();
		if (player::IconValidForRequest(playerName, static_cast<SpellSeedType>(seedType)))
		{
			player::RequestSpell(playerName, static_cast<SpellSeedType>(seedType));
		}
	}

	[[nodiscard]] bool CanRepeat() const override { return player::CanRepeat(InterfacePlayer()); }

	void RepeatLast(int lastSeedType) override
	{
		// packet 0x26 with the interface's last seed type (the argument).
		// (approximate): openblack keeps a second copy per player (player::SetLastSeedType), overwritten here so that
		// RepeatLastSpell reads the interface's value; the original has no such copy to overwrite
		const auto playerName = InterfacePlayer();
		player::SetLastSeedType(playerName, static_cast<SpellSeedType>(lastSeedType));
		player::RepeatLastSpell(playerName);
	}

	[[nodiscard]] bool AnyIconChargingForHand() const override { return player::AnyIconChargingFor(InterfacePlayer()); }

	[[nodiscard]] float MaxChargeFraction() const override { return player::MaxChargeFraction(InterfacePlayer()); }

	void CancelMostChargedIcon() override { player::CancelMostRecentCharge(InterfacePlayer()); }

	[[nodiscard]] bool PowerUpAvailable(entt::entity icon, int powerUp) const override
	{
		return icon != entt::null && Locator::entitiesRegistry::value().Valid(icon) &&
		       icon::PowerUpValid(icon, InterfacePlayer(), powerUp);
	}

	void SetPowerUpCharge(entt::entity icon, int powerUp) override
	{
		player::SetChargingPowerUp(icon, InterfacePlayer(), powerUp);
	}
};
} // namespace

void gesture_icons::Register()
{
	magic::gestures::SetIconProvider(std::make_unique<WorshipIconProvider>());
}
