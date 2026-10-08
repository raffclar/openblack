/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Alignment.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "Audio/Services/Guidance.h"
#include "Camera/Camera.h"
#include "ECS/Abodes.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "EffectValues.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs::effects;

namespace
{
/// The alignment info's row `effect` (0 burn .. 4 fly away), column `type`
float AlignmentFactor(const InfoConstants& info, size_t effect, AlignmentType type)
{
	if (effect >= info.alignment.size())
	{
		return 0.0f;
	}
	const auto& row = info.alignment[effect];
	switch (type)
	{
	case AlignmentType::AnimalNice:
		return row.animalNice;
	case AlignmentType::AnimalNasty:
		return row.animalNasty;
	case AlignmentType::Creature:
		return row.creature;
	case AlignmentType::Priest:
		return row.priest;
	case AlignmentType::Skeleton:
		return row.skeleton;
	case AlignmentType::Villager:
		return row.villager;
	case AlignmentType::Building:
		return row.building;
	case AlignmentType::Plant:
		return row.plant;
	case AlignmentType::Field:
		return row.field;
	case AlignmentType::Feature:
		return row.feature;
	case AlignmentType::MobileObject:
		return row.mobileObject;
	case AlignmentType::Land:
		return row.land;
	case AlignmentType::Script:
		return row.script;
	case AlignmentType::Unimportant:
		return row.unimportant;
	}
	return 0.0f;
}
bool Trace()
{
	return std::getenv("OPENBLACK_ALIGNMENT_TRACE") != nullptr;
}
} // namespace

float alignment::ScaleChange(const ecs::components::Alignment& alignment, float change)
{
	// the same sign as the alignment (0 counts as positive) is damped, the opposite sign boosted
	const float a = std::abs(alignment.value * 0.5f);
	const bool sameSign = (alignment.value < 0.0f) == (change < 0.0f);
	return sameSign ? (1.0f - a) * change : (a + 1.0f) * change;
}

void alignment::Update(ecs::components::Alignment& alignment, entt::entity object, const EffectValues& values, float lifeBefore)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float lifeChange = lifeBefore - life::LifeOf(object);
	if (lifeChange == 0.0f)
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const GObjectInfo* objectInfo = physics::PhysicsObjects::ObjectInfo(object);
	// an abode's (a field's) info is its GAbodeInfo (abodes::InfoOf), which PhysicsObjects does not keep
	if (objectInfo == nullptr)
	{
		objectInfo = abodes::InfoOf(object);
	}
	// the info's alignmentType ((inferred) Unimportant without an info)
	const auto type = objectInfo != nullptr ? objectInfo->alignmentType : AlignmentType::Unimportant;
	const float k = std::abs(lifeChange) + info.player.applyEffectAlignmentChangeAddition;
	constexpr auto k_First = static_cast<size_t>(EffectValues::Number::Crush);
	constexpr auto k_Last = static_cast<size_t>(EffectValues::Number::FlyAway);
	for (size_t i = k_First; i <= k_Last; ++i)
	{
		alignment.pending += ScaleChange(alignment, values.numbers[i] * AlignmentFactor(info, i, type) * k);
	}
	constexpr auto k_Burn = static_cast<size_t>(EffectValues::Number::Burn);
	const float burn = ConvertTemperatureToDamage(object, values.numbers[k_Burn]);
	alignment.pending += ScaleChange(alignment, burn * AlignmentFactor(info, k_Burn, type) * k);
}

ecs::components::Alignment& alignment::Of(PlayerNames player)
{
	return magic::players::AlignmentOf(player);
}

float alignment::Get(PlayerNames player)
{
	return Of(player).value;
}

void alignment::SetClamped(PlayerNames player, float value)
{
	Of(player).value = std::clamp(value, -1.0f, 1.0f);
}

void alignment::AddClamped(PlayerNames player, float change)
{
	auto& alignment = Of(player);
	alignment.value = std::clamp(alignment.value + change, -1.0f, 1.0f);
}

void alignment::UpdateForTree(PlayerNames player, bool good)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float change = Locator::infoConstants::value().player.treePullPutAlignmentChange;
	auto& alignment = Of(player);
	const float weighed = ScaleChange(alignment, good ? change : -change);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} tree {} {:+.5f} (pending {:+.5f}, alignment {:+.4f})",
		                   static_cast<int>(player), good ? "planted" : "uprooted", weighed, alignment.pending,
		                   alignment.value);
	}
}

void alignment::UpdateForResource(PlayerNames player, entt::entity abode, int32_t amount, float change)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// the abode's town is not tested; there is a single town info
	static_cast<void>(abode);
	const auto& town = Locator::infoConstants::value().town;
	const float k = amount > 0 ? town.giveResourceAligmnetChangeMultiplier : town.takeResourceAligmnetChangeMultiplier;
	auto& alignment = Of(player);
	const float weighed = ScaleChange(alignment, change * k);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} resource {:+d} {:+.5f} (pending {:+.5f})",
		                   static_cast<int>(player), amount, weighed, alignment.pending);
	}
}

