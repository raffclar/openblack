/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/OrientedText.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"

namespace openblack::gui
{
class GameInterface;
} // namespace openblack::gui

namespace openblack::graphics
{
class Texture2D;
} // namespace openblack::graphics

namespace openblack::ecs::systems
{

/// How the miracles look outside their own effects, frame by frame: the one-shot globes (their glint, the miracle
/// spinning inside each with its effect, the rings round the extreme ones), the hand holding a miracle (its bands,
/// bracelets and glow) and the piles of food and wood rising and sinking for what they hold. The miracles' system tells it when
/// a miracle comes to the hand, powers up, or leaves it.
class MiracleFxSystemInterface: public SavedStateInterface
{
public:
	virtual ~MiracleFxSystemInterface() = default;

	/// The hand some seconds on, and the piles of food and wood some seconds of game time on
	virtual void Update(float seconds, float gameSeconds) = 0;
	/// The globes after a frame of this real and game time, once a frame after the hand has placed what it holds: each
	/// turns to face the camera about its middle, wherever it now is, and its miracle spins inside. The effect in each
	/// globe steps no more often than 60 times a second, each step by all the game time since the last, and only in a
	/// frame that draws the land.
	virtual void UpdateGlobes(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime,
	                          bool landDrawn) = 0;
	/// At the start of each turn: each miracle's pile stands on the land where it is, as the land is now
	virtual void PlacePilesOnLand() {}
	/// Loads the models of the globes and their bands, if they are not loaded. The first update of the globes also
	/// loads them.
	virtual void LoadMeshes() {}
	/// A miracle came to the hand at a power-up level, or powered up from one: the hand wears a bracelet for it and one
	/// for each power-up; bands fly onto it with their sound unless the level fell; the announcer names the power-up
	virtual void SeedInHand(int powerUp, int previousPowerUp) = 0;
	/// The miracle left the hand: the bracelets go
	virtual void SeedLeftHand() = 0;
	/// The miracle was shaken off the hand: a band flies off it to the camera
	virtual void SeedShakenOff() = 0;
	/// A scribble shakes the hand: the shake sound plays and a band flies off the hand to the camera, with or without a
	/// miracle in the hand
	virtual void HandShaken() {}
	/// Everything shown goes, for a new land
	virtual void Reset() = 0;

	/// The interface whose font a tribe's power is written in, none while there is none
	virtual void SetInterface(const gui::GameInterface* /*interface*/) {}
	/// A tribe's power is behind the miracle that came to this computer's hand: the tribe's name rings the hand
	virtual void StartTribalPowerRing(Tribe /*tribe*/) {}
	/// The miracle left the hand: a ring still round it goes
	virtual void StopTribalPowerRing() {}
	/// The miracle in this computer's hand was cast with a tribe's power behind it: the ring is let go where the hand is
	/// and rises as a column of the name; without a ring a column rises there anyway
	virtual void ReleaseTribalPowerRing(Tribe /*tribe*/, glm::vec3 /*handPosition*/) {}
	/// Another player cast a miracle with a tribe's power behind it: a column of the name rises there in their colour
	virtual void TribalPowerColumn(Tribe /*tribe*/, glm::vec3 /*position*/, PlayerNames /*player*/) {}
	/// Once a frame, once the hand is drawn: the tribes' names some seconds of game time on
	virtual void UpdateTribalPower(float /*gameSeconds*/) {}
	/// The letters of the tribes' names to draw, and the font's texture they are cut from (none without a font)
	[[nodiscard]] virtual std::vector<OrientedTextVertex> GetTribalPowerText() const { return {}; }
	[[nodiscard]] virtual const graphics::Texture2D* GetTextTexture() const { return nullptr; }

	/// The models the globes and the hand's bands are drawn with, loaded once
	[[nodiscard]] virtual entt::id_type BubbleMesh() = 0;
	[[nodiscard]] virtual entt::id_type BandMesh() = 0;
};

} // namespace openblack::ecs::systems
