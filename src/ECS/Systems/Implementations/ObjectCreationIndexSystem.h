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

#include <string>
#include <unordered_map>
#include <vector>

#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The creation counter kept for the whole game; OnLoadMap starts it again for every land
class ObjectCreationIndexSystem final: public ObjectCreationIndexSystemInterface
{
public:
	void OnLoadMap() override;
	[[nodiscard]] uint32_t Next() override;
	void Skip(uint32_t count) override;
	void AddTownSpell(uint32_t town, const std::string& spell) override;
	void OnTownCentre(uint32_t town) override;

private:
	/// A town's distinct spell seeds, whether its centre exists, and the icons it has taken
	struct TownSpells
	{
		std::vector<std::string> seeds;
		bool centre {false};
		uint32_t icons {0};
	};

	uint32_t _counter {0};
	/// A land has been loaded in this game
	bool _loaded {false};
	std::unordered_map<uint32_t, TownSpells> _towns;
};
} // namespace openblack::ecs::systems
