/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSpellIcon.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "InterfaceStatus.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "SpellSeedGraphic.h"
#include "TownMagic.h"
#include "WorshipSite.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The charge pulse ring: mesh 561 (MSH_S_PULSE_IN)
constexpr MeshId k_PulseMesh = static_cast<MeshId>(561);

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

bool IsIcon(entt::entity icon)
{
	return icon != entt::null && Registry().Valid(icon) && Registry().AllOf<WorshipSpellIcon, SpellIcon>(icon);
}

WorshipSpellIcon& IconOf(entt::entity icon)
{
	return Registry().Get<WorshipSpellIcon>(icon);
}

bool HasSite(const WorshipSpellIcon& icon)
{
	return icon.site != entt::null && Registry().Valid(icon.site) && Registry().AllOf<WorshipSite>(icon.site);
}

const GSpellSeedInfo& SeedInfoOf(entt::entity icon)
{
	return magic::GetSpellSeedInfo(Locator::infoConstants::value(), Registry().Get<const SpellIcon>(icon).seedType);
}

/// A seed charged from this icon; its creator is the icon and its scale the icon's x the seed's (scale)
entt::entity CreateSeedFromIcon(entt::entity iconEntity, const glm::vec3& position, PlayerNames player, int powerUp,
                                float multiplier)
{
	auto& registry = Registry();
	const auto& transform = registry.Get<const Transform>(iconEntity);
	const auto seedType = registry.Get<const SpellIcon>(iconEntity).seedType;
	const auto seedEntity = magic::seed::Create(position, seedType, player, powerUp, multiplier);
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	seed.icon = iconEntity;
	seed.creator = {SpellCreator::Kind::WorshipSpellIcon, icon::PlayerOf(iconEntity), iconEntity};
	if (auto* seedTransform = registry.TryGet<Transform>(seedEntity); seedTransform != nullptr)
	{
		seedTransform->scale = transform.scale * SeedInfoOf(iconEntity).scale;
	}
	icon::AddSeed(iconEntity, seedEntity);
	return seedEntity;
}

/// The voice of a fully charged seed, by seed type: bank 9 (SpellDialogue), 8 for the unlisted ones
constexpr std::array<int, 30> k_FullyChargedVoice = {
    14, 6, 2, 5, 13, 9, 8, 7, 17, 16, 3, 4, 21, 24, 19, 26, 25, 8, 8, 22, 20, 18, 8, 8, 8, 8, 8, 23, 15, 1,
};

/// IN_GAME 42 (G_ClickOnSpell_01) with the pitch of the icon's placement
void PlayTapSound(int placement)
{
	// The index is clamped to 0..5, so the 7th (190) is never read
	constexpr std::array<int, 7> k_Pitch = {100, 115, 130, 145, 155, 175, 190};
	const int index = std::clamp(placement, 0, 5);
	// Default options with the InGame bank, sample 42, no owner, not 3D and the pitch. The caller mask stays 0: the
	// .sad's flags of sample 42 have no pitch bit, so the sample keeps the options' pitch.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 0x2A};
	options.owner = audio::Owner::None();
	options.is3D = false;
	options.pitch = k_Pitch.at(static_cast<size_t>(index));
	audio::PlaySoundEffect(options);
}

/// The icon's placement: a worship icon's slot - 10; a town centre icon's index in the town centre's icons
int PlacementIndexOf(entt::entity icon)
{
	auto& registry = Registry();
	if (const auto* worship = registry.TryGet<const WorshipSpellIcon>(icon); worship != nullptr)
	{
		return worship->slot - 10;
	}
	if (const auto* town = registry.TryGet<const TownCentreSpellIcon>(icon); town != nullptr)
	{
		return town->slot;
	}
	return 0;
}

} // namespace

