/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SyncedState.h"

#include <algorithm>
#include <array>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>

#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"

namespace openblack::synced_state
{

namespace
{

/// FNV-1a over bytes, as the parts of the world are hashed
constexpr uint64_t k_HashOffset = 14695981039346656037ull;
constexpr uint64_t k_HashPrime = 1099511628211ull;

void Mix(uint64_t& hash, const void* data, size_t size)
{
	const auto* bytes = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < size; ++i)
	{
		hash = (hash ^ bytes[i]) * k_HashPrime;
	}
}

// The frames draw these. Each turn reader of them was checked: the turns read none of these fields.
constexpr auto k_PresentationParts = std::to_array<PresentationPart>({
    // The animals play their clips and tilt in the frames. The turns go by Animal::position and Animal::heading.
    {.component = "Animal", .field = "clipPlace", .marker = ""},
    {.component = "Animal", .field = "groundCarry", .marker = ""},
    {.component = "Animal", .field = "bank", .marker = ""},
    {.component = "AnimalPose", .field = "", .marker = ""},
    {.component = "Transform", .field = "rotation", .marker = "Animal"},
    {.component = "AnimatedStaticPose", .field = "", .marker = ""},
    {.component = "AppleBarrelWorm", .field = "place", .marker = ""},
    // The villagers are drawn gliding between their turns
    {.component = "VillagerPose", .field = "place", .marker = ""},
    {.component = "VillagerPose", .field = "groundCarry", .marker = ""},
    {.component = "VillagerPose", .field = "drawnAt", .marker = ""},
    {.component = "VillagerPose", .field = "easedHeading", .marker = ""},
    {.component = "VillagerPose", .field = "drawnHeading", .marker = ""},
    {.component = "VillagerPose", .field = "detailedHeading", .marker = ""},
    {.component = "VillagerPose", .field = "slopeLift", .marker = ""},
    // The frames pose a villager's bones only while it is in view
    {.component = "VillagerPose", .field = "bones", .marker = ""},
    {.component = "VillagerPose", .field = "clipBones", .marker = ""},
    {.component = "VillagerPose", .field = "clipBlend", .marker = ""},
    // The seeds of the miracle icons spin, and their phials pulse, as the frames draw them
    {.component = "IconSeed", .field = "spin", .marker = ""},
    {.component = "IconSeed", .field = "ringSpin", .marker = ""},
    {.component = "IconSeed", .field = "phialFrame", .marker = ""},
    {.component = "IconSeed", .field = "phialPhase", .marker = ""},
    // The puffs of smoke, the clouds, the mists, the sky and the lights
    {.component = "ChimneySmoke", .field = "puffs", .marker = ""},
    {.component = "ChimneySmoke", .field = "state", .marker = ""},
    {.component = "Cloud", .field = "track", .marker = ""},
    {.component = "Mist", .field = "counter", .marker = ""},
    {.component = "Mist", .field = "counterRemainder", .marker = ""},
    {.component = "Mist", .field = "colour", .marker = ""},
    {.component = "Transform", .field = "position", .marker = "Cloud"},
    {.component = "Transform", .field = "position", .marker = "Mist"},
    {.component = "Sun", .field = "", .marker = ""},
    {.component = "SkyDome", .field = "", .marker = ""},
    // The moon's phase comes from the date of the computer's clock, as in the game
    {.component = "Moon", .field = "", .marker = ""},
    // The hum and the mana path of the hand past the border follow the hand that this computer draws
    {.component = "VirtualInfluence", .field = "shown", .marker = ""},
    {.component = "VirtualInfluence", .field = "hum", .marker = ""},
    {.component = "VillageLight", .field = "flicker", .marker = ""},
    {.component = "VillageLight", .field = "flickerTimer", .marker = ""},
    {.component = "VillageLight", .field = "glowSize", .marker = ""},
    {.component = "FireLook", .field = "", .marker = ""},
    {.component = "ResourcePile", .field = "rise", .marker = ""},
    // The drawn height of a pile, and the amount that it last rose or sank to show. The turns read only its speed-up.
    {.component = "ResourcePile", .field = "height", .marker = ""},
    {.component = "ResourcePile", .field = "shownAmount", .marker = ""},
    {.component = "ResourcePile", .field = "risen", .marker = ""},
    {.component = "LeashPost", .field = "look", .marker = ""},
    {.component = "Transform", .field = "rotation", .marker = "LeashPost"},
    // The frames find whether a post hangs its leash, for the hand's picks and tooltips, and show it at its size
    {.component = "LeashPost", .field = "hung", .marker = ""},
    {.component = "Transform", .field = "scale", .marker = "LeashPost"},
    // The moving bodies are drawn between their last two turns. The turns read the Transform.
    {.component = "PhysicsDrawPose", .field = "", .marker = ""},
    // The particle effects that the frames step
    {.component = "Vortex", .field = "beforeLandEffect", .marker = ""},
    {.component = "Vortex", .field = "afterLandEffect", .marker = ""},
    // The particle effects of a vortex and of a script's spot, by the numbers that the particles of this computer gave them
    {.component = "Vortex", .field = "lightMapEffect", .marker = ""},
    {.component = "Vortex", .field = "objectMoverEffect", .marker = ""},
    {.component = "ScriptSpotVisual", .field = "effect", .marker = ""},
    // The town totems ease to their shares by the computer's clock, as in the game
    {.component = "VillageTotem", .field = "ease", .marker = ""},
    // The rise that the totem last showed, by which the hand's tooltip of its share comes
    {.component = "VillageTotem", .field = "lastShownRise", .marker = ""},
    {.component = "VillageTotem", .field = "quietOnce", .marker = ""},
    // The pictures that face the camera, the glows and sparks of the scripts' highlights, and the highlights' spin
    {.component = "Sprite", .field = "", .marker = ""},
    {.component = "Transform", .field = "", .marker = "Sprite"},
    {.component = "*", .field = "", .marker = "ScriptHighlightGlow"},
    {.component = "ScriptHighlight", .field = "centre", .marker = ""},
    {.component = "HiddenByState", .field = "", .marker = "ScriptHighlight"},
    {.component = "ScriptHighlight", .field = "radius", .marker = ""},
    {.component = "ScriptHighlight", .field = "yAngle", .marker = ""},
    {.component = "ScriptHighlight", .field = "sparks", .marker = ""},
    // The particle effects of a highlight, by the numbers that the particles of this computer gave them
    {.component = "ScriptHighlight", .field = "glints", .marker = ""},
    {.component = "ScriptHighlight", .field = "activeEffect", .marker = ""},
    {.component = "ScriptHighlight", .field = "onScreen", .marker = ""},
    {.component = "Transform", .field = "rotation", .marker = "ScriptHighlight"},
    {.component = "Transform", .field = "scale", .marker = "ScriptHighlight"},
    // A miracle's globe as the frames draw it: turned to face the camera, its seed spun and its glint, rings and phial
    // stepped by the frames. The turns read the globe's Transform.
    {.component = "OneOffSpellSeed", .field = "position", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "middle", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "spin", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "ringSpin", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "glintFrame", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "phialFrame", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "phialPhase", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "seedPlacement", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "drawnModel", .marker = ""},
    {.component = "OneOffSpellSeed", .field = "drawnTurn", .marker = ""},
    // The roots that the frames show on a tree out of the land. Only the draw reads them.
    {.component = "ShownRoots", .field = "", .marker = ""},
    // The flags over the stores and the apple barrel move in the frames
    {.component = "*", .field = "", .marker = "TownDesireFlag"},
    {.component = "Transform", .field = "", .marker = "AppleBarrel"},
    {.component = "Transform", .field = "", .marker = "AppleBarrelWorm"},
    // The drawn hand follows the pointer and the camera in each frame, and its size follows its distance from the camera.
    // The world reads the hand only from the reports of its player (ReportedHand).
    {.component = "Transform", .field = "", .marker = "Hand"},
    {.component = "HandGrab", .field = "handPoint", .marker = ""},
    // The press, the pull, the spring, the offer and the scoop of this computer's hand are its own input. The world
    // has what the hand holds from the turns: the InHand of the thing.
    {.component = "HandGrab", .field = "state", .marker = ""},
    {.component = "HandGrab", .field = "object", .marker = ""},
    {.component = "HandGrab", .field = "pressMs", .marker = ""},
    {.component = "HandGrab", .field = "pressTurn", .marker = ""},
    {.component = "HandGrab", .field = "waits", .marker = ""},
    {.component = "HandGrab", .field = "pullSeconds", .marker = ""},
    {.component = "HandGrab", .field = "pulling", .marker = ""},
    {.component = "HandGrab", .field = "grips", .marker = ""},
    {.component = "HandGrab", .field = "tug", .marker = ""},
    {.component = "HandGrab", .field = "pullPosition", .marker = ""},
    {.component = "HandGrab", .field = "pullRotation", .marker = ""},
    {.component = "HandGrab", .field = "pullScale", .marker = ""},
    {.component = "HandGrab", .field = "hold", .marker = ""},
    {.component = "HandGrab", .field = "stretch", .marker = ""},
    {.component = "HandGrab", .field = "holdDistance", .marker = ""},
    {.component = "HandGrab", .field = "pullPlanePoint", .marker = ""},
    {.component = "HandGrab", .field = "pullPlaneNormal", .marker = ""},
    {.component = "HandGrab", .field = "lowering", .marker = ""},
    {.component = "HandGrab", .field = "rise", .marker = ""},
    {.component = "HandGrab", .field = "spring", .marker = ""},
    {.component = "HandGrab", .field = "springOn", .marker = ""},
    {.component = "HandGrab", .field = "springPending", .marker = ""},
    {.component = "HandGrab", .field = "recordedThrow", .marker = ""},
    {.component = "HandGrab", .field = "lastThrow", .marker = ""},
    {.component = "HandGrab", .field = "lastTarget", .marker = ""},
    {.component = "HandGrab", .field = "scoopSource", .marker = ""},
    {.component = "HandGrab", .field = "scoopTurns", .marker = ""},
    {.component = "HandGrab", .field = "locking", .marker = ""},
    // The frames hold the hand to a creature; the world has the hold from the lock (HandLockedCreature)
    {.component = "HandOnCreature", .field = "", .marker = ""},
    // The guidance that the player at this machine hears, weighed from this machine's camera with the local draws
    {.component = "GuidanceHeard", .field = "", .marker = ""},
    // The hand holds a town's totem and slides it as the frames draw it; the town takes the share at the turn
    {.component = "VillageTotem", .field = "gripped", .marker = ""},
    {.component = "VillageTotem", .field = "held", .marker = ""},
    {.component = "HandGrab", .field = "scoopStream", .marker = ""},
    {.component = "HandGrab", .field = "scoopSound", .marker = ""},
    {.component = "HandGrab", .field = "scoopAnchor", .marker = ""},
    {.component = "HandGrab", .field = "released", .marker = ""},
    {.component = "HandGrab", .field = "releaseSpinMs", .marker = ""},
    {.component = "HandGrab", .field = "pourEffect", .marker = ""},
    {.component = "HandGrab", .field = "pourSeconds", .marker = ""},
    {.component = "HandGrab", .field = "scoopStreamSeconds", .marker = ""},
    {.component = "HandGrab", .field = "offeredTo", .marker = ""},
    {.component = "HandGrab", .field = "offer", .marker = ""},
    {.component = "HandGrab", .field = "givenTo", .marker = ""},
    {.component = "HandGrab", .field = "offerTurn", .marker = ""},
    {.component = "HandGrab", .field = "lastHand", .marker = ""},
    // The frames bend the trees away from what moves near them, and only the frames read the bend
    {.component = "Tree", .field = "bendSlot", .marker = ""},
    // The light of a held thing as this computer last drew it
    {.component = "HeldLight", .field = "drawn", .marker = ""},
    {.component = "HeldLight", .field = "point", .marker = ""},
    {.component = "HeldLight", .field = "viewDepth", .marker = ""},
    {.component = "HeldLight", .field = "tint", .marker = ""},
    {.component = "HeldLight", .field = "added", .marker = ""},
    {.component = "HeldLight", .field = "treeGrey", .marker = ""},
    // The hand's size follows its distance from the camera in each frame. Only the frames read it.
    {.component = "HandGrab", .field = "handSize", .marker = ""},
    {.component = "HandMorph", .field = "", .marker = ""},
    // The hand's bones as the frame draws them. Only the frames read them, as the place of the leash's rope.
    {.component = "Hand", .field = "boneMatrices", .marker = ""},
    {.component = "HandMiracleFx", .field = "", .marker = ""},
    // The creature's bones as the frame draws them, between the poses of its last two turns. The turns read the pose of
    // the newest turn.
    {.component = "CreatureAnimation", .field = "boneMatrices", .marker = ""},
    // The creature's eyes, the sounds of its body and the prints of its feet
    {.component = "CreatureEyes", .field = "", .marker = ""},
    {.component = "CreatureAudio", .field = "", .marker = ""},
    {.component = "CreatureDrawnFeet", .field = "", .marker = ""},
    // The creature's skins as the frames paint them for the renderer
    {.component = "CreatureSkin", .field = "", .marker = ""},
    // Whether an object's sound is on: the frames turn the street lanterns' sounds on at dusk, and only the sounds read it
    {.component = "SoundTag", .field = "active", .marker = ""},
    // The sound that the audio of this computer plays for a tag. A server with no audio plays none.
    {.component = "SoundTag", .field = "emitter", .marker = ""},
});

} // namespace

