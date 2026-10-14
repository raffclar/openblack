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

#include <optional>

namespace openblack::help::profile
{
class HelpProfile;
struct WorldCameraView;
} // namespace openblack::help::profile

namespace openblack::ecs::systems
{

/// The help system's profile of what the player has done, kept for the whole session. Nothing is counted, and its clock
/// stands still, while a script's cut scene has the cinema bars in. A turn also stops while a front-end box shows.
class HelpProfileSystemInterface
{
public:
	virtual ~HelpProfileSystemInterface() = default;

	/// The player does one of the things counted
	virtual void Trigger(uint32_t event) = 0;
	/// A game turn ends. The profile counts the view of the world camera, and then its clock moves on. Nothing occurs
	/// while a front-end box shows (the game's menu, for example). The camera view is none when the player's world
	/// camera does not show the world (a script, the temple or a creature has the camera).
	virtual void ProcessTurn(bool frontEndBoxShows, const std::optional<help::profile::WorldCameraView>& worldCamera) = 0;

	[[nodiscard]] virtual const help::profile::HelpProfile& Get() const = 0;
	[[nodiscard]] virtual help::profile::HelpProfile& Get() = 0;
};

} // namespace openblack::ecs::systems