/// A worship icon is its own; a town centre's icon asks its town's site for the icon of its seed
entt::entity icon::WorshipIconOf(entt::entity icon)
{
	auto& registry = Registry();
	if (registry.AllOf<WorshipSpellIcon>(icon))
	{
		return icon;
	}
	const auto* town = registry.TryGet<const TownCentreSpellIcon>(icon);
	if (town == nullptr || town->town == entt::null || !registry.Valid(town->town))
	{
		return entt::null;
	}
	const auto* magic = registry.TryGet<const TownMagic>(town->town);
	if (magic == nullptr || magic->worshipSite == entt::null)
	{
		return entt::null;
	}
	return site::GetSpellIconFromSeedType(magic->worshipSite, registry.Get<const SpellIcon>(icon).seedType);
}

entt::entity icon::Create(const glm::vec3& worldPosition, SpellSeedType seed, entt::entity siteEntity, int16_t slot)
{
	auto& registry = Registry();
	const auto& site = registry.Get<const WorshipSite>(siteEntity);
	const auto& siteTransform = registry.Get<const Transform>(siteEntity);
	const auto& iconInfo = Locator::infoConstants::value().spellIcon.at(0); // "Spell Icon"

	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// At pos, with the site's scale and angle
	registry.Assign<Transform>(entity, worldPosition, affine::AngleY(site.yAngle), siteTransform.scale);
	// The info's mesh (203)
	registry.Assign<Mesh>(entity, resources::HashIdentifier(iconInfo.meshId), static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& spellIcon = registry.Assign<SpellIcon>(entity);
	spellIcon.infoIndex = 0;
	spellIcon.seedType = seed;
	spellIcon.player = site.player;
	auto& icon = registry.Assign<WorshipSpellIcon>(entity);
	icon.site = siteEntity;
	icon.slot = slot;
	// At the head of the site's list
	auto& icons = registry.Get<WorshipSite>(siteEntity).icons;
	icons.insert(icons.begin(), entity);

	// The graphic at the icon mesh's special point 0 + 1.0
	const auto point = GetSpecialPoint(entity, 0);
	const auto graphicPosition = (point ? point->position : worldPosition) + glm::vec3(0.0f, 1.0f, 0.0f);
	const auto graphic = seed_graphic::Create(graphicPosition, seed, site.player, 1.0f, -1);
	seed_graphic::SetAutoUpdate(graphic, false);
	registry.Get<SpellIcon>(entity).graphic = graphic;
	UpdatePowerUpGraphics(entity);
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Worship trace: icon {} for seed {} at slot {} ({:.1f}, {:.1f}, {:.1f}) of site {}",
		                   static_cast<uint32_t>(entity), static_cast<int>(seed), slot, worldPosition.x, worldPosition.y,
		                   worldPosition.z, static_cast<uint32_t>(siteEntity));
	}
	return entity;
}

void icon::ToBeDeleted(entt::entity iconEntity)
{
	if (!IsIcon(iconEntity))
	{
		return;
	}
	auto& registry = Registry();
	auto& icon = IconOf(iconEntity);
	if (HasSite(icon))
	{
		auto& icons = registry.Get<WorshipSite>(icon.site).icons;
		icons.erase(std::remove(icons.begin(), icons.end(), iconEntity), icons.end());
		icon.site = entt::null;
	}
	for (const auto seed : std::vector<entt::entity>(icon.seeds))
	{
		magic::seed::ToBeDeleted(seed); // every seed of the icon
	}
	// Out of the map, the graphic goes
	ecs::map_cells::RemoveMapObject(iconEntity);
	const auto& spellIcon = registry.Get<const SpellIcon>(iconEntity);
	seed_graphic::Delete(spellIcon.graphic);
	if (spellIcon.chargeRing != entt::null && registry.Valid(spellIcon.chargeRing))
	{
		registry.Destroy(spellIcon.chargeRing);
	}
	registry.Destroy(iconEntity);
	registry.SetDirty();
}

PlayerNames icon::PlayerOf(entt::entity icon)
{
	// A town centre's icon belongs to its town's owner at the time of asking
	if (const auto* townIcon = Registry().TryGet<const TownCentreSpellIcon>(icon); townIcon != nullptr)
	{
		return town::OwnerOf(townIcon->town);
	}
	return Registry().Get<const SpellIcon>(icon).player;
}

SpellSeedType icon::SeedTypeOf(entt::entity icon)
{
	return Registry().Get<const SpellIcon>(icon).seedType;
}

