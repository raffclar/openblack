/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::video
{
class VideoPlayer;
class FallingSpellVideo;
} // namespace openblack::video

namespace openblack::ecs::systems
{
/// The game's film player and its falling spell video (video::Get, video::GetFallingSpell), each made on first use
/// with its game hooks, the falling spell after the player it plays on (Locator::videoSystem); and whether the game's
/// films play at all (the debug GUI's Video window turns them off for testing)
class VideoSystemInterface
{
public:
	virtual ~VideoSystemInterface() = default;

	[[nodiscard]] virtual video::VideoPlayer& Player() = 0;
	[[nodiscard]] virtual video::FallingSpellVideo& FallingSpell() = 0;
	/// On (the default): a film the game starts is decoded and shown. Off: it is skipped, as a build without the
	/// decoder skips it. Read when a film starts
	[[nodiscard]] virtual bool FilmsEnabled() const = 0;
	virtual void SetFilmsEnabled(bool enabled) = 0;
};
} // namespace openblack::ecs::systems
