/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RewardOpening.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// A seed's plain miracle, as the chest reads it
constexpr int k_PlainLevel = -1;

bool AllowedOn(const reward::ProgressEntry& entry, int32_t land)
{
	return land >= 0 && static_cast<size_t>(land) < entry.onLand.size() && entry.onLand.at(static_cast<size_t>(land)) != 0;
}

/// A miracle seed: learnt by the player or the town, a power-up the player lacks, or a one-shot globe
void GiveSeed(SpellSeedType seed, reward::OpeningWorld& world)
{
	const auto plain = world.MagicAt(seed, k_PlainLevel);
	if (!world.PlayerMay(plain))
	{
		// A town learns it for good and shows it above the chest; otherwise, or if the town knew it, a globe
		if (world.HasTown() && world.TownLearns(plain))
		{
			world.ShowTownSeed(seed);
			return;
		}
		world.OneShotGlobe(seed);
		return;
	}

	// The player can cast it already: the first power-up level of it they lack
	const int levels = world.LevelCount(seed);
	auto magic = plain;
	int level = 0;
	for (; level < levels; ++level)
	{
		magic = world.MagicAt(seed, level);
		if (!world.PlayerMay(magic))
		{
			break;
		}
	}
	if (level == levels || magic == MagicType::None)
	{
		world.OneShotGlobe(seed);
		return;
	}
	if (world.HasTown())
	{
		world.TownLearns(magic);
	}
	else
	{
		world.PlayerLearns(magic);
	}
	world.ShowGesture(magic);
}
} // namespace

void reward::Open(const Contents& contents, OpeningWorld& world)
{
	if (contents.food > 0.0f)
	{
		world.Pour(ResourceType::Food, static_cast<uint32_t>(contents.food));
		return;
	}
	if (contents.wood != 0)
	{
		world.Pour(ResourceType::Wood, contents.wood);
		return;
	}
	if (contents.belief != 0.0f)
	{
		// Only a town is given belief
		if (world.HasTown())
		{
			world.GiveTownBelief(contents.belief);
		}
		return;
	}
	if (contents.object != MobileStaticInfo::None)
	{
		world.MakeObject(contents.object);
		return;
	}
	if (contents.scaffold)
	{
		world.MakeScaffold();
		return;
	}

	auto seed = contents.seed;
	if (contents.nextMiracle)
	{
		if (!world.HasTown())
		{
			world.FindTownForMiracle();
		}
		const auto next = world.NextMiracle();
		if (next == MagicType::None)
		{
			// Nothing left to learn: a random miracle of the progress, as a one-shot globe
			const auto random = world.RandomMiracle();
			if (random != MagicType::None)
			{
				world.OneShotGlobe(world.FirstSeedOf(random));
			}
			return;
		}
		seed = world.FirstSeedOf(next);
	}

	if (seed == SpellSeedType::None)
	{
		// A power-up on its own only shows its gesture
		if (contents.powerUp != MagicType::None)
		{
			world.ShowGesture(contents.powerUp);
		}
		return;
	}
	GiveSeed(seed, world);
}

MagicType reward::MagicAtLevel(const std::array<MagicType, 4>& magicTypes, const std::array<GestureType, 3>& powerUpGestures,
                               int level)
{
	if (level < k_PlainLevel)
	{
		return MagicType::None;
	}
	const auto index = static_cast<size_t>(level + 1);
	if (index < magicTypes.size())
	{
		return magicTypes.at(index);
	}
	// One past the magic types the game reads on into the power-up gestures that follow them
	const auto past = index - magicTypes.size();
	return past < powerUpGestures.size() ? static_cast<MagicType>(powerUpGestures.at(past)) : MagicType::None;
}

bool reward::TakesGoodList(float alignment, const std::function<bool()>& coinIsHeads)
{
	if (alignment == 0.0f)
	{
		return coinIsHeads();
	}
	return alignment > 0.0f;
}

MagicType reward::NextMiracle(std::span<const ProgressEntry> list, int32_t land,
                              const std::function<bool(MagicType)>& playerMay)
{
	const auto found = std::ranges::find_if(
	    list, [&](const ProgressEntry& entry) { return AllowedOn(entry, land) && !playerMay(entry.magic); });
	return found != list.end() ? found->magic : MagicType::None;
}

MagicType reward::RandomMiracle(std::span<const ProgressEntry> list, int32_t land,
                                const std::function<uint32_t(uint32_t)>& draw)
{
	// The game would draw for ever from a list with nothing allowed on the land
	if (list.empty() || std::ranges::none_of(list, [land](const ProgressEntry& entry) {
		    return AllowedOn(entry, land) && entry.magic != MagicType::None;
	    }))
	{
		return MagicType::None;
	}
	auto magic = MagicType::None;
	while (magic == MagicType::None)
	{
		const auto index = draw(static_cast<uint32_t>(list.size()));
		if (index < list.size() && AllowedOn(list[index], land))
		{
			magic = list[index].magic;
		}
	}
	return magic;
}

uint32_t reward::StepOpenedClip(uint32_t place, uint32_t milliseconds, uint32_t playTime, bool looping)
{
	if (playTime == 0)
	{
		return 0;
	}
	const auto next = place + milliseconds;
	return looping ? next % playTime : std::min(next, playTime - 1);
}