float alignment::DeathAlignmentChange(const GPlayerInfo& info, DeathReason reason, bool child, bool animal)
{
	// no player -> nothing (the caller's); the player info's dealthReason[r]
	const auto r = std::min<size_t>(static_cast<size_t>(reason), info.dealthReason.size() - 1);
	float change = info.dealthReason.at(r);
	// a child: doubled (v + v)
	if (child)
	{
		change = change + change;
	}
	// an animal: halved
	if (animal)
	{
		change *= 0.5f;
	}
	return change;
}

void alignment::UpdateForDeath(PlayerNames owner, DeathReason reason, bool child, bool animal)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// pending += the change
	const float change = DeathAlignmentChange(Locator::infoConstants::value().player, reason, child, animal);
	auto& alignment = Of(owner);
	alignment.pending += change;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} death {} {:+.5f} (pending {:+.5f}, alignment {:+.4f})",
		                   static_cast<int>(owner), static_cast<int>(reason), change, alignment.pending, alignment.value);
	}
}

void alignment::ProcessForPlayer(PlayerNames player)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	auto& alignment = Of(player);
	const float cap = Locator::infoConstants::value().player.maxAlignmentChangePerGameTurn;
	// for the local interface's player ((inferred) openblack's local player is PLAYER_ONE, as Game.cpp's
	// localPlayerNumber), every turn and before the fold, even with nothing pending (the advisors' running sum decays):
	// the guidance's alignment process with the maximum change per game turn x the pending change, not clamped yet;
	// the guidance reads the alignment before this turn's change and the same maximum
	if (player == PlayerNames::PLAYER_ONE && magic::players::EntityOf(player) != entt::null)
	{
		audio::guidance::UpdateAlignmentRemarks(cap * alignment.pending, alignment.value, cap);
	}
	// with nothing pending its AddClamped(0) changes nothing
	if (alignment.pending == 0.0f)
	{
		return;
	}
	const float change = cap * std::clamp(alignment.pending, -1.0f, 1.0f);
	AddClamped(player, change);
	alignment.pending = 0.0f;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} {:+.5f} -> {:+.4f}", static_cast<int>(player), change,
		                   alignment.value);
	}
}

void alignment::ProcessPlayers()
{
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		ProcessForPlayer(static_cast<PlayerNames>(i));
	}
}

namespace
{
/// The sky stores (1 - x) x 2 of it and starts at 1 (neutral) -> x = 0.5
struct InterfaceAlignmentState
{
	float value {0.5f};
};

/// The alignment the interface shows (Locator::handMagicState)
InterfaceAlignmentState& InterfaceAlignment()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("ecs::effects::alignment: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<InterfaceAlignmentState>();
}
} // namespace

PlayerNames alignment::MostInfluentialPlayer(const glm::vec3& position)
{
	// the neutral player first, best 0; then every player from the first one on
	PlayerNames best = PlayerNames::NEUTRAL;
	float bestInfluence = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		if (magic::players::EntityOf(player) == entt::null)
		{
			continue;
		}
		const float influence = influence::CalculatePlayerInfluence(player, position, influence::CalcType::Default, true);
		// only a strictly greater influence takes it
		if (influence > bestInfluence)
		{
			bestInfluence = influence;
			best = player;
		}
	}
	return best;
}

float alignment::LandAlignmentAt(const glm::vec3& position)
{
	// 0 to start with, then for every player add influence x the player's alignment value
	float sum = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		// the player walk covers only the seven player slots: the neutral player is not one of them
		if (player == PlayerNames::NEUTRAL || magic::players::EntityOf(player) == entt::null)
		{
			continue;
		}
		sum += influence::CalculatePlayerInfluence(player, position, influence::CalcType::Default, true) * Get(player);
	}
	// below -1 -> -1; above 1 -> 1
	return std::clamp(sum, -1.0f, 1.0f);
}

float alignment::InterfaceAlignmentAt(const glm::vec3& position)
{
	// that player's alignment value, + 1, x 0.5; the sky clamps it
	const float x = (Get(MostInfluentialPlayer(position)) + 1.0f) * 0.5f;
	return std::clamp(x, 0.0f, 1.0f);
}

float alignment::GetInterfaceAlignment()
{
	return InterfaceAlignment().value;
}

void alignment::UpdateInterfaceAlignment()
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	InterfaceAlignment().value = InterfaceAlignmentAt(Locator::camera::value().GetOrigin());
}

void alignment::ResetInterfaceAlignment()
{
	InterfaceAlignment().value = 0.5f;
}
