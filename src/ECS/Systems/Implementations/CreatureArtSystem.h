/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <bitset>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Creature/CreatureArt.h"
#include "ECS/Systems/CreatureArtSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureArtSystem final: public CreatureArtSystemInterface
{
public:
	void HoldSpecies(CreatureType species) override;
	void HoldSkinFile(CreatureType species, creature::CreatureBody::Appearance look, entt::id_type file) override;
	void AskFor(CreatureType species) override;
	void AskForWorld() override;
	void Update() override;
	[[nodiscard]] size_t TakeSkinsToPrepare() override;

private:
	/// Whether the system asked for the art of the species, or the game does not hold it
	[[nodiscard]] bool IsAskedFor(CreatureType species) const;

	static constexpr size_t k_SpeciesCount = static_cast<size_t>(CreatureType::_COUNT);

	/// Starts the scan of the scripts again when the scripts changed, as for a new land
	void FollowScripts();
	/// Adds the number of painted skins of each asked species whose base mesh arrived to the skins to prepare
	void PrepareSkins();

	std::bitset<k_SpeciesCount> _held;
	std::bitset<k_SpeciesCount> _asked;
	std::array<std::vector<entt::id_type>, k_SpeciesCount> _skinFiles;
	/// The file of the base mesh of each species, which gives the number of painted skins. Zero when it is not held.
	std::array<entt::id_type, k_SpeciesCount> _baseSkinFiles {};
	/// The species whose spare skin textures the system made
	std::bitset<k_SpeciesCount> _skinsPrepared;
	/// The most painted skins of a species whose base mesh arrived since the last call of TakeSkinsToPrepare
	size_t _skinsToPrepare {0};

	/// The scripts that the system scans. These are not a part of the game state.
	const void* _code {nullptr};
	size_t _codeSize {0};
	uint32_t _lastTask {0};
	std::vector<creature_art::MakeNative> _makeNatives;
	std::unordered_map<uint32_t, creature_art::ScriptCode> _scripts;
	std::unordered_set<uint32_t> _scanned;
};

} // namespace openblack::ecs::systems
