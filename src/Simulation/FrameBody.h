/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::simulation
{

/// The parts of the effects of a frame that belong to the view of one player: the hand, the advisors on the screen, the
/// click and the key of the player on the dialogue, the words of a song, the names of the villagers and the menu. The
/// game's client does them. A server has no view.
class FrameViewInterface
{
public:
	virtual ~FrameViewInterface() = default;

	/// Whether the hand of this computer shows this frame
	[[nodiscard]] virtual bool IsHandDrawn() const = 0;
	/// The advisors move and act on the screen
	virtual void UpdateAdvisors() = 0;
	/// Whether the player clicked the dialogue since the last frame. The click is then used.
	[[nodiscard]] virtual bool TakeDialogueClick() = 0;
	/// Whether the player holds the key that skips the dialogue
	[[nodiscard]] virtual bool DialogueSkipKeyDown() const = 0;
	/// The word of a sung verse that the song has reached gets its glow
	virtual void UpdateSingAlong(bool inTemple) = 0;
	/// Whether the words of the advisors showed in the last frame
	[[nodiscard]] virtual bool DialogueWordsDrawn() const = 0;
	/// The display of the villagers' names grows in or out over a frame of this game time
	virtual void UpdateVillagerLabels(float milliseconds) = 0;
	/// The bars of a cinema hide the dialogs of the game: the menu closes
	virtual void CloseMenuForCinema() = 0;
};

/// A view that does nothing, for a world with no player in front of it
class NoFrameView final: public FrameViewInterface
{
public:
	[[nodiscard]] bool IsHandDrawn() const override { return false; }
	void UpdateAdvisors() override {}
	[[nodiscard]] bool TakeDialogueClick() override { return false; }
	[[nodiscard]] bool DialogueSkipKeyDown() const override { return false; }
	void UpdateSingAlong(bool /*inTemple*/) override {}
	[[nodiscard]] bool DialogueWordsDrawn() const override { return false; }
	void UpdateVillagerLabels(float /*milliseconds*/) override {}
	void CloseMenuForCinema() override {}
};

/// Whether this frame draws the land. The land is not drawn while the player is in the temple, or while a film covers
/// the screen or hides the world.
[[nodiscard]] bool LandDrawn();

/// The draw starts: the creatures get their poses before the hand moves. The fights and the walks of the creatures
/// move on with their animations.
void PlayCreaturePoses();

/// The effects of the frame move on by the game time of the frame, after the hand moved. Some of them change the world:
/// the things that the creatures hold, the animals and the villagers between their turns, and the words of the scripts.
void PlayFrameEffects(FrameViewInterface& view);

} // namespace openblack::simulation