MagicType icon::MagicTypeOf(entt::entity icon, int powerUp)
{
	return magic::MagicInfoForPowerUpLevel(Locator::infoConstants::value(), SeedInfoOf(icon), powerUp).magicType;
}

void icon::UpdatePowerUpGraphics(entt::entity icon)
{
	const auto& spellIcon = Registry().Get<const SpellIcon>(icon);
	if (spellIcon.graphic == entt::null)
	{
		return;
	}
	int level = -1;
	for (int pu = 0; pu < 3; ++pu)
	{
		if (magic::players::IsMagicTypeEnabled(PlayerOf(icon), magic::MagicTypeForPowerUpLevel(SeedInfoOf(icon), pu)))
		{
			level = pu;
		}
	}
	seed_graphic::SetPowerUpType(spellIcon.graphic, level);
	seed_graphic::SetBandScale(spellIcon.graphic, 0.5f); // the band size
}

int icon::Process(entt::entity iconEntity)
{
	auto& icon = IconOf(iconEntity);
	if (icon.removeTimer != 0 && --icon.removeTimer == 0)
	{
		ToBeDeleted(iconEntity);
		return 3;
	}
	if (influence::IsInfluenceEverywhere() && PlayerOf(iconEntity) == PlayerNames::NEUTRAL)
	{
		AddToChantStore(iconEntity, Locator::infoConstants::value().spellIcon.at(0).gatheringChantAddPerGameTurn);
	}
	if (!IconOf(iconEntity).charging)
	{
		return 1;
	}
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		UseCreateChants(iconEntity, seed);
	}
	if (GetChantNeeded(iconEntity) <= 0.0f)
	{
		auto& charged = IconOf(iconEntity);
		if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
		{
			magic::seed::SetPowerUp(seed, charged.powerUp);
		}
		else
		{
			if (charged.hasChargingInterface && ValidForPutFullyChargedSeedInHand(iconEntity, charged.chargingFor))
			{
				PutFullyChargedSeedInHand(iconEntity, charged.chargingFor);
			}
			// For the local player's icons, the seed's voice from bank 9
			if (magic::players::IsHuman(PlayerOf(iconEntity)))
			{
				// A seed type above 29 gives 8; no owner, mode 2, no loops, not 3D
				const auto seedType = static_cast<size_t>(SeedTypeOf(iconEntity));
				const int voice = seedType < k_FullyChargedVoice.size() ? k_FullyChargedVoice.at(seedType) : 8;
				audio::PlaySoundEffect(audio::Owner::None(), voice, 2, 0, false, false, audio::SfxBank::SpellDialogue);
			}
		}
		auto& done = IconOf(iconEntity);
		done.charging = false;
		done.hasChargingInterface = false;
		done.powerUp = -1;
		if (trace::Enabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: icon {} fully charged", static_cast<uint32_t>(iconEntity));
		}
	}
	return 1;
}

entt::entity icon::GetHeldSpellSeed(entt::entity icon)
{
	// Over the icon's player's hands
	const auto seed = interface::HeldSpellSeed(PlayerOf(icon));
	if (seed != entt::null && Registry().Get<const SpellSeed>(seed).icon == icon)
	{
		return seed;
	}
	return entt::null;
}

float icon::GetChantRequired(entt::entity iconEntity)
{
	const auto& icon = IconOf(iconEntity);
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		return magic::seed::GetChantNeeded(Registry().Get<const SpellSeed>(seed), icon.powerUp);
	}
	return magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), MagicTypeOf(iconEntity, icon.powerUp));
}

float icon::GetChantNeeded(entt::entity iconEntity)
{
	const auto& icon = IconOf(iconEntity);
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		return magic::seed::GetChantNeeded(Registry().Get<const SpellSeed>(seed), icon.powerUp);
	}
	return GetChantRequired(iconEntity) - icon.chantStore;
}

