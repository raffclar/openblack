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

#include <bit>
#include <optional>

#include "ECS/Components/Scaffold.h"

/// How the phantom building of a scaffold looks in each frame: in the hand of the local player, the phantom grows,
/// becomes small and turns. On a building site, the phantom of the building fades as the building grows, and the
/// scaffold becomes small. These rules use only the frames: the game turns do not read them.
namespace openblack::ecs::scaffold_ghost
{

/// The alpha of the phantom in the hand: two thirds of the full alpha of the light of the land
inline constexpr uint8_t k_HeldAlpha = 170;
/// The size that the phantom changes for each millisecond of its change, as a part of its full size
inline constexpr float k_SizePerMs = std::bit_cast<float>(0x3B23D70AU); // 0.0025
/// The part of the turn of a game turn that the phantom turns for each millisecond into the turn
inline constexpr float k_TurnPerMs = std::bit_cast<float>(0x3C23D70AU); // 0.01
/// The scaffold on a site becomes small one and a half times as quickly as the building grows
inline constexpr float k_ShrinkOnSite = 1.5f;
/// The smallest size of a scaffold on a site, as a part of its full size
inline constexpr float k_SmallestOnSite = std::bit_cast<float>(0x3C23D70AU); // 0.01
/// The full alpha of the light of the land, which the phantom on a site divides
inline constexpr float k_LandAlpha = 255.0f;
/// The phantom on a site fades as the building grows: the land's alpha divided by 15 times the built part plus 3
inline constexpr float k_SiteFade = 15.0f;
inline constexpr float k_SiteFadeStart = 3.0f;
/// The alpha reference of every material of the phantom on a site, in place of the reference of the material
inline constexpr uint8_t k_SiteAlphaRef = 10;
/// The pitch of the sound when the phantom grows: 110 percent less 3 percent for each abode number
inline constexpr int32_t k_GrowPitch = 110;
inline constexpr int32_t k_GrowPitchStep = 3;
/// The pitch of the sound for a building with no abode number
inline constexpr int32_t k_PlainPitch = 100;

/// The sound that a frame starts for the phantom in the hand
enum class GhostSound : uint8_t
{
	None,
	/// The phantom starts to become small
	Shrinks,
	/// The phantom starts to grow
	Grows,
};

/// The sounds of the phantom that played. Each sound plays one time when the phantom starts to change, and the other
/// sound can then play again.
struct SoundsPlayed
{
	bool shrinks {false};
	bool grows {false};
};

/// The size and the turn of the phantom in the hand in a frame
struct HeldPose
{
	float scale {1.0f};
	float yAngle {0.0f};
};

/// The phantom in the hand in a frame: none when it shows no building now, and the sound that starts
struct HeldLook
{
	std::optional<HeldPose> pose;
	GhostSound sound {GhostSound::None};
};

/// The phantom in the hand, `msIntoTurn` milliseconds after the last game turn. The phantom turns by
/// `turnPerGameTurn` in each game turn. The phantom must show an offer.
[[nodiscard]] HeldLook Held(const components::ScaffoldPhantom& phantom, int32_t msIntoTurn, float turnPerGameTurn,
                            SoundsPlayed& played);

/// The pitch in percent of the sound when a phantom of a building with an abode number grows
[[nodiscard]] int32_t GrowPitchPercent(int32_t abodeNumber);

/// The alpha of the phantom on a site whose building has the built part `built`, 0 to 1
[[nodiscard]] uint8_t SiteAlpha(float built);

/// The alpha reference that the phantom uses in place of the references of its materials: the phantom on a site uses
/// `k_SiteAlphaRef`, and the phantom in the hand uses the references of its materials
[[nodiscard]] constexpr std::optional<uint8_t> SeeThroughRef(bool inHand)
{
	return inHand ? std::nullopt : std::optional<uint8_t>(k_SiteAlphaRef);
}

/// The size of a scaffold on a site whose building has the built part `built`. The phantom of the scaffold had the size
/// `scale` when the site started.
[[nodiscard]] float ScaleOnSite(float built, float scale);

} // namespace openblack::ecs::scaffold_ghost
