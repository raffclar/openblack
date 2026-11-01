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

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"
#include "Particles/ParticleSpellLink.h"

// The running state of an effect as a saved game of the game keeps it. A rule or a creator is the number of its object
// in the particle file, counted from 0 in the order of the file. An effect made from the same file takes this state in
// place of its first collections, so that it continues from where the save stopped.
namespace openblack::particles
{

/// How the game draws a saved atom, which tells which creator made it
enum class RestoredLook : uint8_t
{
	/// Not drawn: a point. The save does not name the creator.
	Point,
	/// A sprite of the creator that the save names
	Sprite,
	/// Kept on an object, as a rule that follows an object makes it
	ObjectRef,
	/// Kept on an object, and drawn as a shower of sparks that rises from it
	GoldenShower,
};

struct RestoredCollection;

/// One saved atom
struct RestoredAtom
{
	RestoredLook look {RestoredLook::Point};
	/// The creator of a sprite
	std::optional<int> creator;
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
	glm::mat3 rotation {1.0f};
	float baseScale {1.0f};
	float ruleScale {1.0f};
	float stretch {1.0f};
	/// Red, green, blue and alpha
	std::array<uint8_t, 4> rgba {255, 255, 255, 255};
	std::array<uint8_t, 3> specular {0, 0, 0};
	float birth {0.0f};
	bool visible {true};
	bool drawOnFirstUpdate {false};
	bool deflected {false};
	/// The steps it has been through, up to two
	uint8_t steps {0};
	float frameRate {0.0f};
	bool playAnim {false};
	float gravity {1.0f};
	uint32_t random {0};
	DrawState previous;
	DrawState current;
	/// The object that an atom kept on an object follows; none once the object has gone
	entt::entity object {entt::null};
	/// The collections under the atom, the oldest first
	std::vector<RestoredCollection> collections;
};

/// What an emitter rule keeps for a collection
struct RestoredEmitter
{
	int rule {-1};
	maths::EmitterClock clock;
};

/// One saved collection
struct RestoredCollection
{
	int group {0};
	float birth {0.0f};
	float alpha {255.0f};
	bool hierarchy {false};
	bool interpolated {true};
	bool emitting {true};
	/// The rules of the group that are still on the collection. A rule of the group that is not in the list has let
	/// the collection go.
	std::vector<int> rules;
	std::vector<RestoredEmitter> emitters;
	/// The atoms, the oldest first
	std::vector<RestoredAtom> atoms;
};

/// One saved effect
struct RestoredEffect
{
	glm::vec3 origin {0.0f};
	float magnitude {1.0f};
	float age {0.0f};
	float closeAge {0.0f};
	bool closing {false};
	bool firstStepTwice {false};
	size_t atomCount {0};
	int player {-1};
	float globalAlpha {255.0f};
	glm::vec3 direction {0.0f};
	ProcessInfo info;
	/// The objects and points that the rules are to act on, the last given last
	std::vector<entt::entity> targets;
	std::vector<glm::vec3> targetPositions;
	/// The first collections, the oldest first
	std::vector<RestoredCollection> roots;
};

} // namespace openblack::particles