float icon::ChargeFraction(entt::entity iconEntity)
{
	const auto& icon = IconOf(iconEntity);
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		// The seed's need at that level over its cost (inferred: the fraction still to charge)
		const auto& component = Registry().Get<const SpellSeed>(seed);
		const float cost =
		    magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), MagicTypeOf(iconEntity, icon.powerUp));
		return cost != 0.0f ? magic::seed::GetChantNeeded(component, icon.powerUp) / cost : 0.0f;
	}
	const float required = GetChantRequired(iconEntity);
	return required != 0.0f ? icon.chantStore / required : 0.0f;
}

bool icon::IsCharging(entt::entity icon, PlayerNames player, bool anyPlayer)
{
	const auto& component = IconOf(icon);
	if (!anyPlayer && (!component.hasChargingInterface || component.chargingFor != player))
	{
		return false;
	}
	return component.charging;
}

float icon::AddToChantStore(entt::entity iconEntity, float chants)
{
	auto& icon = IconOf(iconEntity);
	const float total = chants + icon.chantStore;
	const float required = GetChantRequired(iconEntity);
	if (required < total)
	{
		// the quirk: the excess is what the caller charges to the site
		const float excess = chants - (required - icon.chantStore);
		IconOf(iconEntity).chantStore = required;
		return excess;
	}
	icon.chantStore = total;
	return chants;
}

float icon::RemoveFromChantStore(entt::entity iconEntity, float chants)
{
	auto& icon = IconOf(iconEntity);
	const float removed = chants <= icon.chantStore ? chants : icon.chantStore;
	icon.chantStore -= removed;
	return removed;
}

void icon::ReturnAllChantsToWorshipSite(entt::entity iconEntity)
{
	auto& icon = IconOf(iconEntity);
	if (HasSite(icon))
	{
		Registry().Get<WorshipSite>(icon.site).battery += icon.chantStore;
	}
	RemoveFromChantStore(iconEntity, icon.chantStore);
}

float icon::UseCreateChants(entt::entity iconEntity, entt::entity seedEntity)
{
	auto& icon = IconOf(iconEntity);
	auto& seed = Registry().Get<SpellSeed>(seedEntity);
	const float need = magic::seed::GetChantNeeded(seed, icon.powerUp);
	if (need <= 0.0f)
	{
		return 0.0f;
	}
	const float moved = need < icon.chantStore ? need : icon.chantStore;
	magic::seed::AddToChantStore(seed, moved);
	RemoveFromChantStore(iconEntity, moved);
	return moved;
}

entt::entity icon::CreateSeed(entt::entity icon, const glm::vec3& position, PlayerNames player, int powerUp, float multiplier)
{
	return CreateSeedFromIcon(icon, position, player, powerUp, multiplier);
}

bool icon::ValidForPutFullyChargedSeedInHand(entt::entity icon, PlayerNames player)
{
	return GetChantNeeded(icon) <= 0.0f && GetHeldSpellSeed(icon) == entt::null && interface::IsHandReadyForObject(player) &&
	       magic::players::IsHuman(player);
}

int icon::PutFullyChargedSeedInHand(entt::entity iconEntity, PlayerNames player)
{
	if (!interface::IsHandReadyForObject(player))
	{
		return 0;
	}
	// At the icon, no power-up, multiplier 1
	const auto seed = CreateSeedFromIcon(iconEntity, Registry().Get<const Transform>(iconEntity).position, player, -1, 1.0f);
	UseCreateChants(iconEntity, seed);
	if (interface::PlaceSeedInMagicHand(player, seed) != 1)
	{
		return 1;
	}
	auto& registry = Registry();
	if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed))
	{
		magic::seed::SetInactive(registry.Get<SpellSeed>(seed), true);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: seed {} of icon {} in the hand with {:.0f} chants",
		                   static_cast<uint32_t>(seed), static_cast<uint32_t>(iconEntity),
		                   registry.Get<const SpellSeed>(seed).chantStore);
	}
	return 1;
}

bool icon::CancelCharge(entt::entity iconEntity, PlayerNames player)
{
	auto& icon = IconOf(iconEntity);
	if (!icon.charging && icon.chantStore == 0.0f)
	{
		return false;
	}
	if (!icon.hasChargingInterface || icon.chargingFor != player)
	{
		return false;
	}
	icon.charging = false;
	icon.powerUp = -1;
	icon.hasChargingInterface = false;
	ReturnAllChantsToWorshipSite(iconEntity);
	return true;
}

