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

namespace openblack::audio::atmos_banks
{

/// The ambient banks: the audio's atmos part and the audio library's mixer of the banks. Each of the 14 ATMOS_TYPE
/// banks of Audio\SFX\Atmos (the sound groups "ocean.sad"...) has a volume 0..127 that follows the sound map's volume
/// of its type; its loops (atmos frequency 0) play 2D while that is not 0, fading in by 5 a turn, and one loose sample
/// (frequency f > 0, next time = counter + 4f + U[0, 12f] turns) at most starts a turn, from the head of one queue for
/// all banks, at a random point beside the listener.

/// The atmos of the audio's game turn (audio::ProcessTurn calls them in its order, once per game turn after the sound
/// map's update, when the game is not paused, past turn 5 and with the audio active): UpdateBanks sets the targets (the
/// sound map's volumes, all 0 inside the citadel) and runs ProcessAtmosBanks (step 0.02 / 0.04 towards them, group by
/// the alignment, bank volume = current * 127); then the channels and the listener; then Mix, the mixer's pass, unless
/// a video plays. The first UpdateBanks registers the banks.
void UpdateBanks();
void Mix();

/// The camera alignment (-1 evil .. 1 good), which ProcessAtmosBanks compares with -0.6 to put every bank in group 1 or
/// 2: GameQueries::cameraAlignment (0 when unset)
[[nodiscard]] float Alignment();

/// The group of every bank for that alignment, 1 when it is above the double -0.6, else 2 (a NaN too)
[[nodiscard]] uint32_t GroupFor(float alignment);

/// The loops stop and every atmos channel (the loops' and the loose samples') is stopped; the other samples play on.
/// The end of a turn calls it instead of the audio's game turn while the game is paused or in the first 5 turns.
void Silence();

/// A new map (the audio's reset): Silence, the atmos channels stopped; the banks keep their volumes, as in the
/// original
void Clear();

} // namespace openblack::audio::atmos_banks
