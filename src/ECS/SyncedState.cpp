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
#include <vector>

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
    // Where the frame draws an animal between its last two turns. The turns go by its Transform.
    {.component = "Animal", .field = "drawn", .marker = ""},
    // A shark swims through its clip in the frames, drawn between the start of its turn and its place, facing the way
    // it moved. The turns go by Shark::position and its Transform.
    {.component = "Shark", .field = "clipPlace", .marker = ""},
    {.component = "Shark", .field = "heading", .marker = ""},
    {.component = "Shark", .field = "drawn", .marker = ""},
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
    // A villager's soul plays its clip and fades in the frames. The turns remove it by the turns since it rose.
    {.component = "RisingSoul", .field = "timeMs", .marker = ""},
    {.component = "RisingSoul", .field = "alpha", .marker = ""},
    {.component = "RisingSoul", .field = "bones", .marker = ""},
    {.component = "RisingSoul", .field = "shownOut", .marker = ""},
    // The soul's clip, heaven or hell, comes from the random numbers that the frames draw too. The turns remove the soul
    // by the longer clip.
    {.component = "RisingSoul", .field = "clip", .marker = ""},
    // The frames pose a villager's bones only while it is in view
    {.component = "VillagerPose", .field = "bones", .marker = ""},
    {.component = "VillagerPose", .field = "clipBones", .marker = ""},
    {.component = "VillagerPose", .field = "clipBlend", .marker = ""},
    // The storm's swirl and the trail of a flock are effects of each computer. Each computer gives them its own
    // numbers, which the turns use only to find the effect again.
    {.component = "Spell", .field = "castEffect", .marker = ""},
    {.component = "FlockSpell", .field = "castEffect", .marker = ""},
    // The mark over a learning creature rises from the head of its drawn pose, and only the draw reads it
    {.component = "LearningMarker", .field = "", .marker = ""},
    // The frames move the hair of a creature. Only the draw reads it.
    {.component = "CreatureHair", .field = "", .marker = ""},
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
    // The frames draw a leash at its temple, or in the hand that carries the picked leash. The turns go by
    // LeashPost::point, and no turn reads the Transform of a leash (it has no place in the map).
    {.component = "Transform", .field = "position", .marker = "LeashPost"},
    {.component = "Transform", .field = "rotation", .marker = "LeashPost"},
    // The frames find whether a post hangs its leash, for the hand's picks and tooltips, and show it at its size
    {.component = "LeashPost", .field = "hung", .marker = ""},
    {.component = "Transform", .field = "scale", .marker = "LeashPost"},
    // The frames put the picked leash of this computer's player in its hand. The turns read the point of the post.
    {.component = "Transform", .field = "position", .marker = "LeashPost"},
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
    // The height where the frames draw a town's totem and its icon as they ease by the computer's clock
    {.component = "DrawnHeight", .field = "", .marker = ""},
    // The frames move the fish of a shoal near the camera of each computer, and a grip of the sea scares them only on the
    // computer of the hand. The turns read only the shoal's centre, which the land gives, and its fullness, which comes
    // from the fish of the farm.
    {.component = "FishFarm", .field = "shoal", .marker = ""},
    {.component = "FishFarm", .field = "shownAlpha", .marker = ""},
    // The pictures that face the camera, the glows and sparks of the scripts' highlights, and the highlights' spin
    {.component = "Sprite", .field = "", .marker = ""},
    {.component = "Transform", .field = "", .marker = "Sprite"},
    // A firefly drifts about its place in the frames. The turns go by Firefly::at, and its Transform is that place, which
    // the hash takes from Firefly::at. Only the frames give it a sprite, while it is in view.
    {.component = "Firefly", .field = "clock", .marker = ""},
    {.component = "Firefly", .field = "amplitude", .marker = ""},
    {.component = "Firefly", .field = "drawn", .marker = ""},
    {.component = "Transform", .field = "", .marker = "Firefly"},
    // The opening's hand and the things of the explorers' boat film: the turns make and remove their entities, and the
    // frames show, move, pose and hide them. The boy that the opening's hand holds is drawn in its grip, which only the
    // draw and the script camera read.
    {.component = "*", .field = "", .marker = "IntroHand"},
    {.component = "*", .field = "", .marker = "BoatFilmPart"},
    // The rooms of the temple, their lights and the mist of the creature cave are not a part of the land. The temple
    // makes them for the player of this computer when the player goes into it, and removes them when the player leaves.
    {.component = "*", .field = "", .marker = "TempleInteriorPart"},
    // The glows on a creature's body turn and slide in the frames, and the frames place them where the body is drawn.
    // The turns set their powers and scales.
    {.component = "CreatureHandGlows", .field = "slidePace", .marker = ""},
    {.component = "CreatureHandGlows", .field = "turnPace", .marker = ""},
    {.component = "CreatureHandGlows", .field = "placed", .marker = ""},
    {.component = "CreatureHandGlows", .field = "motion", .marker = ""},
    // The snow on a thing comes and goes in the frames that draw the thing. Only the draw reads it.
    {.component = "SnowLevel", .field = "", .marker = ""},
    // The picture of a challenge comes from the screen of this computer, or from its profile's files, in the frames. The
    // turns only count the pictures that the scripts ask for.
    {.component = "ChallengePicture", .field = "full", .marker = ""},
    {.component = "ChallengePicture", .field = "small", .marker = ""},
    {.component = "ChallengePicture", .field = "waiting", .marker = ""},
    {.component = "ChallengePicture", .field = "taken", .marker = ""},
    // The turn makes the see-through copy of a town totem and of its icon. The frames move, show and hide it.
    {.component = "*", .field = "", .marker = "VillageTotemGhost"},
    // The draw gives the light of the frame to a thing in the hand, and takes it away when the hand lets go
    {.component = "HeldLight", .field = "", .marker = ""},
    // The static of a creature that fizzes slides only in the frames that draw its body. Only the draw reads it.
    {.component = "CreatureSpells", .field = "staticScroll", .marker = ""},
    {.component = "HighDetail", .field = "heldAt", .marker = ""},
    // The eyes of a villager that a script draws in high detail blink and look in the frames, by the local random
    // numbers. Only the draw reads them.
    {.component = "HighDetail", .field = "eyes", .marker = ""},
    {.component = "HighDetail", .field = "drawnEyes", .marker = ""},
    // A fish farm's shoal swims and darts in the frames near the camera, by the local random numbers. The turns go by
    // the farm's fish; the shoal's centre never moves.
    {.component = "FishFarm", .field = "shoal", .marker = ""},
    {.component = "FishFarm", .field = "shownAlpha", .marker = ""},
    {.component = "*", .field = "", .marker = "ScriptHighlightGlow"},
    // The clouds of a storm gather and drift in the frames by the local random numbers, as the game draws them. The
    // rain and the storm's turns read the storm, not its clouds.
    {.component = "*", .field = "", .marker = "StormCloud"},
    // The last flash of a storm lights the land and the clouds. A storm that strikes round the camera puts its flash near
    // the camera of this computer, so each computer has its own flash. The turns read the timers, not the flash.
    {.component = "Storm", .field = "flash", .marker = ""},
    // The sparkles of the moves that the player queued in a fight: only the computer of the player adds them, as the
    // game draws them. The turns read the queue, not its sparkles.
    {.component = "CreatureFighting", .field = "marks", .marker = ""},
    // The static that a creature fizzes through slides only while this computer draws it, as a knocked out fighter
    // fades. The turns read how far it fizzed, not the static.
    {.component = "CreatureSpells", .field = "staticScroll", .marker = ""},
    // The glow of the hand while a gesture is drawn, which only the computer that draws the gesture shows
    {.component = "HandGlow", .field = "", .marker = ""},
    // The sum of the strokes and slaps that the hand let go of a creature with, which only the creature's status panel
    // shows
    {.component = "HandLastFeedback", .field = "", .marker = ""},
    // A miracle's seed in the hand is drawn at the hand of this computer, with its effect in the hand
    {.component = "SpellSeed", .field = "handEffect", .marker = ""},
    {.component = "Transform", .field = "position", .marker = "SpellSeed"},
    {.component = "Transform", .field = "rotation", .marker = "SpellSeed"},
    // The number of a miracle's particle effect: each computer numbers its effects, and makes more of them for its own
    // hand
    {.component = "Spell", .field = "effect", .marker = ""},
    // The numbers of other effects that the turns make, as the particles of this computer numbered them. The frames make
    // effects of their own, such as the hand's gesture trails, which take numbers too. The turns use a number only to
    // find their effect on this computer.
    {.component = "SpellDispenser", .field = "effect", .marker = ""},
    {.component = "TownArtefact", .field = "effect", .marker = ""},
    {.component = "MagicShield", .field = "effect", .marker = ""},
    // The number of the spot visual that a temple's heart fires its beams from, as the particles of this computer
    // numbered it
    {.component = "Temple", .field = "beamSource", .marker = ""},
    // The same for the glow and the beams of a temple that the enemy destroys, and the sound loops of its destruction,
    // which only a computer with audio plays. The turns read them only to stop the sounds and to show the glow and the
    // beams.
    {.component = "Temple", .field = "destructionGlow", .marker = ""},
    {.component = "Temple", .field = "destructionBeamSource", .marker = ""},
    {.component = "Temple", .field = "destructionLoops", .marker = ""},
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
    {.component = "OneOffSpellSeed", .field = "flightModel", .marker = ""},
    // The frames ease the drawn height of a field's crop toward the height that its food gives. Only the draw reads it.
    {.component = "Field", .field = "height", .marker = ""},
    // The roots that the frames show on a tree out of the land. Only the draw reads them.
    {.component = "ShownRoots", .field = "", .marker = ""},
    // The snow that a thing shows eases only while this computer draws the thing. Only the draw reads it.
    {.component = "SnowLevel", .field = "", .marker = ""},
    // The flags over the stores and the apple barrel move in the frames
    {.component = "*", .field = "", .marker = "TownDesireFlag"},
    {.component = "Transform", .field = "", .marker = "AppleBarrel"},
    {.component = "Transform", .field = "", .marker = "AppleBarrelWorm"},
    // The drawn hand follows the pointer and the camera in each frame, and its size follows its distance from the camera.
    // The world reads the hand only from the reports of its player (ReportedHand).
    {.component = "Transform", .field = "", .marker = "Hand"},
    // The frames of this computer move what the hand of another player holds toward the reported place of that hand
    {.component = "HeldDrawTrack", .field = "", .marker = ""},
    // A dead tree drops its roots the first time that a frame draws it. Only the frames read the mark.
    {.component = "DropsRoots", .field = "", .marker = ""},
    // Where the frames draw a thing that a tornado carries. The turns go by its Transform.
    {.component = "CarriedByTornado", .field = "drawn", .marker = ""},
    // The pose that the frames draw a thing in while a hand pulls at it. The turns keep the thing as it stood.
    {.component = "HandPulled", .field = "", .marker = ""},
    // Where the frames draw a thing at the drawn hand. The turns go by its Transform, at the reported place of the hand.
    {.component = "InHand", .field = "drawn", .marker = ""},
    // The frames move the rope of a worn leash; the world has the lengths of the leash in CreatureLeash
    {.component = "CreatureLeashRope", .field = "", .marker = ""},
    // The computers that draw make the sparkles of a leash order; the game server makes none
    {.component = "LeashOrderSparkles", .field = "", .marker = ""},
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
    {.component = "HandGrab", .field = "predicted", .marker = ""},
    {.component = "HandGrab", .field = "lastTarget", .marker = ""},
    {.component = "HandGrab", .field = "scoopSource", .marker = ""},
    {.component = "HandGrab", .field = "scoopTurns", .marker = ""},
    {.component = "HandGrab", .field = "locking", .marker = ""},
    // The frames hold the hand to a creature; the world has the hold from the lock (HandLockedCreature)
    {.component = "HandOnCreature", .field = "", .marker = ""},
    // The guidance that the player at this machine hears, weighed from this machine's camera with the local draws
    {.component = "GuidanceHeard", .field = "", .marker = ""},
    // The help of the creature is for the player at this machine only. Each computer keeps it for the creature of its
    // own player, from its own dialogue. Only the help reads it.
    {.component = "CreatureHelpMemory", .field = "", .marker = ""},
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
    {.component = "HandGrab", .field = "callingOff", .marker = ""},
    {.component = "HandGrab", .field = "offerTurn", .marker = ""},
    {.component = "HandGrab", .field = "lastHand", .marker = ""},
    // The frames bend the trees away from what moves near them, and only the frames read the bend
    {.component = "Tree", .field = "bendSlot", .marker = ""},
    // The heart of the player at this machine beats faster while its temple goes. Only the sound of the heart reads the
    // beat.
    {.component = "Heartbeat", .field = "", .marker = ""},
    // The strain that the player at this machine hears from the worship sites of its temple. Only the guidance reads it.
    {.component = "CitadelWorship", .field = "strainSound", .marker = ""},
    // The pictures of the web page of the creature of the player at this machine, from this machine's camera
    {.component = "CreaturePictures", .field = "", .marker = ""},
    // The copy of the creature of the player at this machine that a film shows
    {.component = "*", .field = "", .marker = "FilmCreature"},
    // A field's crop sways as one of the trees' sways, which each computer picks with its own local random number. Only
    // the frames read it.
    {.component = "Swayable", .field = "swaySlot", .marker = "Field"},
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
    // The blended animations of a creature are a store of blends made the first time that something asks for them: a
    // turn, or a frame that finds where the creature's hand takes a gift. A blend comes only from the species and the
    // drawn body, which only a turn changes, and a change of the body empties the store. Thus a turn reads the same
    // blend whoever made it, and only which blends the store keeps can differ.
    {.component = "CreatureAnimation", .field = "animations", .marker = ""},
    // Where the frame draws a walking creature between its last two turns. The turns go by its Transform and slots.
    {.component = "CreatureAnimation", .field = "drawnPlace", .marker = ""},
    // Where the frame draws the thing in a creature's hand. The turns go by its Transform.
    {.component = "HeldByCreature", .field = "drawn", .marker = ""},
    // The creature's eyes, the sounds of its body and the prints of its feet
    {.component = "CreatureEyes", .field = "", .marker = ""},
    {.component = "CreatureAudio", .field = "", .marker = ""},
    {.component = "CreatureDrawnFeet", .field = "", .marker = ""},
    // The creature's skins as the frames paint them for the renderer
    {.component = "CreatureSkin", .field = "", .marker = ""},
    // The skin paints the tattoos that the tattoo editor shows, and the masks of the player, in the frames. The turns put
    // the slots and the block of pictures on the creature (player_commands::CreatureTattoo) and read only those.
    {.component = "CreatureTattoos", .field = "shown", .marker = ""},
    {.component = "CreatureTattoos", .field = "picturesShown", .marker = ""},
    {.component = "CreatureTattoos", .field = "shownPictures", .marker = ""},
    {.component = "CreatureTattoos", .field = "revision", .marker = ""},
    {.component = "TattooMasks", .field = "", .marker = ""},
    // Whether an object's sound is on: the frames turn the street lanterns' sounds on at dusk, and only the sounds read it
    {.component = "SoundTag", .field = "active", .marker = ""},
    // The sound that the audio of this computer plays for a tag. A server with no audio plays none.
    {.component = "SoundTag", .field = "emitter", .marker = ""},
	// The dust of a reward chest: the C library's random numbers place it, and the frames fade it. Only the draw reads it.
	{.component = "Reward", .field = "dust", .marker = ""},
    // The frames draw a reward chest that falls between its last two turns. The turns go by its Transform and clock.
    {.component = "Reward", .field = "drawn", .marker = ""},
    // The lid of an opened chest lifts in the frames that draw it, and its dust fades in the frames
    {.component = "Reward", .field = "clipPlace", .marker = ""},
    {.component = "Reward", .field = "dustMilliseconds", .marker = ""},
    // The miracle above an opened chest spins and pulses in the frames
    {.component = "RewardSeed", .field = "spin", .marker = ""},
    {.component = "RewardSeed", .field = "phialFrame", .marker = ""},
    {.component = "RewardSeed", .field = "phialPhase", .marker = ""},
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
            bool byField, const ecs::reflection::RowVisitor& visit, WorkerPool* pool, ecs::reflection::WorldRows* rows)
{
	// The hash does not include the presentation parts. A part that only some entities have uses the storage of its marker
	// component. When the registry has no such storage, no entity has the marker, and the hash keeps the part.
	std::vector<ecs::reflection::HashLeaveOut> leaveOut;
	leaveOut.reserve(presentation.size());
	for (const auto& part : presentation)
	{
		if (part.marker.empty())
		{
			leaveOut.push_back({.component = part.component, .field = part.field, .marker = nullptr});
		}
		else if (const auto* marker = ecs::reflection::FindStorage(registry, part.marker); marker != nullptr)
		{
			leaveOut.push_back({.component = part.component, .field = part.field, .marker = marker});
		}
	}
	const auto hashes = ecs::reflection::HashWorld(registry, context, leaveOut, byField, visit, pool, rows);
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
	result.unregistered = hashes.unregistered;
	return result;
}

std::string UnregisteredLine(std::span<const std::string> names)
{
	std::string line(k_UnregisteredWords);
	for (const auto& name : names)
	{
		line += (line.size() == k_UnregisteredWords.size() ? "" : ", ") + name;
	}
	return line;
}

std::vector<std::string> UnregisteredReporter::Fresh(std::span<const std::string> names)
{
	std::vector<std::string> fresh;
	for (const auto& name : names)
	{
		if (_seen.insert(name).second)
		{
			fresh.push_back(name);
		}
	}
	return fresh;
}

uint64_t MixValues(uint64_t stateHash, std::span<const int64_t> values)
{
	uint64_t hash = stateHash;
	for (const auto value : values)
	{
		Mix(hash, &value, sizeof(value));
	}
	return hash;
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