bool icon::StartCharge(entt::entity iconEntity, PlayerNames player, int powerUp, bool requireChants)
{
	auto& icon = IconOf(iconEntity);
	if (icon.charging)
	{
		return false;
	}
	bool chants = true;
	if (HasSite(icon))
	{
		if (requireChants && !(site::TotalChantsAvailable(Registry().Get<const WorshipSite>(icon.site)) > 0.0f))
		{
			chants = icon.chantStore != 0.0f;
		}
	}
	else
	{
		chants = icon.chantStore != 0.0f;
	}
	if (!chants)
	{
		return false;
	}
	icon.powerUp = powerUp;
	icon.charging = true;
	icon.chargingFor = player;
	icon.hasChargingInterface = true;
	icon.chargeStartTurn = magic::CurrentTurn(); // the game turn
	if (ValidForPutFullyChargedSeedInHand(iconEntity, player))
	{
		PutFullyChargedSeedInHand(iconEntity, player);
	}
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: icon {} starts charging (pu {}), store {:.0f}",
		                   static_cast<uint32_t>(iconEntity), powerUp, IconOf(iconEntity).chantStore);
	}
	return true;
}

bool icon::ValidForStartCharge(entt::entity iconEntity, PlayerNames player, int powerUp, bool requireChants)
{
	const auto& icon = IconOf(iconEntity);
	if (icon.charging || GetHeldSpellSeed(iconEntity) != entt::null)
	{
		return false;
	}
	if (!magic::players::IsMagicTypeEnabled(player, MagicTypeOf(iconEntity, powerUp)))
	{
		return false;
	}
	if (HasSite(icon))
	{
		if (!requireChants || site::TotalChantsAvailable(Registry().Get<const WorshipSite>(icon.site)) > 0.0f)
		{
			return true;
		}
	}
	return icon.chantStore != 0.0f;
}

bool icon::ValidForRequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants)
{
	// The icon must be functional: built when its site is built (always without a site). (pending) the availability
	// and life parts of that test
	if (IsIcon(icon) && HasSite(IconOf(icon)) && !ecs::abodes::IsBuilt(IconOf(icon).site))
	{
		return false;
	}
	if (ValidForPutFullyChargedSeedInHand(icon, player))
	{
		return true;
	}
	return ValidForStartCharge(icon, player, powerUp, requireChants);
}

bool icon::RequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants)
{
	if (ValidForPutFullyChargedSeedInHand(icon, player))
	{
		return PutFullyChargedSeedInHand(icon, player) == 1;
	}
	if (ValidForStartCharge(icon, player, powerUp, requireChants))
	{
		return StartCharge(icon, player, powerUp, requireChants);
	}
	return false;
}

bool icon::PowerUpValid(entt::entity icon, PlayerNames player, int powerUp)
{
	return GetHeldSpellSeed(icon) != entt::null && magic::players::IsMagicTypeEnabled(player, MagicTypeOf(icon, powerUp));
}

bool icon::SetChargingPowerUp(entt::entity iconEntity, PlayerNames player, int powerUp)
{
	if (!PowerUpValid(iconEntity, player, powerUp))
	{
		return false;
	}
	auto& icon = IconOf(iconEntity);
	icon.charging = true;
	icon.powerUp = powerUp;
	icon.chargingFor = player;
	icon.hasChargingInterface = true;
	icon.chargeStartTurn = magic::CurrentTurn();
	return true;
}

int icon::HandleValidatedTap(entt::entity icon, PlayerNames player)
{
	if (ValidForPutFullyChargedSeedInHand(icon, player))
	{
		return PutFullyChargedSeedInHand(icon, player);
	}
	if (CancelCharge(icon, player))
	{
		return 1;
	}
	if (interface::IsHandReadyForObject(player) && ValidForStartCharge(icon, player, -1, false))
	{
		return StartCharge(icon, player, -1, false) ? 1 : 0;
	}
	return 0;
}

