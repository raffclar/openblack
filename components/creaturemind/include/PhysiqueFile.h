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
#include <optional>
#include <span>
#include <vector>

/// Creature physique files: the small file the game saves beside the player's creature's mind as a land is cleared, from
/// which the creature's body can be drawn without its mind. A little-endian stream of 32-bit numbers: the creature's
/// species row, the size it is drawn at, its strength, its fatness and its alignment, then the drops of blood on its
/// skin and the wounds, each a count followed by a word for each.
namespace openblack::creaturemind
{

struct PhysiqueFileData
{
	uint32_t speciesRow {0};
	/// The size its body is drawn at, as spells have made it
	float drawnSize {1.0f};
	/// 0 to 1
	float strength {0.0f};
	float fatness {0.0f};
	/// -1 (evil) to 1 (good), as it was before any spell changed it
	float alignment {0.0f};
	std::vector<uint32_t> blood;
	std::vector<uint32_t> wounds;

	bool operator==(const PhysiqueFileData&) const = default;
};

[[nodiscard]] std::vector<uint8_t> WritePhysique(const PhysiqueFileData& data);
/// The file read back, or none when it ends early or counts more marks than it holds
[[nodiscard]] std::optional<PhysiqueFileData> ReadPhysique(std::span<const uint8_t> bytes);
/// Whether the file was written whole
[[nodiscard]] bool WritePhysiqueFile(const std::filesystem::path& path, const PhysiqueFileData& data);

} // namespace openblack::creaturemind
