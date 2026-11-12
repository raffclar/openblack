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
	/// The edges of the land blocks' coarse textures, mixed again by the paints of the blocks, and then the coarse
	/// textures of the blocks that the paints changed: two passes for each paint of the frame, before the land is drawn
	LandCoarseEdges0,
	LandCoarseTextures0,
	LandCoarseEdges1,
	LandCoarseTextures1,
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
	/// row has one hint for each gesture at most, and 23 pictures at most. For example, the open miracle selection shows
	/// a seed over each gesture that it waits for.
	GestureHintSeed0,
	GestureHintSeed1,
	GestureHintSeed2,
	GestureHintSeed3,
	GestureHintSeed4,
	GestureHintSeed5,
	GestureHintSeed6,
	GestureHintSeed7,
	GestureHintSeed8,
	GestureHintSeed9,
	GestureHintSeed10,
	GestureHintSeed11,
	GestureHintSeed12,
	GestureHintSeed13,
	GestureHintSeed14,
	GestureHintSeed15,
	GestureHintSeed16,
	GestureHintSeed17,
	GestureHintSeed18,
	GestureHintSeed19,
	GestureHintSeed20,
	GestureHintSeed21,
	GestureHintSeed22,
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
	/// The colours that the world gave the bodies of the players' creatures in a frame that draws the land, kept in a
	/// target for the frames that do not draw the land: the statistics box uses them in the temple
	WorldBodyColours,
	/// The colours of the last hair that the frame drew, kept in a target for the next frame: a body with the
	/// environment map lights its hair with them
	HairLight,
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
    "Land Coarse Edges 0 Pass",    //
    "Land Coarse Textures 0 Pass", //
    "Land Coarse Edges 1 Pass",    //
    "Land Coarse Textures 1 Pass", //
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
    "Gesture Hint Seed 5 Pass",    //
    "Gesture Hint Seed 6 Pass",    //
    "Gesture Hint Seed 7 Pass",    //
    "Gesture Hint Seed 8 Pass",    //
    "Gesture Hint Seed 9 Pass",    //
    "Gesture Hint Seed 10 Pass",   //
    "Gesture Hint Seed 11 Pass",   //
    "Gesture Hint Seed 12 Pass",   //
    "Gesture Hint Seed 13 Pass",   //
    "Gesture Hint Seed 14 Pass",   //
    "Gesture Hint Seed 15 Pass",   //
    "Gesture Hint Seed 16 Pass",   //
    "Gesture Hint Seed 17 Pass",   //
    "Gesture Hint Seed 18 Pass",   //
    "Gesture Hint Seed 19 Pass",   //
    "Gesture Hint Seed 20 Pass",   //
    "Gesture Hint Seed 21 Pass",   //
    "Gesture Hint Seed 22 Pass",   //
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
    "World Body Colours Pass",     //
    "Hair Light Pass",             //
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