bool icon::InterfaceValidToTap(entt::entity icon, PlayerNames player)
{
	auto& registry = Registry();
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<SpellIcon>(icon))
	{
		return false;
	}
	const auto owner = PlayerOf(icon);
	if (influence::IsInfluenceEverywhere() && owner == PlayerNames::NEUTRAL)
	{
		return true;
	}
	return owner == player;
}

int icon::InterfaceTap(entt::entity icon, PlayerNames player)
{
	const auto worshipIcon = WorshipIconOf(icon);
	if (worshipIcon == entt::null)
	{
		return 0;
	}
	if (HandleValidatedTap(worshipIcon, player) == 1 && magic::players::IsHuman(player))
	{
		PlayTapSound(PlacementIndexOf(icon));
	}
	return 1;
}

float icon::MaintainSpell(entt::entity iconEntity, float amount)
{
	auto& icon = IconOf(iconEntity);
	if (HasSite(icon))
	{
		return site::MaintainSpell(icon.site, amount);
	}
	if (amount <= icon.chantStore)
	{
		icon.chantStore -= amount;
		return amount;
	}
	const float store = icon.chantStore;
	icon.chantStore = 0.0f;
	return store;
}

void icon::AddSeed(entt::entity iconEntity, entt::entity seed)
{
	auto& registry = Registry();
	registry.Get<SpellSeed>(seed).flags |= 1u;
	auto& seeds = IconOf(iconEntity).seeds;
	if (std::ranges::find(seeds, seed) == seeds.end())
	{
		seeds.insert(seeds.begin(), seed);
	}
}

void icon::RemoveSeed(entt::entity iconEntity, entt::entity seed)
{
	auto& registry = Registry();
	if (IsIcon(iconEntity))
	{
		auto& seeds = IconOf(iconEntity).seeds;
		seeds.erase(std::remove(seeds.begin(), seeds.end(), seed), seeds.end());
	}
	if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed))
	{
		registry.Get<SpellSeed>(seed).flags &= static_cast<uint8_t>(~1u);
	}
}

void icon::UpdateChargingVisual(entt::entity iconEntity, float phase)
{
	auto& registry = Registry();
	auto& spellIcon = registry.Get<SpellIcon>(iconEntity);
	const bool charging = IsCharging(iconEntity, PlayerNames::NEUTRAL, true);
	// While charging, t = (phase - 0.1, wrapped) x 3.33; for 0 <= t < 1 the MSH_S_PULSE_IN ring on the icon's matrix,
	// scaled 3 (1 - t), alpha 255 (1 - t), raised 0.1 x twice the mesh's height, in the player's colour
	float t = -1.0f;
	if (charging)
	{
		t = (phase <= 0.1f ? phase + 1.0f : phase) - 0.1f;
		t *= 3.33f;
	}
	const bool visible = charging && t >= 0.0f && t < 1.0f;
	if (!visible)
	{
		if (spellIcon.chargeRing != entt::null && registry.Valid(spellIcon.chargeRing))
		{
			registry.Destroy(spellIcon.chargeRing);
			registry.SetDirty();
		}
		spellIcon.chargeRing = entt::null;
		return;
	}
	const auto& transform = registry.Get<const Transform>(iconEntity);
	if (spellIcon.chargeRing == entt::null || !registry.Valid(spellIcon.chargeRing))
	{
		spellIcon.chargeRing = registry.Create();
		registry.Assign<Transform>(spellIcon.chargeRing, transform);
		registry.Assign<Mesh>(spellIcon.chargeRing, resources::HashIdentifier(k_PulseMesh), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.Assign<Alpha>(spellIcon.chargeRing, 1.0f);
	}
	auto& ring = registry.Get<Transform>(spellIcon.chargeRing);
	const float scale = 3.0f * (1.0f - t);
	// (approximate): on the icon's matrix, without the 0.1 x twice-the-height raise and without the player's colour
	ring = transform;
	ring.scale = transform.scale * scale;
	registry.Get<Alpha>(spellIcon.chargeRing).value = t; // the original draws 255 t
	registry.SetDirty();
}
