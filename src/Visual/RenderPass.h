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

#include <algorithm>
#include <array>
#include <string_view>

namespace openblack::graphics
{

enum class RenderPass : uint8_t
{
	/// The clouds' shadows over the land this frame, and the lights at night
	LandShade,
	/// The land's luminosity this frame: as it was laid, shaded by the clouds and lit by the lights
	LandLuminosity,
	/// The land cells' colours this frame, which light the land and the models on it
	LandColour,
	/// The sky's dome for each alignment, blended for the time of day a band of rows at a time
	SkyDome,
	ObjectShadow,
	/// The land seen from above, which the map in the temple's pool is textured with
	TempleMap,
	/// The temple's map at half size for the low detail levels: every second texel of every second row of the map
	TempleMapHalf,
	/// The sky of the sea's reflection, drawn first into its target
	ReflectionSky,
	Reflection,
	/// What blends in the sea's reflection, the farthest first
	ReflectionTranslucent,
	/// The hand, the thing that it holds and the player's creature, which the game mirrors after the land, over all of
	/// it: drawn into the reflection's target with its depth cleared, the smallest sort key first
	ReflectionMirrored,
	/// What is under the sea's surface, seen from the view as it is (not mirrored), drawn into the reflection's target
	/// over the mirrored world: the sea is then blended over it all. The sharks' bodies.
	UnderSea,
	/// The sky, drawn first into the view: everything in the scene is drawn over it and in front of it
	Sky,
	/// The land, drawn after the sea and before everything on it, as the game draws it: it writes its depth even where
	/// it is clear over the sea, so what is under the sea's surface behind it is hidden
	Land,
	Main,
	/// What blends in the world, drawn after the rest of the scene, the farthest first
	Translucent,
	/// The labels over the villagers' heads, tested against the depth of the scene that the views before leave
	WorldLabels,
	/// The collars of the leashes the gestures' hints offer, each in a view of its own over its hint's square
	GestureHintCollar0,
	GestureHintCollar1,
	GestureHintCollar2,
	/// The seeds and the globe that the gestures' hints picture, each in a view of its own over its hint's square. A
	/// row has at most five such pictures: the five miracles that a creature can cast in a fight.
	GestureHintSeed0,
	GestureHintSeed1,
	GestureHintSeed2,
	GestureHintSeed3,
	GestureHintSeed4,
	/// The gestures' hints along the bottom of the screen, over the scene and under the cinema bars
	GestureHints,
	/// The cinema bars over the scene: the advisors near the screen are drawn over them
	CinemaBars,
	/// The advisors near the screen, drawn over the whole scene with its depth cleared
	Advisors,
	/// The logo effect at the first start: its sky and its dots, drawn into a picture that keeps the last frame
	LogoEffect,
	/// The logo effect's text, the sponsor's logo and the fade to black, over its sky and its dots in the same picture
	LogoEffectOverlay,
	Interface,
	/// The player's creature in the falling spell's film, over the film's picture, seen by the film's camera, with the
	/// depth cleared
	FilmCreature,
	/// The fades over the falling spell's film and its creature
	FilmOverlay,
	/// The creature that the main menu's tattoo editor shows over its dialog, seen by the editor's own camera, with the
	/// depth cleared
	MenuCreature,
	/// What the tattoo editor draws over its creature: the marker on the place under the pointer and the symbols being
	/// dragged
	MenuCreatureOverlay,
	/// The creatures that the interface shows over itself, each player's creature on the statistics box's creature tab,
	/// with the depth cleared
	InterfaceCreatures,
	ImGui,
	/// The game's pointer, over the debug windows too
	Cursor,
	MeshViewer,

	_count
};

static constexpr std::array<std::string_view, static_cast<uint8_t>(RenderPass::_count)> k_RenderPassNames {
    "Land Shade Pass",             //
    "Land Luminosity Pass",        //
    "Land Colour Pass",            //
    "Sky Dome Pass",               //
    "Object Shadow Pass",          //
    "Temple Map Pass",             //
    "Temple Map Half Pass",        //
    "Reflection Sky Pass",         //
    "Reflection Pass",             //
    "Reflection Translucent Pass", //
    "Reflection Mirrored Pass",    //
    "Under Sea Pass",              //
    "Sky Pass",                    //
    "Land Pass",                   //
    "Main Pass",                   //
    "Translucent Pass",            //
    "World Labels Pass",           //
    "Gesture Hint Collar 0 Pass",  //
    "Gesture Hint Collar 1 Pass",  //
    "Gesture Hint Collar 2 Pass",  //
    "Gesture Hint Seed 0 Pass",    //
    "Gesture Hint Seed 1 Pass",    //
    "Gesture Hint Seed 2 Pass",    //
    "Gesture Hint Seed 3 Pass",    //
    "Gesture Hint Seed 4 Pass",    //
    "Gesture Hints Pass",          //
    "Cinema Bars Pass",            //
    "Advisors Pass",               //
    "Logo Effect Pass",            //
    "Logo Effect Overlay Pass",    //
    "Interface Pass",              //
    "Film Creature Pass",          //
    "Film Overlay Pass",           //
    "Menu Creature Pass",          //
    "Menu Creature Overlay Pass",  //
    "Interface Creatures Pass",    //
    "ImGui Pass",                  //
    "Cursor Pass",                 //
    "Mesh Viewer Pass",            //
};
// Every pass has a name: a short list would leave the last ones empty
static_assert(std::ranges::none_of(k_RenderPassNames, &std::string_view::empty));

/// Whether a pass draws a scene with its sky in a pass of its own before it
[[nodiscard]] constexpr bool HasSkyPass(RenderPass pass)
{
	return pass == RenderPass::Main || pass == RenderPass::Reflection;
}

/// The pass what blends in a scene is drawn in: the one after it, into the same target, or the scene's own if it has
/// none
[[nodiscard]] constexpr RenderPass TranslucentPassOf(RenderPass pass)
{
	switch (pass)
	{
	case RenderPass::Main:
		return RenderPass::Translucent;
	case RenderPass::Reflection:
		return RenderPass::ReflectionTranslucent;
	default:
		return pass;
	}
}

/// The pass a scene's land is drawn in: the one before it, into the same target, or the scene's own if it has none
[[nodiscard]] constexpr RenderPass LandPassOf(RenderPass pass)
{
	return pass == RenderPass::Main ? RenderPass::Land : pass;
}

/// The pass a scene's sky is drawn in: the one before it, into the same target, or the scene's own if it has none
[[nodiscard]] constexpr RenderPass SkyPassOf(RenderPass pass)
{
	switch (pass)
	{
	case RenderPass::Main:
		return RenderPass::Sky;
	case RenderPass::Reflection:
		return RenderPass::ReflectionSky;
	default:
		return pass;
	}
}

} // namespace openblack::graphics
