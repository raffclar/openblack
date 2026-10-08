/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownCentreSpellIcon.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
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
auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

TownCentreIcons& IconsOf(entt::entity townCentre)
{
	auto& registry = Registry();
	if (auto* icons = registry.TryGet<TownCentreIcons>(townCentre); icons != nullptr)
	{
		return *icons;
	}
	return registry.Assign<TownCentreIcons>(townCentre);
}

entt::entity TownOf(entt::entity townCentre)
{
	const auto* abode = Registry().TryGet<const Abode>(townCentre);
	return abode != nullptr ? town::FromId(abode->townId) : entt::null;
}

/// The graphic shows the highest level the town holds
void UpdateTownCentrePowerUpGraphics(entt::entity icon)
{
	auto& registry = Registry();
	const auto& townIcon = registry.Get<const TownCentreSpellIcon>(icon);
	int level = -1;
	for (int pu = 0; pu < 3; ++pu)
	{
		if (townIcon.powerUps[static_cast<size_t>(pu)])
		{
			level = pu;
		}
	}
	seed_graphic::SetPowerUpType(registry.Get<const SpellIcon>(icon).graphic, level);
}

/// One power-up level on or off, then the graphic
void SetTownCentrePowerUpLevel(entt::entity icon, int powerUp, bool on)
{
	if (powerUp < 0 || powerUp > 2)
	{
		return;
	}
	Registry().Get<TownCentreSpellIcon>(icon).powerUps[static_cast<size_t>(powerUp)] = on;
	UpdateTownCentrePowerUpGraphics(icon);
}

/// A town spell icon in the slot: the entity, its seed graphic, and its place in the town's list
entt::entity CreateIcon(entt::entity townCentre, uint8_t slot, const SpecialPoint& point, SpellSeedType seed)
{
	auto& registry = Registry();
	const auto townEntity = TownOf(townCentre);
	const auto& centreTransform = registry.Get<const Transform>(townCentre);
	const auto& iconInfo = Locator::infoConstants::value().spellIcon.at(1); // "TownSpell Icon"

	// the town centre morphs with the land: the point is raised like its totem (AbodeArchetype CreateTotemStatue).
	// (inferred): an openblack adaptation, not in the original
	glm::vec3 position = point.position;
	position.y += GroundAt(position) - GroundAt(centreTransform.position);

	// its creation index is taken by ecs::object_index (AddTownSpell / OnTownCentre)
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, point.rotation, centreTransform.scale);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(iconInfo.meshId), static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& spellIcon = registry.Assign<SpellIcon>(entity);
	spellIcon.infoIndex = 1;
	spellIcon.seedType = seed;
	spellIcon.player = town::OwnerOf(townEntity); // the town's player
	registry.Assign<TownCentreSpellIcon>(entity, townEntity, townCentre, slot);

	// the seed graphic 1 m above the icon's top point
	const auto top = GetSpecialPoint(entity, 0);
	const auto graphicPosition = (top ? top->position : position) + glm::vec3(0.0f, 1.0f, 0.0f);
	const auto graphic = seed_graphic::Create(graphicPosition, seed, spellIcon.player, 1.0f, -1);
	seed_graphic::SetAutoUpdate(graphic, false);
	registry.Get<SpellIcon>(entity).graphic = graphic;
	UpdateTownCentrePowerUpGraphics(entity);

	// at the head of the town's list; the town's worship site gets an icon of the seed
	if (townEntity != entt::null)
	{
		if (auto* magic = registry.TryGet<TownMagic>(townEntity); magic != nullptr)
		{
			magic->spellIcons.insert(magic->spellIcons.begin(), entity);
			if (magic->worshipSite != entt::null)
			{
				site::AddSpellIconIfNecessary(magic->worshipSite, seed);
			}
		}
	}
	return entity;
}

/// The icon leaves the town and is destroyed
void DeleteIcon(entt::entity icon)
{
	auto& registry = Registry();
	const auto& townIcon = registry.Get<const TownCentreSpellIcon>(icon);
	const auto seed = registry.Get<const SpellIcon>(icon).seedType;
	if (townIcon.town != entt::null && registry.Valid(townIcon.town))
	{
		// out of the town's list
		if (auto* magic = registry.TryGet<TownMagic>(townIcon.town); magic != nullptr)
		{
			auto& icons = magic->spellIcons;
			icons.erase(std::remove(icons.begin(), icons.end(), icon), icons.end());
			if (magic->worshipSite != entt::null)
			{
				site::RemoveSpellIconIfUnheld(magic->worshipSite, seed);
			}
		}
	}
	// out of the map, then the graphic
	ecs::map_cells::RemoveMapObject(icon);
	seed_graphic::Delete(registry.Get<const SpellIcon>(icon).graphic);
	registry.Destroy(icon);
	registry.SetDirty();
}
} // namespace

