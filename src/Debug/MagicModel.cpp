/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicModel.h"

#include <cstring>

#include <array>

#include <fmt/format.h>

#include "InfoConstants.h"
#include "Particles/ParticleTypes.h"

namespace openblack::debug::magic_window
{

namespace
{
constexpr std::array<std::string_view, static_cast<size_t>(magic::MagicInfoSection::_COUNT)> k_SectionNames {
    "General", "Heal", "Teleport", "Forest",       "Food",         "Storm and tornado",
    "Shield",  "Wood", "Water",    "Flying flock", "Ground flock", "Creature spell",
};

std::string_view NameOf(const std::array<char, 0x30>& name)
{
	return {name.data(), strnlen(name.data(), name.size())};
}
} // namespace

std::string_view SectionName(magic::MagicInfoSection section)
{
	const auto index = static_cast<size_t>(section);
	return index < k_SectionNames.size() ? k_SectionNames.at(index) : std::string_view("?");
}

std::string MagicName(const InfoConstants& info, MagicType type)
{
	const auto name = NameOf(magic::GetMagicEffectInfo(info, type).debugString);
	return name.empty() ? fmt::format("Magic type {}", static_cast<uint32_t>(type)) : std::string(name);
}

std::string SeedName(const InfoConstants& info, SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return "none";
	}
	const auto name = NameOf(magic::GetSpellSeedInfo(info, seed).debugString);
	return name.empty() ? fmt::format("Seed {}", static_cast<int>(seed)) : std::string(name);
}

std::string Seconds(float seconds)
{
	return seconds < 0.0f ? std::string("no limit") : fmt::format("{:.1f} s", seconds);
}

std::string PowerUpLevelName(int level)
{
	return level < 0 ? std::string("base") : fmt::format("{}", level);
}

std::string ParticleTypeLabel(ParticleType type)
{
	const auto file = psys::ParticleTypeFile(type);
	return fmt::format("{} ({})", static_cast<uint32_t>(type), file.empty() ? std::string_view("no file") : file);
}

std::string_view DrawPathName(psys::DrawPath path)
{
	switch (path)
	{
	case psys::DrawPath::Sorted:
		return "Sorted";
	case psys::DrawPath::Queued:
		return "Queued";
	case psys::DrawPath::Immediate:
		return "Immediate";
	}
	return "?";
}

} // namespace openblack::debug::magic_window
