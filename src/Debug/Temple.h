/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Enums.h"
#include "Window.h"

namespace openblack::debug::gui
{

class TempleInterior final: public Window
{
public:
	TempleInterior() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	/// A player's alignment, set from the window, and how their temple's outside follows it
	void DrawAlignment() noexcept;

	PlayerNames _player {PlayerNames::PLAYER_ONE};
};

} // namespace openblack::debug::gui
