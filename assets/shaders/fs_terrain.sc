$input v_texcoord0, v_texcoord1, v_lightColour, v_smallBumpFade, v_shadowCoord, v_haze, v_world

#include <bgfx_shader.sh>

#include "creature_shadow.sh"
#include "ground_shadow.sh"
#include "snow.sh"
#include "vortex_ground.sh"

#define M_PI 3.1415926535897932384626433832795

// The blocks' painted textures, a layer each: the land's colour, and the coast alpha that fades it into the sea
SAMPLER2DARRAY(s0_blockTextures, 0);
// The small bump detail's colour and alpha, 12 times across a block
SAMPLER2D(s1_smallBumpAlpha, 1);
SAMPLER2D(s2_smallBump, 2);
SAMPLER2D(s3_footprints, 3);
SAMPLER2D(s4_handShadow, 4);
// Where the shadows of trees, rocks and buildings cover the island, laid out as the footprints are
SAMPLER2D(s5_objectShadows, 5);
// What of the land's alpha the rivers' channels leave, laid out as the footprints are
SAMPLER2D(s8_landAlpha, 8);

// The noise that makes the snow's edges ragged, once across each land block of 160 units
SAMPLER2D(s12_snowNoise, 12);
// The ring an open vortex lays over the land around its hole
SAMPLER2D(s13_vortexRing, 13);

// The land under the snow lying on it: whiter the deeper the snow over each texel of the noise, from its own colour to
// a grey that is blue-white where it lies thickest. Where no corner of its cell has more than a little snow, none shows.
vec3 LandUnderSnow(vec3 colour, vec2 xz)
{
	vec2 cell = floor(xz * 0.025f);
	float most = max(max(SnowDepthCell(cell), SnowDepthCell(cell + vec2(1.0f, 0.0f))),
	                 max(SnowDepthCell(cell + vec2(0.0f, 1.0f)), SnowDepthCell(cell + 1.0f)));
	if (most <= 5.0f)
	{
		return colour;
	}
	vec2 texel = floor(fract(xz / 160.0f) * 256.0f);
	float noise = floor(texture2DLod(s12_snowNoise, (texel + 0.5f) / 256.0f, 0.0f).r * 255.0f + 0.5f);
	float level = clamp(floor((floor(SnowDepthAt(xz)) - noise) / 8.0f), 0.0f, 16.0f);
	if (level <= 0.0f)
	{
		return colour;
	}
	float white = 14.0f + mod(floor(noise / 4.0f) + floor(noise / 2.0f), 2.0f);
	if (level >= 16.0f)
	{
		return vec3(white, white, 15.0f) / 15.0f;
	}
	return colour * (16.0f - level) / 16.0f + vec3_splat(floor(white * level / 16.0f) / 15.0f);
}

// x: the block's layer of s0_blockTextures
uniform vec4 u_block;
// y: how bright the land's light is, a half for the land mirrored under the sea
// z: the small bump detail's strength
// w: 1 to draw the land's textures alone, unlit and with no sea, as the temple's map is textured with
uniform vec4 u_skyAndBump;
// x: darkness of the objects' shadows where they fully cover a texel, 0 without them
// yz: size of a texel of s5_objectShadows
uniform vec4 u_objectShadows;
// x: darkness of the hand's shadow where its silhouette fully covers a texel, 0 without a shadow
// y: how far before the hand along the light the shadow starts
uniform vec4 u_handShadow;
// zw: the block's size in units
uniform vec4 u_blockPositionAndSize;
// xy: the island's lowest corner, zw: its highest, in units
uniform vec4 u_islandExtent;

// Texels along a side of a block's texture
#define BLOCK_TEXELS 256.0f

