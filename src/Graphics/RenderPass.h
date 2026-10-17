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
	/// EXPERIMENT (not for merge): the map of the volumetric clouds' shadow round the eye, before the land's shade reads it
	CloudShadow,
	/// EXPERIMENT (not for merge): the map of the sun's visibility over the land, made again as the light moves, and its
	/// blend from the old map to the new
	SunVisibility,
	SunVisibilityBlend,
	/// EXPERIMENT (not for merge): the air's light: the texture of the light that gets through the air and the table of
	/// the light that the air scatters more than once (both made again only when the air changes), the texture of the
	/// sky round the eye, and the haze's grid that follows the view
	AtmosphereTransmittance,
	AtmosphereMultiple,
	AtmosphereSkyView,
	AtmosphereAerial,
	/// EXPERIMENT (not for merge): the light probes over the island, a band of rows a frame after each new set of
	/// shadows, and the blend of the old grid into the new
	LightProbes,
	LightProbesBlend,
	/// EXPERIMENT (not for merge): the maps of the environment's shadows seen from the light, two cascades made again
	/// about every 3 seconds, and the map of the moving things' shadows, made each frame
	SunShadowEnvironment0,
	SunShadowEnvironment1,
	SunShadowMoving,
	/// The clouds' shadows over the land this frame, and the lights at night
	LandShade,
	/// The land's luminosity this frame: as it was laid, shaded by the clouds and lit by the lights
	LandLuminosity,
	/// The land cells' colours this frame, which light the land and the models on it
	LandColour,
	/// The sky's dome for each alignment, blended for the time of day a band of rows at a time
	SkyDome,
	Footprint,
	/// The rivers' channels, into the land's alpha
	LandAlpha,
	ObjectShadow,
	/// The land seen from above, which the map in the temple's pool is textured with
	TempleMap,
	/// The sky of the sea's reflection, drawn first into its target
	ReflectionSky,
	Reflection,
	/// What blends in the sea's reflection, the farthest first
	ReflectionTranslucent,
	/// What is under the sea's surface, seen from the view as it is (not mirrored), drawn into the reflection's target
	/// over the mirrored world: the sea is then blended over it all. The sharks' bodies.
	UnderSea,
	/// The sky, drawn first into the view: everything in the scene is drawn over it and in front of it
	Sky,
	/// The land, drawn after the sea and before everything on it, as the game draws it: it writes its depth even where
	/// it is clear over the sea, so what is under the sea's surface behind it is hidden
	Land,
	Main,
	/// EXPERIMENT (not for merge): the rays through the volumetric clouds at half the screen's size, after the scene's
	/// depth is known, and their blend with the frames before
	CloudMarch,
	CloudResolve,
	/// EXPERIMENT (not for merge): the shade where the things round a point hide the sky, at half the screen's size
	AmbientOcclusion,
	/// EXPERIMENT (not for merge): the shadows of the things from the light's maps, over the scene's colour
	ShadowApply,
	/// EXPERIMENT (not for merge): the volumetric fog's grid of the light that each cell scatters, and its sum from the
	/// eye
	FogScatter,
	FogIntegrate,
	/// What blends in the world, drawn after the rest of the scene, the farthest first
	Translucent,
	/// EXPERIMENT (not for merge): one step of the foam that builds up on the water and decays
	UnderwaterFoam,
	/// EXPERIMENT (not for merge): the sky of one face of the cube of the far scene round the eye, which the water
	/// reflects where the screen does not show the reflection
	UnderwaterEnvironmentSky,
	/// EXPERIMENT (not for merge): the land and what stands on it, in the same face of the cube
	UnderwaterEnvironment,
	/// EXPERIMENT (not for merge): the steps of the field of small waves on the water, at most two in a frame
	UnderwaterWake0,
	UnderwaterWake1,
	/// EXPERIMENT (not for merge): the scene seen under the sea, from its own target to the window, with the motes in
	/// the water over it
	UnderwaterPost,
	/// EXPERIMENT (not for merge): what blends over the water when the eye is above it (the clouds, the particles, the
	/// soft edges of the birds). The water is drawn first, so that these blend over it and not over the empty sky. The
	/// scene's land and models and the water's surface hide them by their depth
	UnderwaterOver,
	/// The collars of the leashes the gestures' hints offer, each in a view of its own over its hint's square
	GestureHintCollar0,
	GestureHintCollar1,
	GestureHintCollar2,
	/// The gestures' hints along the bottom of the screen, over the scene and under the cinema bars
	GestureHints,
	/// The cinema bars over the scene: the advisors near the screen are drawn over them
	CinemaBars,
	/// The advisors near the screen, drawn over the whole scene with its depth cleared
	Advisors,
	Interface,
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
    "Cloud Shadow Pass",           //
    "Sun Visibility Pass",         //
    "Sun Visibility Blend Pass",   //
    "Atmosphere Transmittance",    //
    "Atmosphere Multiple",         //
    "Atmosphere Sky View",         //
    "Atmosphere Aerial",           //
    "Light Probes",                //
    "Light Probes Blend",          //
    "Sun Shadow Environment 0",    //
    "Sun Shadow Environment 1",    //
    "Sun Shadow Moving Pass",      //
    "Land Shade Pass",             //
    "Land Luminosity Pass",        //
    "Land Colour Pass",            //
    "Sky Dome Pass",               //
    "Footprint Pass",              //
    "Land Alpha Pass",             //
    "Object Shadow Pass",          //
    "Temple Map Pass",             //
    "Reflection Sky Pass",         //
    "Reflection Pass",             //
    "Reflection Translucent Pass", //
    "Under Sea Pass",              //
    "Sky Pass",                    //
    "Land Pass",                   //
    "Main Pass",                   //
    "Cloud March Pass",            //
    "Cloud Resolve Pass",          //
    "Ambient Occlusion Pass",      //
    "Shadow Apply Pass",           //
    "Fog Scatter Pass",            //
    "Fog Integrate Pass",          //
    "Translucent Pass",            //
    "Underwater Foam Pass",        //
    "Underwater Cube Sky Pass",    //
    "Underwater Cube Pass",        //
    "Underwater Wake Pass 0",      //
    "Underwater Wake Pass 1",      //
    "Underwater Post Pass",        //
    "Underwater Over Pass",        //
    "Gesture Hint Collar 0 Pass",  //
    "Gesture Hint Collar 1 Pass",  //
    "Gesture Hint Collar 2 Pass",  //
    "Gesture Hints Pass",          //
    "Cinema Bars Pass",            //
    "Advisors Pass",               //
    "Interface Pass",              //
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
	return pass == RenderPass::Main || pass == RenderPass::Reflection || pass == RenderPass::UnderwaterEnvironment;
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
	case RenderPass::UnderwaterEnvironment:
		return RenderPass::UnderwaterEnvironmentSky;
	default:
		return pass;
	}
}

} // namespace openblack::graphics