void town_centre::DeleteDependants(entt::entity townCentre)
{
	if (Registry().TryGet<TownCentreIcons>(townCentre) == nullptr)
	{
		return;
	}
	for (auto& icon : IconsOf(townCentre).icons)
	{
		if (icon == entt::null)
		{
			continue;
		}
		const auto removed = icon;
		icon = entt::null;
		if (auto* townIcon = Registry().TryGet<TownCentreSpellIcon>(removed); townIcon != nullptr)
		{
			townIcon->townCentre = entt::null;
			DeleteIcon(removed);
		}
	}
}

bool town_centre::AddSpell(entt::entity townCentre, SpellSeedType seed)
{
	auto& icons = IconsOf(townCentre).icons;
	if (FindSpellIcon(townCentre, seed) != entt::null)
	{
		return false;
	}
	for (size_t slot = 0; slot < icons.size(); ++slot)
	{
		if (icons[slot] != entt::null)
		{
			continue;
		}
		// the first free slot's special point and angle
		const auto point = GetSpecialPoint(townCentre, static_cast<int>(slot));
		if (!point)
		{
			if (trace::Enabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Worship: town centre {} has no special point {} (its mesh has {} extra metrics)",
				                   static_cast<uint32_t>(townCentre), slot, ExtraMetricCount(townCentre));
			}
			// the original's loop goes on, but it always tries the first free slot again, so every later try fails too
			// and AddSpell returns false
			return false;
		}
		const auto icon = CreateIcon(townCentre, static_cast<uint8_t>(slot), *point, seed);
		IconsOf(townCentre).icons[slot] = icon;
		return true;
	}
	return false;
}

void town_centre::RemoveSpell(entt::entity townCentre, SpellSeedType seed)
{
	auto& icons = IconsOf(townCentre).icons;
	for (auto& icon : icons)
	{
		if (icon != entt::null && Registry().Get<const SpellIcon>(icon).seedType == seed)
		{
			const auto removed = icon;
			icon = entt::null;
			Registry().Get<TownCentreSpellIcon>(removed).townCentre = entt::null;
			DeleteIcon(removed);
		}
	}
}

void town_centre::AddPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp)
{
	if (const auto icon = FindSpellIcon(townCentre, seed); icon != entt::null)
	{
		SetTownCentrePowerUpLevel(icon, powerUp, true);
	}
}

void town_centre::ClearPowerUp(entt::entity townCentre, SpellSeedType seed, int powerUp)
{
	if (const auto icon = FindSpellIcon(townCentre, seed); icon != entt::null)
	{
		SetTownCentrePowerUpLevel(icon, powerUp, false);
	}
}

entt::entity town_centre::FindSpellIcon(entt::entity townCentre, SpellSeedType seed)
{
	for (const auto icon : IconsOf(townCentre).icons)
	{
		if (icon != entt::null && Registry().Get<const SpellIcon>(icon).seedType == seed)
		{
			return icon;
		}
	}
	return entt::null;
}

int town_centre::SpellCount(entt::entity townCentre)
{
	const auto& icons = IconsOf(townCentre).icons;
	return static_cast<int>(std::ranges::count_if(icons, [](entt::entity icon) { return icon != entt::null; }));
}

void town_centre::MakeFunctional(entt::entity townCentre)
{
	const auto townEntity = TownOf(townCentre);
	if (townEntity == entt::null)
	{
		return;
	}
	const auto& tables = Locator::infoConstants::value();
	for (size_t m = 0; m < magic::k_MagicTypeCount; ++m)
	{
		const auto type = static_cast<MagicType>(m);
		if (!town::IsMagicTypeHeld(townEntity, type))
		{
			continue;
		}
		const auto seed = magic::GetFirstSpellSeedForMagicType(tables, type);
		if (static_cast<int>(seed) < 0)
		{
			continue;
		}
		AddSpell(townCentre, seed);
		const int powerUp = magic::GetPowerUpFromMagicType(magic::GetSpellSeedInfo(tables, seed), type);
		if (powerUp != -1)
		{
			AddPowerUp(townCentre, seed, powerUp);
		}
	}
	if (const auto* magic = Registry().TryGet<const TownMagic>(townEntity);
	    magic != nullptr && magic->worshipSite != entt::null)
	{
		site::AddTownSpells(magic->worshipSite, townEntity);
	}
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: town centre {} functional, {} spell icons",
		                   static_cast<uint32_t>(townCentre), SpellCount(townCentre));
	}
}