// A texel of the block's texture with the footprints painted into it, as the game paints them: the footprints' colour
// mixed into the block's 4-bit colour by how much they cover it, each channel rounded down to 4 bits again. The block
// keeps its coast alpha. blockTexel is the texel along the block texture's axes (z, then x); islandUv is where the
// footprints have the same point.
vec4 PaintedTexel(vec2 blockTexel, vec2 islandUv)
{
	vec4 land = texture2DArrayLod(s0_blockTextures, vec3((blockTexel + 0.5f) / BLOCK_TEXELS, u_block.x), 0.0f);
	vec4 footprints = texture2DLod(s3_footprints, islandUv, 0.0f);
	// The mix's exact result is a whole number of 255ths of a level: a thousandth of a level absorbs the rounding of
	// what is stored without lifting any result to the next level
	vec3 painted = floor((land.rgb * (1.0f - footprints.a) + footprints.rgb) * 15.0f + 0.001f) / 15.0f;
	return vec4(painted, land.a);
}

// The block's painted texture filtered across its texels: the four nearest texels, each painted, blended by distance
// as the block's texture is filtered, each block on its own. blockUv is where the fragment is on the block's texture,
// islandUv where it is on the footprints.
vec4 PaintedBlock(vec2 blockUv, vec2 islandUv)
{
	vec2 position = blockUv * BLOCK_TEXELS - 0.5f;
	vec2 first = floor(position);
	vec2 blend = position - first;
	// Island texture coordinates a texel of the block's texture moves by. The block's texture runs along z then x, the
	// island's along x then z, upside down where the island's textures are.
	vec2 unitsPerTexel = u_blockPositionAndSize.wz / BLOCK_TEXELS;
	vec2 islandPerUnit = 1.0f / (u_islandExtent.zw - u_islandExtent.xy);
	#if BGFX_SHADER_LANGUAGE_GLSL
		vec2 islandPerTexel = unitsPerTexel.yx * islandPerUnit;
	#else
		vec2 islandPerTexel = unitsPerTexel.yx * islandPerUnit * vec2(1.0f, -1.0f);
	#endif
	vec2 last = vec2_splat(BLOCK_TEXELS - 1.0f);
	vec2 low = clamp(first, vec2_splat(0.0f), last);
	vec2 high = clamp(first + 1.0f, vec2_splat(0.0f), last);
	vec4 texels[4];
	texels[0] = PaintedTexel(low, islandUv + (low - position).yx * islandPerTexel);
	texels[1] = PaintedTexel(vec2(high.x, low.y), islandUv + (vec2(high.x, low.y) - position).yx * islandPerTexel);
	texels[2] = PaintedTexel(vec2(low.x, high.y), islandUv + (vec2(low.x, high.y) - position).yx * islandPerTexel);
	texels[3] = PaintedTexel(high, islandUv + (high - position).yx * islandPerTexel);
	return mix(mix(texels[0], texels[1], blend.x), mix(texels[2], texels[3], blend.x), blend.y);
}

