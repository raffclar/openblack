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

#include <filesystem>
#include <queue>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>

namespace openblack::audio
{
/// OpenAL's names of a source and a buffer (ALuint), made only by the device (Device.h)
using SourceId = uint32_t;
using BufferId = uint32_t;

enum class SoundId : entt::id_type
{
	// InGame.sad
	G_AcknowledgeCommand = entt::hashed_string("InGame.sad/1").value(),
	G_Fire_01 = entt::hashed_string("InGame.sad/2").value(),
	G_HandGesture_02 = entt::hashed_string("InGame.sad/3").value(),
	G_HandGrabLand_01 = entt::hashed_string("InGame.sad/4").value(),
	G_HandGrabLand_02 = entt::hashed_string("InGame.sad/5").value(),
	G_HandGrabLand_03 = entt::hashed_string("InGame.sad/6").value(),
	G_HandGrabLand_04 = entt::hashed_string("InGame.sad/7").value(),
	G_HandGrabLand_05 = entt::hashed_string("InGame.sad/8").value(),
	G_HandGrabLand_06 = entt::hashed_string("InGame.sad/9").value(),
	G_PickUpObject = entt::hashed_string("InGame.sad/10").value(),
	G_TotumMove = entt::hashed_string("InGame.sad/11").value(),
	G_WaterFlow = entt::hashed_string("InGame.sad/12").value(),
	G_windmill = entt::hashed_string("InGame.sad/13").value(),
	H_ImpressedGroup_01 = entt::hashed_string("InGame.sad/14").value(),
	H_ImpressedGroup_02 = entt::hashed_string("InGame.sad/15").value(),
	H_ImpressedGroup_03 = entt::hashed_string("InGame.sad/16").value(),
	H_LoseBeliefGroup_01 = entt::hashed_string("InGame.sad/17").value(),
	H_LoseBeliefGroup_02 = entt::hashed_string("InGame.sad/18").value(),
	H_LoseBeliefGroup_03 = entt::hashed_string("InGame.sad/19").value(),
	G_BabyCry_01 = entt::hashed_string("InGame.sad/20").value(),
	G_BabyCry_02 = entt::hashed_string("InGame.sad/21").value(),
	G_BabyCry_03 = entt::hashed_string("InGame.sad/22").value(),
	G_BabyCry_04 = entt::hashed_string("InGame.sad/23").value(),
	G_BabyCry_05 = entt::hashed_string("InGame.sad/24").value(),
	G_BabyCry_06 = entt::hashed_string("InGame.sad/25").value(),
	G_BabyCry_07 = entt::hashed_string("InGame.sad/26").value(),
	G_BabyCry_08 = entt::hashed_string("InGame.sad/27").value(),
	G_BabyCry_09 = entt::hashed_string("InGame.sad/28").value(),
	G_BabyCry_10 = entt::hashed_string("InGame.sad/29").value(),
	G_VillageBell = entt::hashed_string("InGame.sad/30").value(),
	G_TreeFall_01 = entt::hashed_string("InGame.sad/31").value(),
	G_TreeBreak_01_1 = entt::hashed_string("InGame.sad/32").value(),
	G_TreeBreak_02_1 = entt::hashed_string("InGame.sad/33").value(),
	G_TreeBreak_03_1 = entt::hashed_string("InGame.sad/34").value(),
	G_SpellPowerUpBand = entt::hashed_string("InGame.sad/35").value(),
	G_SpellGestureRecognise = entt::hashed_string("InGame.sad/36").value(),
	G_SpellCastFailure = entt::hashed_string("InGame.sad/37").value(),
	G_SpellTeleportEnergiseArrive = entt::hashed_string("InGame.sad/38").value(),
	G_SpellTeleportEnergiseGo = entt::hashed_string("InGame.sad/39").value(),
	G_RewardSting = entt::hashed_string("InGame.sad/40").value(),
	G_OpenChest = entt::hashed_string("InGame.sad/41").value(),
	G_ClickOnSpell_01 = entt::hashed_string("InGame.sad/42").value(),
	G_OutOfBounds = entt::hashed_string("InGame.sad/43").value(),
	G_PickUpFood = entt::hashed_string("InGame.sad/44").value(),
	G_Heartbeat_01 = entt::hashed_string("InGame.sad/45").value(),
	G_Woosh_01 = entt::hashed_string("InGame.sad/46").value(),
	G_Woosh_02 = entt::hashed_string("InGame.sad/47").value(),
	G_Woosh_03 = entt::hashed_string("InGame.sad/48").value(),
	G_Woosh_04 = entt::hashed_string("InGame.sad/49").value(),
	G_ScaffoldDisappear_01 = entt::hashed_string("InGame.sad/50").value(),
	G_ScaffoldAppear_01 = entt::hashed_string("InGame.sad/51").value(),
	G_HandThroughInfluence_01 = entt::hashed_string("InGame.sad/52").value(),
	G_Steam_01 = entt::hashed_string("InGame.sad/53").value(),
	G_ScrollSqueak_01 = entt::hashed_string("InGame.sad/54").value(),
	G_ScrollSqueak_02 = entt::hashed_string("InGame.sad/55").value(),
	G_ScrollSqueak_03 = entt::hashed_string("InGame.sad/56").value(),
	G_ScrollSqueak_04 = entt::hashed_string("InGame.sad/57").value(),
	G_ScrollSqueak_05 = entt::hashed_string("InGame.sad/58").value(),
	G_ScrollSqueak_06 = entt::hashed_string("InGame.sad/59").value(),
	G_CitadelDoorOpen_01 = entt::hashed_string("InGame.sad/60").value(),
	G_CitadelDoorClose_02 = entt::hashed_string("InGame.sad/61").value(),
	G_CitadelButtonUp_01 = entt::hashed_string("InGame.sad/62").value(),
	G_CitadelButtonDown_01 = entt::hashed_string("InGame.sad/63").value(),
	G_FireballPast_01 = entt::hashed_string("InGame.sad/64").value(),
	G_FireballPast_02 = entt::hashed_string("InGame.sad/65").value(),
	G_FireballPast_03 = entt::hashed_string("InGame.sad/66").value(),
	G_FireballPast_04 = entt::hashed_string("InGame.sad/67").value(),
	G_FireballPast_05 = entt::hashed_string("InGame.sad/68").value(),
	G_RockPast_01 = entt::hashed_string("InGame.sad/69").value(),
	G_RockPast_02 = entt::hashed_string("InGame.sad/70").value(),
	G_RockPast_03 = entt::hashed_string("InGame.sad/71").value(),
	G_RockPast_04 = entt::hashed_string("InGame.sad/72").value(),
	G_RockPast_05 = entt::hashed_string("InGame.sad/73").value(),
	G_Workshop = entt::hashed_string("InGame.sad/74").value(),
	G_PileFood_01 = entt::hashed_string("InGame.sad/75").value(),
	G_PileFood_02 = entt::hashed_string("InGame.sad/76").value(),
	G_PileFoodSmall_01 = entt::hashed_string("InGame.sad/77").value(),
	G_PileFoodSmall_02 = entt::hashed_string("InGame.sad/78").value(),
	G_PileFoodSmall_03 = entt::hashed_string("InGame.sad/79").value(),
	G_PileFoodSmall_04 = entt::hashed_string("InGame.sad/80").value(),
	G_PileFoodSmall_05 = entt::hashed_string("InGame.sad/81").value(),
	G_PileFoodSmall_06 = entt::hashed_string("InGame.sad/82").value(),
	G_PlantTree_01 = entt::hashed_string("InGame.sad/83").value(),
	G_PlantTree_02 = entt::hashed_string("InGame.sad/84").value(),
	G_PlantTree_03 = entt::hashed_string("InGame.sad/85").value(),
	G_PileWood_01 = entt::hashed_string("InGame.sad/86").value(),
	G_PileWood_02 = entt::hashed_string("InGame.sad/87").value(),
	G_PileWood_03 = entt::hashed_string("InGame.sad/88").value(),
	G_PileWood_04 = entt::hashed_string("InGame.sad/89").value(),
	G_PileWood_05 = entt::hashed_string("InGame.sad/90").value(),
	G_PileWood_06 = entt::hashed_string("InGame.sad/91").value(),
	G_PileWoodSmall_01 = entt::hashed_string("InGame.sad/92").value(),
	G_PileWoodSmall_02 = entt::hashed_string("InGame.sad/93").value(),
	G_PileWoodSmall_03 = entt::hashed_string("InGame.sad/94").value(),
	G_PileWoodSmall_04 = entt::hashed_string("InGame.sad/95").value(),
	G_PileWoodSmall_05 = entt::hashed_string("InGame.sad/96").value(),
	G_PileWoodSmall_06 = entt::hashed_string("InGame.sad/97").value(),
	G_PickUpWood = entt::hashed_string("InGame.sad/98").value(),
	G_HandInWater_01 = entt::hashed_string("InGame.sad/99").value(),
	G_HandInWater_02 = entt::hashed_string("InGame.sad/100").value(),
	G_HandInWater_03 = entt::hashed_string("InGame.sad/101").value(),
	G_HandInWater_04 = entt::hashed_string("InGame.sad/102").value(),
	G_HandInWater_05 = entt::hashed_string("InGame.sad/103").value(),
	G_HandInWater_06 = entt::hashed_string("InGame.sad/104").value(),
	G_HandInWater_07 = entt::hashed_string("InGame.sad/105").value(),
	G_HandInWater_08 = entt::hashed_string("InGame.sad/106").value(),
	G_HandInWater_09 = entt::hashed_string("InGame.sad/107").value(),
	G_HandInWater_10 = entt::hashed_string("InGame.sad/108").value(),
	G_SpellBubblePop_04 = entt::hashed_string("InGame.sad/109").value(),
	G_KnockRoofMulti_01_1 = entt::hashed_string("InGame.sad/110").value(),
	G_KnockRoofMulti_02_1 = entt::hashed_string("InGame.sad/111").value(),
	G_KnockRoofMulti_03_1 = entt::hashed_string("InGame.sad/112").value(),
	G_KnockRoofMulti_01_2 = entt::hashed_string("InGame.sad/113").value(),
	G_KnockRoofMulti_02_2 = entt::hashed_string("InGame.sad/114").value(),
	G_KnockRoofMulti_03_2 = entt::hashed_string("InGame.sad/115").value(),
	G_KnockRoofMulti_01_3 = entt::hashed_string("InGame.sad/116").value(),
	G_KnockRoofMulti_02_3 = entt::hashed_string("InGame.sad/117").value(),
	G_KnockRoofMulti_03_3 = entt::hashed_string("InGame.sad/118").value(),
	G_ShakeHand_01 = entt::hashed_string("InGame.sad/119").value(),
	G_TreeGrow_01_1 = entt::hashed_string("InGame.sad/120").value(),
	G_TreeGrow_02_1 = entt::hashed_string("InGame.sad/121").value(),
	G_TreeGrow_03_1 = entt::hashed_string("InGame.sad/122").value(),
	G_TreeGrow_04_1 = entt::hashed_string("InGame.sad/123").value(),
	G_TreeGrow_01_2 = entt::hashed_string("InGame.sad/124").value(),
	G_TreeGrow_02_2 = entt::hashed_string("InGame.sad/125").value(),
	G_TreeGrow_03_2 = entt::hashed_string("InGame.sad/126").value(),
	G_TreeGrow_04_2 = entt::hashed_string("InGame.sad/127").value(),
	G_TreeGrow_01_3 = entt::hashed_string("InGame.sad/128").value(),
	G_VirtualInfluence_04 = entt::hashed_string("InGame.sad/129").value(),
	G_RockTap_01_1 = entt::hashed_string("InGame.sad/130").value(),
	G_RockTap_02_1 = entt::hashed_string("InGame.sad/131").value(),
	G_RockTap_03_1 = entt::hashed_string("InGame.sad/132").value(),
	G_RockTap_04_1 = entt::hashed_string("InGame.sad/133").value(),
	G_ClickOnScroll_01 = entt::hashed_string("InGame.sad/134").value(),
	G_WorshipDrain_01 = entt::hashed_string("InGame.sad/135").value(),
	G_TreeBreak_01_2 = entt::hashed_string("InGame.sad/136").value(),
	G_TreeBreak_02_2 = entt::hashed_string("InGame.sad/137").value(),
	G_TreeBreak_03_2 = entt::hashed_string("InGame.sad/138").value(),
	G_RockTap_01 = entt::hashed_string("InGame.sad/139").value(),
	G_RockTap_02 = entt::hashed_string("InGame.sad/140").value(),
	G_RockTap_03 = entt::hashed_string("InGame.sad/141").value(),
	G_RockTap_04 = entt::hashed_string("InGame.sad/142").value(),
	G_SquashAnimal_01 = entt::hashed_string("InGame.sad/143").value(),
	G_SquashAnimal_02 = entt::hashed_string("InGame.sad/144").value(),
	G_SquashAnimal_03 = entt::hashed_string("InGame.sad/145").value(),
	G_SquashAnimal_04 = entt::hashed_string("InGame.sad/146").value(),
	G_Lantern_01 = entt::hashed_string("InGame.sad/147").value(),
	G_LeashAttach_01_1 = entt::hashed_string("InGame.sad/148").value(),
	G_LeashAttach_01_2 = entt::hashed_string("InGame.sad/149").value(),
	G_ScaffoldReady_01 = entt::hashed_string("InGame.sad/150").value(),
	G_ScaffoldTap_01 = entt::hashed_string("InGame.sad/151").value(),
	G_ScaffoldTap_02 = entt::hashed_string("InGame.sad/152").value(),
	G_ScaffoldTap_03 = entt::hashed_string("InGame.sad/153").value(),
	G_ScaffoldTap_04 = entt::hashed_string("InGame.sad/154").value(),
	G_TreeMulch_01 = entt::hashed_string("InGame.sad/155").value(),
	G_TreeMulch_02 = entt::hashed_string("InGame.sad/156").value(),
	G_TreeMulch_03 = entt::hashed_string("InGame.sad/157").value(),
	G_TreeMulch_04 = entt::hashed_string("InGame.sad/158").value(),
	G_MenuButton = entt::hashed_string("InGame.sad/159").value(),
	Logo = entt::hashed_string("InGame.sad/160").value(),
	G_SpriteShootGun = entt::hashed_string("InGame.sad/161").value(),
	G_SpriteKnockGlass = entt::hashed_string("InGame.sad/162").value(),
	G_SpriteSlapHead = entt::hashed_string("InGame.sad/163").value(),
	G_SpriteFart = entt::hashed_string("InGame.sad/164").value(),
	G_FireCreatureCave_01 = entt::hashed_string("InGame.sad/175").value(),
	G_WaterCreatureCave_01 = entt::hashed_string("InGame.sad/177").value(),
};

enum class AudioStatus
{
	Initial,
	Playing,
	Paused,
	Stopped
};

enum class ChannelLayout
{
	Mono,
	Stereo
};

enum class PlayType
{
	Repeat,
	Once,
	Overlap
};

class Sound
{
public:
	std::string name;
	int id;
	int sampleRate;
	int priority;
	int bitRate;
	float volume;        ///< the mixer's gain of volume127 (sample_play::QMixerGain: floor(127 v / 127) * 258 / 32767)
	int volume127 {127}; ///< the volume 0..127: the .sad's (low u16 of its volume field) with flag 0x20, else 127
	/// Which of the .sad's fields override the play options (0x1 pitch, 0x20 volume, 0x40 loops, 0x80 min, 0x100 max,
	/// 0x200 scale, 0x400 play mode)
	uint32_t overrides {0};
	/// The .sad's loops with flag 0x40 (0 once, -1 for ever)
	int loops {0};
	/// The .sad's user parameter (high u16 of the volume field): the kind PlaySoundEffect filters on (1 = not while a
	/// script holds the widescreen, 2 = also inside the citadel, 4 = not in some interface states)
	int userParam {0};
	int pitch;          ///< percent of the sample rate (100 = as recorded)
	int pitchDeviation; ///< +-percent, random at each start
	/// The sample does not start farther than this from the camera (animation sounds). PlaySoundEffect reads it raw
	/// (no flag test).
	float maxDistance {0.0f};
	/// The distance mapping of the sample's channel {min, max, scale}: the .sad's when the override flags 0x80 / 0x100 /
	/// 0x200 are set, otherwise the play options' defaults 1 / 9999 / 0.3. The mixer's gain is 1 up to min,
	/// min / (min + scale * (d - min)) up to max, 0 beyond max.
	float minDistance {1.0f};
	float mappingMaxDistance {9999.0f};
	float scale {0.3f};
	/// The .sad's clone group (u16). With play mode 3 a new sample reuses (restarts) the channel of any sample of the
	/// same bank, object and group (0 = none)
	int cloneGroup {0};
	/// The .sad's play mode when the override flag 0x400 is set, else 3: 1 = a new channel, 2 = nothing while the same
	/// sample (or group) plays, 3 = restart the same channel
	int playMode {3};
	/// The .sad's atmos group of a bank sample (u16 after the clone group: 0 any, 1 good/neutral, 2 evil)
	int atmosGroup {0};
	/// The .sad's atmos frequency (i32): -1 = not an atmos sample, 0 = the bank's loop, f > 0 = a loose sample queued
	/// again 4f + rand * 12f / 32767 turns later
	int32_t atmosFrequency {-1};
	/// The bank the sample belongs to (audio::BankId of the .sad it was read from), as the channel keeps it. 0 = not
	/// registered.
	uint16_t bank {0};
	/// The sample whose wave this one plays (clones share one, e.g. editor 1..6 -> 1)
	int wave {0};
	/// WAVEFORMATEX.wFormatTag of the RIFF wave: 1 PCM, 2 MS-ADPCM, 0x50 MPEG layer II (decoded by
	/// wave_buffers, WaveBuffers.h)
	uint16_t waveFormat {0};
	/// The loop section in frames (-1 = none). Playing passes it to the mixer when both are set and start < end
	int32_t loopStart {-1};
	int32_t loopEnd {-1};
	ChannelLayout channelLayout {ChannelLayout::Mono};
	PlayType playType {PlayType::Once};
	/// The wave's OpenAL buffer, made at the first use (wave_buffers::Get) and kept: 0 = not decoded yet
	BufferId bufferId {0};
	float duration {0.0f};
	std::vector<std::vector<uint8_t>> buffer;
	/// A wave left in its .sad (the dialogue banks, registered with headers only, as the original does): `buffer` is
	/// empty and wave_buffers reads `waveSize` bytes at `waveOffset` of `waveFile` when it decodes the sample (at its
	/// first play, as the original). Empty path: the bytes are in `buffer`.
	std::filesystem::path waveFile;
	uint64_t waveOffset {0};
	uint32_t waveSize {0};
	size_t sizeInBytes {0};
};
} // namespace openblack::audio
