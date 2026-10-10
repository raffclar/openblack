/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AbodeKnockSystem.h"

#include <array>
#include <utility>

#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "ECS/AbodeKnock.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The knocking sounds on a roof, in the order knocks take them
constexpr std::array<audio::SoundId, ecs::abode_knock::k_KnockSounds> k_KnockSounds {
    audio::SoundId::G_KnockRoofMulti_01_1, audio::SoundId::G_KnockRoofMulti_02_1, audio::SoundId::G_KnockRoofMulti_03_1,
    audio::SoundId::G_KnockRoofMulti_01_2, audio::SoundId::G_KnockRoofMulti_02_2, audio::SoundId::G_KnockRoofMulti_03_2,
    audio::SoundId::G_KnockRoofMulti_01_3, audio::SoundId::G_KnockRoofMulti_02_3, audio::SoundId::G_KnockRoofMulti_03_3,
};
} // namespace

bool AbodeKnockSystem::Tap(entt::entity abodeEntity, glm::vec3 handPoint, bool ownHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.TryGet<const Abode>(abodeEntity);
	const auto* info = ecs::world_objects::AbodeInfoOf(abodeEntity);
	if (abode == nullptr || info == nullptr)
	{
		return false;
	}
	const auto* progress = registry.TryGet<const BuildProgress>(abodeEntity);
	if (!ecs::abode_knock::IsTappable(info->abodeType, progress != nullptr ? progress->built : 1.0f))
	{
		return false;
	}

	// Every house of the building's town shows how many live in it
	const auto& towns = registry.Context().towns;
	const auto town = towns.find(abode->townId);
	_knockedTown = town != towns.end() && registry.Valid(town->second) ? town->second : entt::null;
	_readoutMs = ecs::abode_knock::Knock(_readoutMs);

	// Everyone inside comes out, whoever's town it is
	const auto inhabitants = abode->inhabitants;
	for (const auto villager : inhabitants)
	{
		ecs::villager_home::SetStateWhenTappedOnAbode(villager);
	}

	if (!ecs::abode_knock::KnocksBack(info->abodeType))
	{
		return true;
	}
	if (ownHand)
	{
		_handKnock = true;
	}
	// Every player hears it, at the hand
	if (Locator::audio::has_value())
	{
		audio::PlayGameSoundEffect(static_cast<entt::id_type>(k_KnockSounds.at(_knockSound)), handPoint);
	}
	_knockSound = ecs::abode_knock::NextKnockSound(_knockSound);
	return true;
}

void AbodeKnockSystem::Update(std::chrono::milliseconds frame)
{
	// Nothing changes while the read-out isn't showing
	if (_readoutMs == 0)
	{
		return;
	}
	_readoutMs = ecs::abode_knock::Tick(_readoutMs, static_cast<uint32_t>(frame.count()));
	_readoutScale = ecs::abode_knock::ReadoutScale(_readoutMs);
}

std::optional<entt::entity> AbodeKnockSystem::GetReadoutTown() const
{
	if (_readoutScale <= 0.0f || _knockedTown == entt::null)
	{
		return std::nullopt;
	}
	return _knockedTown;
}

bool AbodeKnockSystem::TakeHandKnock()
{
	return std::exchange(_handKnock, false);
}