void main()
{
	// unpack uniforms
	float skyType = u_skyAndBump.x;
	float smallBumpMapStrength = u_skyAndBump.z;

	// The block's texture with the footprints painted into it, filtered across its texels
	vec4 block = PaintedBlock(v_texcoord0.xy, v_texcoord1.xy);
	vec4 col = vec4(block.rgb, 1.0f);

	// The objects' shadows are baked into the land's textures over the footprints, under the light. The game counts
	// how many of eight samples in each texel are covered; four filtered taps across a texel soften the edges as much.
	vec2 texel = u_objectShadows.yz * 0.5f;
	float objectShadow = 0.25f * (
		texture2D(s5_objectShadows, v_texcoord1.xy + vec2(-texel.x, -texel.y)).r +
		texture2D(s5_objectShadows, v_texcoord1.xy + vec2(texel.x, -texel.y)).r +
		texture2D(s5_objectShadows, v_texcoord1.xy + vec2(-texel.x, texel.y)).r +
		texture2D(s5_objectShadows, v_texcoord1.xy + vec2(texel.x, texel.y)).r);
	col.rgb = col.rgb * (1.0f - objectShadow * u_objectShadows.x);

	// The snow lies over all of it, under the light
	if (u_snow.x > 0.5f)
	{
		col.rgb = LandUnderSnow(col.rgb, v_texcoord1.zw);
	}

	if (u_skyAndBump.w > 0.0f)
	{
		gl_FragColor = vec4(col.rgb, 1.0f);
		return;
	}

	// Where the vortex's hole texture is clearer than its threshold no land is drawn, not even its depth, and what was
	// drawn before the land shows through
	bool vortex = u_vortexGround.w >= 0.0f;
	vec4 ring = vec4_splat(0.0f);
	if (vortex)
	{
		vec2 vortexUv = VortexGroundUv(v_texcoord1.zw);
		if (InVortexHole(texture2D(s6_vortexHole, vortexUv)))
		{
			discard;
		}
		ring = texture2D(s13_vortexRing, vortexUv);
	}

	col.rgb = col.rgb * v_lightColour * v_haze.a;

	// the hand's shadow, projected along the sunlight onto the land beyond it
	vec3 shadowCoord = v_shadowCoord.xyz;
	if (u_handShadow.x > 0.0f && shadowCoord.z > u_handShadow.y && shadowCoord.x > 0.0f && shadowCoord.x < 1.0f &&
	    shadowCoord.y > 0.0f && shadowCoord.y < 1.0f)
	{
		float coverage = texture2D(s4_handShadow, shadowCoord.xy).r;
		col.rgb = col.rgb * (1.0f - coverage * v_shadowCoord.w * u_handShadow.x);
	}
	// The creatures' shadows, which fade out towards the water's edge as the hand's does
	if (u_creatureShadowInfo.x > 0.0f && v_shadowCoord.w > 0.0f)
	{
		col.rgb = col.rgb * CreatureShadowLight(v_world);
	}

	// The small bump detail is a second layer over the lit land, its colour unlit, blended by its alpha whatever the
	// coast alpha. The distance haze is added to both. The land is blended over the sea by its coast alpha, and writes
	// its depth even where it is clear. Both layers are drawn at once, premultiplied: what of the sea shows through is
	// what neither layer covers.
	vec2 smallBumpUv = v_texcoord0.xy * 12.0f;
	vec3 smallBump = texture2D(s2_smallBump, smallBumpUv).rgb * v_smallBumpFade.x;
	float bumpAlpha = texture2D(s1_smallBumpAlpha, smallBumpUv).r * v_smallBumpFade.y * smallBumpMapStrength;
	vec3 land = min(col.rgb + v_haze.rgb, vec3_splat(1.0f));
	vec3 detail = min(smallBump + v_haze.rgb, vec3_splat(1.0f));
	// The rivers' channels lower the coast alpha where they run, so the sea shows through as their water
	float landAlpha = min(block.a, texture2D(s8_landAlpha, v_texcoord1.xy).r);
	vec4 premultiplied = vec4(land * landAlpha * (1.0f - bumpAlpha) + detail * bumpAlpha,
	                          1.0f - (1.0f - landAlpha) * (1.0f - bumpAlpha));
	if (vortex)
	{
		// The hole texture's colour was added under the land before it (where the land lets the sea through, it shows).
		// The ring, lit as the land, is blended over all of it by its alpha.
		vec3 ringColour = min(ring.rgb * v_lightColour * v_haze.a + v_haze.rgb, vec3_splat(1.0f));
		premultiplied = vec4(ringColour * ring.a + premultiplied.rgb * (1.0f - ring.a),
		                     1.0f - (1.0f - premultiplied.a) * (1.0f - ring.a));
	}
	// The high-detail villagers' shadows are laid over all of it in black, after the land is drawn
	if (u_groundShadowInfo.x > 0.0f && v_shadowCoord.w > 0.0f)
	{
		premultiplied.rgb = premultiplied.rgb * GroundShadowsLight(v_world, v_shadowCoord.w);
	}
	gl_FragColor = premultiplied;
}
