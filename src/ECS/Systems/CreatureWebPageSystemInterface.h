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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{

/// The web page about the player's creature, and the pictures of the creature on it (rules in creature_web_page,
/// pictures in components::CreaturePictures). The page and the pictures go into the folder of the player's profile.
class CreatureWebPageSystemInterface
{
public:
	/// What the renderer did with the pictures of the screen that the system asked for
	struct Screens
	{
		/// The picture made for a name (empty if the frame was too small), when it is back
		std::function<std::optional<std::vector<uint8_t>>(const std::string& name)> take;
		/// Whether the renderer gave up the picture asked for under a name
		std::function<bool(const std::string& name)> failed;
	};

	virtual ~CreatureWebPageSystemInterface() = default;
	/// Once a game turn, after the creatures act: the game can take a picture of each creature of the player that the
	/// player looks at. Each creature's moment of action for this turn is then used up.
	virtual void ProcessTurn() = 0;
	/// In the creature's own turn, after what a land keeps of it: the same for this creature
	virtual void CreatureTurn(entt::entity /*creature*/) {}
	/// Writes the page of a creature of the player from the game's template. The game does this each time that it
	/// writes the creature's files as the creature lives. The files go to the disk on a thread of their own.
	virtual void WritePage(entt::entity creature) = 0;
	/// Waits until the files of the pages and the pictures are on the disk
	virtual void FlushWrites() = 0;
	/// Whether a picture of the screen is asked for and not back yet. The game had no debug windows, so the frames for a
	/// picture leave them out.
	[[nodiscard]] virtual bool AnyPictureWaiting() const = 0;
	/// Each frame, after the renderer reads the frame: each picture that is back is written to its file
	virtual void CollectPictures(const Screens& screens) = 0;
	/// The captions of the five slots of pictures, which the page also gets
	[[nodiscard]] virtual std::span<const std::u16string> GetCaptions() const = 0;
};

} // namespace openblack::ecs::systems
