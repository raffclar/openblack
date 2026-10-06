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

#include "Enums.h"
#include "Magic/SpellChants.h"
#include "Magic/WorshipBattery.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Shows the miracle tables (costs, timers and seeds of every magic type) and a prayer power sandbox: a worship site
/// with a number of dancers feeding a miracle cast from it, stepped turn by turn
class Magic final: public Window
{
public:
	Magic() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void DrawSelected() noexcept;
	void DrawTable() noexcept;
	void DrawSandbox() noexcept;

	void ResetSandbox() noexcept;
	void CastInSandbox() noexcept;
	void StepSandbox() noexcept;

	MagicType _selected {MagicType::Fireball};

	// The sandbox
	Tribe _tribe {Tribe::NORSE};
	int _dancers {10};
	float _tribalPower {1.0f};
	magic::WorshipBattery _site;
	magic::SpellChants _spell;
	MagicType _spellType {MagicType::None};
	bool _spellRunning {false};
	float _spellAge {0.0f};
	float _spellStrength {0.0f};
	uint32_t _turns {0};
	bool _running {false};
	float _sinceTurn {0.0f};
};

} // namespace openblack::debug::gui
