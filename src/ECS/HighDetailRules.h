/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <numbers>
#include <optional>
#include <string_view>

#include "3D/AllMeshes.h"

/// The rules of the villagers a script draws in high detail for its cinema, such as the opening's family: the detailed
/// models they wear, and the orders a script gives them
namespace openblack::ecs::high_detail_rules
{

/// The face a high-detail model's eyes are drawn with
enum class Face : uint8_t
{
	Man,
	Woman,
	Boy,
};

/// The detailed model a villager wearing one of the ordinary models changes into
struct DetailedModel
{
	/// The model's file in the game's misc folder
	std::string_view file;
	Face face;
};

/// Only four ordinary models have a detailed one: the northern man, woman and boy of the opening, and the creature
/// trainer. Anyone else is drawn in high detail in its own model.
[[nodiscard]] constexpr std::optional<DetailedModel> DetailedModelFor(MeshId model)
{
	switch (model)
	{
	case MeshId::PersonNorseMaleA1:
		return DetailedModel {.file = "Intro/nors_man.l3d", .face = Face::Man};
	case MeshId::PersonNorseFemaleA1:
		return DetailedModel {.file = "Intro/nors_woman.l3d", .face = Face::Woman};
	case MeshId::PersonBoyWhite1:
		return DetailedModel {.file = "Intro/nors_boy.l3d", .face = Face::Boy};
	case MeshId::PersonAnimalTrainer:
		// The trainer is given the woman's face
		return DetailedModel {.file = "sable.l3d", .face = Face::Woman};
	default:
		return std::nullopt;
	}
}

/// The orders a script gives a thing drawn in high detail
enum class ThingSpecial : int32_t
{
	FollowIntroHand = 7,
	EaseTurning = 8,
	FaceMirrored = 9,
	AddQuarterTurn = 16,
	TakeQuarterTurn = 17,
	ReleaseIntroSpecials = 18,
	DrawnObjectSpecial = 19,
};

/// What a high-detail villager's drawing is told
struct DrawOrders
{
	/// Drawn held in the opening's hand while the hand holds someone
	bool followIntroHand {false};
	/// Its turning and the change between its clips are shown at once rather than eased
	bool turnAtOnce {false};
};

/// What an order changes
struct ThingSpecialResult
{
	DrawOrders orders;
	/// The way it faces afterwards, when the order turns it
	std::optional<float> yAngle;
};

/// A quarter turn, which the quarter-turn orders add to or take from the way it faces
constexpr float k_QuarterTurn = std::numbers::pi_v<float> / 2.0f;

/// An order given to a high-detail villager facing `yAngle`. Following the hand ignores `on`; easing its turning is
/// switched by `on`; mirroring and the quarter turns turn it and set it to turn at once.
[[nodiscard]] constexpr ThingSpecialResult ApplyThingSpecial(ThingSpecial special, bool on, DrawOrders orders, float yAngle)
{
	ThingSpecialResult result {.orders = orders};
	switch (special)
	{
	case ThingSpecial::FollowIntroHand:
		result.orders.followIntroHand = true;
		break;
	case ThingSpecial::EaseTurning:
		result.orders.turnAtOnce = !on;
		break;
	case ThingSpecial::FaceMirrored:
		result.yAngle = -yAngle;
		result.orders.turnAtOnce = true;
		break;
	case ThingSpecial::AddQuarterTurn:
		result.yAngle = yAngle + k_QuarterTurn;
		result.orders.turnAtOnce = true;
		break;
	case ThingSpecial::TakeQuarterTurn:
		result.yAngle = yAngle - k_QuarterTurn;
		result.orders.turnAtOnce = true;
		break;
	default:
		break;
	}
	return result;
}

/// The drawn high-detail villagers stay so only while a script holds the cinema bars
[[nodiscard]] constexpr bool KeepsHighDetail(bool wideScreenOn, uint32_t wideScreenOwner)
{
	return wideScreenOn && wideScreenOwner != 0;
}

} // namespace openblack::ecs::high_detail_rules