std::span<const PresentationPart> PresentationParts()
{
	return k_PresentationParts;
}

bool IsPresentationOnly(std::span<const PresentationPart> parts, std::string_view component, std::string_view field)
{
	return std::ranges::any_of(parts, [component, field](const PresentationPart& part) {
		return part.marker.empty() && part.component == component && (part.field.empty() || part.field == field);
	});
}

Hash HashOf(const entt::registry& registry, const entt::meta_ctx& context, std::span<const PresentationPart> presentation,
            bool byField)
{
	// The parts that only some entities have, by the storage of their marker component
	std::vector<std::pair<const PresentationPart*, const entt::sparse_set*>> marked;
	for (const auto& part : presentation)
	{
		if (!part.marker.empty())
		{
			if (const auto* marker = ecs::reflection::FindStorage(registry, part.marker); marker != nullptr)
			{
				marked.emplace_back(&part, marker);
			}
		}
	}
	const auto keep = [&marked, presentation](std::string_view component, std::string_view field, entt::entity entity) {
		if (entity == entt::null)
		{
			return !(field.empty() && IsPresentationOnly(presentation, component, {}));
		}
		if (!field.empty() && IsPresentationOnly(presentation, component, field))
		{
			return false;
		}
		return std::ranges::none_of(marked, [&](const auto& entry) {
			const auto& [part, marker] = entry;
			const bool componentMatches = part->component == "*" || part->component == component;
			const bool fieldMatches = part->field.empty() ? field.empty() : part->field == field;
			return componentMatches && fieldMatches && marker->contains(entity);
		});
	};
	const auto hashes = ecs::reflection::HashWorld(registry, context, keep, byField);
	Hash result {.hash = k_HashOffset, .parts = {}};
	Mix(result.hash, &hashes.entities, sizeof(hashes.entities));
	result.parts.emplace_back("entities", hashes.entities);
	for (const auto& [name, part] : hashes.components)
	{
		Mix(result.hash, name.data(), name.size());
		Mix(result.hash, &part, sizeof(part));
		result.parts.emplace_back(name, part);
	}
	result.parts.insert(result.parts.end(), hashes.fields.begin(), hashes.fields.end());
	return result;
}

uint64_t Checksum(uint64_t stateHash, uint32_t seed, uint64_t draws)
{
	uint64_t hash = k_HashOffset;
	Mix(hash, &stateHash, sizeof(stateHash));
	Mix(hash, &seed, sizeof(seed));
	Mix(hash, &draws, sizeof(draws));
	return hash;
}

} // namespace openblack::synced_state
