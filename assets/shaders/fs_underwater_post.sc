$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// The water reads the field of the wakes and the hulls (see underwater_wake.sh)
#define UW_WAKE
#include "underwater.sh"

// EXPERIMENT (not for merge): the experiment's water. The scene is drawn into its own target, with no sea, and this
// pass draws it to the window with the water over it.
// Above the surface: the sea's surface with its waves, the scene under it bent by them and faded by the water, the
// reflections of the scene on it found in the screen (the sky where they leave it), and foam where the water is
// shallow.
// Under the surface: the fog and the absorption of the light, the shafts of light from the surface, the surface seen
// from below (the sky bent through it, a mirror at low angles), the wobble of the water, its colour, the dark edge of
// the screen, and the line where the surface cuts the view as the camera goes through it.

SAMPLER2D(s_sceneColour, 0);
SAMPLER2D(s_sceneDepth, 1);
// The shore map (see UwShoreUv): the distance to the coast, the direction to it and the height of the ground
SAMPLER2D(s_shore, 2);
// The land's block textures, a layer for each block: their alpha is the original game's coast alpha, which fades the
// land into the sea under it
SAMPLER2DARRAY(s_blockTextures, 3);
// The layer of each block of the 32 by 32 map, plus 1; 0 where there is no block
SAMPLER2D(s_blockLayers, 4);

// The cube of the far scene round the eye: its colour, and its depth for how far away the scene is in each direction
SAMPLERCUBE(s_environment, 5);
SAMPLERCUBE(s_environmentDepth, 6);
// The sky alone, for the rays that meet no scene: the scene near the middle of the cube can hide the sky in the first
SAMPLERCUBE(s_environmentSky, 7);

// rgb: the sky's colour, as the haze takes it; w: the near distance of the view
uniform vec4 u_uwSky;
// xyz: where the middle of the cube of the far scene is
uniform vec4 u_uwEnvironment;

// The foam with a thickness, and the silt in the water
#include "underwater_foam_shade.sh"
#include "underwater_sediment.sh"

// Where a point of the screen (0 to 1, as the textures are read) is in the world, at a depth of the depth buffer
vec3 WorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

// Where a point of the world is on the screen (xy, 0 to 1 as the textures are read) and its depth (z) as the depth
// buffer keeps it
vec3 ScreenAt(vec4 clip)
{
	vec3 ndc = clip.xyz / clip.w;
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec3(ndc.xy * 0.5f + 0.5f, ndc.z * 0.5f + 0.5f);
#else
	return vec3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z);
#endif
}

// x: the things that float, where they are; w: 1 for each that there is
uniform vec4 u_uwFloaters[8];

// 1 while the waves show, 0 while they are turned off to measure them
float WavesOn()
{
	return u_uwWaves.x > 0.0f ? 1.0f : 0.0f;
}

// One texel of a block's texture, at a texel along the texture's axes (z, then x), kept inside the block
float BlockAlphaTexel(vec2 texel, float layer)
{
	vec2 inside = clamp(texel, vec2_splat(0.0f), vec2_splat(255.0f));
	return texture2DArrayLod(s_blockTextures, vec3((inside + 0.5f) / 256.0f, layer), 0.0f).a;
}

// How much the original game shows its land over its sea at a point, 0 to 1: the coast alpha of the block's texture
// there, blended between its four nearest texels. It is 0 where there is no block. The water shows only where the
// original sea shows, and as much as it shows: the land is never under the water where the original game shows land.
float OriginalLandAt(vec2 xz)
{
	vec2 block = floor(xz / 160.0f);
	if (block.x < 0.0f || block.y < 0.0f || block.x > 31.0f || block.y > 31.0f)
	{
		return 0.0f;
	}
	float layer = texture2DLod(s_blockLayers, (block + 0.5f) / 32.0f, 0.0f).r;
	if (layer < 0.5f)
	{
		return 0.0f;
	}
	layer -= 1.0f;
	vec2 texel = (xz - block * 160.0f).yx * (256.0f / 160.0f) - 0.5f;
	vec2 first = floor(texel);
	vec2 f = texel - first;
	float low = mix(BlockAlphaTexel(first, layer), BlockAlphaTexel(first + vec2(1.0f, 0.0f), layer), f.x);
	float high = mix(BlockAlphaTexel(first + vec2(0.0f, 1.0f), layer), BlockAlphaTexel(first + vec2(1.0f, 1.0f), layer), f.x);
	return mix(low, high, f.y);
}

// The shore map at a point
vec4 ShoreAt(vec2 xz)
{
	return texture2DLod(s_shore, UwShoreUv(xz), 0.0f);
}

// The height of the surface at a point near the eye
float SurfaceHeight(vec2 p, float t)
{
	vec2 slope;
	float face;
	return UwSurface(p, t, 0.0f, ShoreAt(p), slope, face);
}

// The steps that a ray takes to find the surface: more near a rogue wave, which makes the surface high and steep. The
// steps after the third go only part of the way, so that they do not jump across the steep face. `onLevel` is where the
// ray meets the sea's level, and `down` is how steeply the ray goes down.
int RogueSteps(vec2 onLevel, float down)
{
	vec4 a = u_uwRogue[0];
	vec4 b = u_uwRogue[1];
	if (a.w <= 0.0f)
	{
		return 3;
	}
	vec2 along = vec2(cos(a.z), sin(a.z));
	vec2 offset = onLevel - a.xy;
	float side = dot(offset, vec2(-along.y, along.x)) / b.y;
	float u = (dot(offset, along) + b.w * b.x * side * side) / b.x;
	// The wave moves the point where the ray meets the surface by up to its height over the steepness of the ray
	float reach = a.w / max(down, 0.05f) / b.x;
	return abs(side) < 2.2f && u > -1.8f - reach && u < 1.1f + reach ? 8 : 3;
}

// Where a ray from the eye meets the sea's surface: the surface's height is found again three times at the point the
// ray reaches, from the sea's level. `along` is how far along the ray, `slope` and `face` as UwSurface gives them,
// and `shore` the shore map there.
float RayToSurface(vec3 eye, vec3 direction, float t, out vec2 slope, out float face, out vec4 shore)
{
	float along = (u_uwParams.z - eye.y) / direction.y;
	slope = vec2_splat(0.0f);
	face = 0.0f;
	shore = vec4(1000.0f, 0.0f, 0.0f, -160.0f);
	int steps = RogueSteps(eye.xz + direction.xz * max(along, 0.0f), abs(direction.y));
	for (int i = 0; i < steps; i++)
	{
		vec2 at = eye.xz + direction.xz * max(along, 0.0f);
		shore = ShoreAt(at);
		float height = UwSurface(at, t, max(along, 0.0f), shore, slope, face);
		along = mix(along, (height - eye.y) / direction.y, i < 3 ? 1.0f : 0.6f);
	}
	return along;
}

// The rings on the water round the things that float, as they bob: the slope they add to the surface and their foam
vec3 FloaterRings(vec2 p, float t)
{
	vec3 rings = vec3_splat(0.0f);
	for (int i = 0; i < 8; i++)
	{
		vec4 floater = u_uwFloaters[i];
		if (floater.w > 0.5f)
		{
			vec2 away = p - floater.xz;
			float d = length(away);
			float wave = sin(d * 2.2f - t * 3.0f) * exp(-d * 0.22f) * smoothstep(0.3f, 1.2f, d);
			rings.xy += away / max(d, 1e-3f) * wave * 0.25f;
			rings.z += (1.0f - smoothstep(0.8f, 2.5f, d)) * (0.5f + 0.5f * UwNoise(p * 3.0f + t));
		}
	}
	return rings;
}

// The sky seen in a direction, for the reflections that leave the screen: brighter towards the horizon
vec3 SkyIn(vec3 direction)
{
	float up = clamp(direction.y, 0.0f, 1.0f);
	return u_uwSky.rgb * mix(1.05f, 0.8f, up);
}

// The reflection of the scene on the surface at p, along r, found by a march across the screen: 16 steps out to 150
// units, the nearer more closely, then 4 steps to close in on the hit. The scene under the surface is not reflected.
// alpha is how much of the hit to take: it fades at the screen's edges and far along the ray.
vec4 ScreenReflection(vec3 p, vec3 r, float jitter)
{
	vec4 start = mul(u_viewProj, vec4(p, 1.0f));
	vec4 end = mul(u_viewProj, vec4(p + r * 150.0f, 1.0f));
	// Not behind the eye
	if (end.w < 0.5f)
	{
		end = mix(start, end, (start.w - 0.5f) / max(start.w - end.w, 1e-4f));
	}
	float before = 0.0f;
	float found = -1.0f;
	for (int i = 1; i <= 16; i++)
	{
		float s = (float(i) - 1.0f + jitter) / 16.0f;
		s = s * s;
		vec3 at = ScreenAt(mix(start, end, s));
		if (at.x < 0.0f || at.x > 1.0f || at.y < 0.0f || at.y > 1.0f)
		{
			break;
		}
		float scene = texture2DLod(s_sceneDepth, at.xy, 0.0f).r;
		// The depth is reversed: the ray is behind the scene where its depth is smaller
		if (scene > 0.0f && at.z < scene && WorldAt(at.xy, scene).y > u_uwParams.z - 0.2f)
		{
			found = s;
			break;
		}
		before = s;
	}
	if (found < 0.0f)
	{
		return vec4_splat(0.0f);
	}
	float low = before;
	float high = found;
	for (int j = 0; j < 4; j++)
	{
		float mid = (low + high) * 0.5f;
		vec3 at = ScreenAt(mix(start, end, mid));
		float scene = texture2DLod(s_sceneDepth, at.xy, 0.0f).r;
		if (at.z < scene)
		{
			high = mid;
		}
		else
		{
			low = mid;
		}
	}
	vec3 hit = ScreenAt(mix(start, end, high));
	vec2 edge = abs(hit.xy * 2.0f - 1.0f);
	float alpha = (1.0f - smoothstep(0.8f, 1.0f, max(edge.x, edge.y))) * (1.0f - smoothstep(0.6f, 1.0f, high));
	return vec4(texture2DLod(s_sceneColour, hit.xy, 0.0f).rgb, alpha);
}

// The far sea seen from above: the waves as each pixel shows them, the reflection of the far scene and the glint.

// The size of a pixel on the water, for a ray from the eye that goes this far to the water (see UwFootprint). With
// no filter, it is the old round pixel.
vec4 FootprintOnWater(vec3 direction, float along)
{
	if (u_uwSea.x < 0.5f)
	{
		return UwFootprint(along);
	}
	vec2 ground = direction.xz;
	float groundLength = max(length(ground), 1e-4f);
	// A pixel is longer along the view the more the view grazes the water
	return vec4(u_uwSea.z * max(along, 1.0f), ground / groundLength, 1.0f / max(-direction.y, 0.02f));
}

// Where a ray from the eye meets the sea's surface from above, as RayToSurface finds it, with the waves as the pixel
// shows them. `variance` is the variance of the slope that the pixel cannot show.
float RayToSurfaceFiltered(vec3 eye, vec3 direction, float t, out vec2 slope, out float face, out vec4 shore,
                           out float variance)
{
	float along = (u_uwParams.z - eye.y) / direction.y;
	slope = vec2_splat(0.0f);
	face = 0.0f;
	variance = 0.0f;
	shore = vec4(1000.0f, 0.0f, 0.0f, -160.0f);
	int steps = RogueSteps(eye.xz + direction.xz * max(along, 0.0f), abs(direction.y));
	for (int i = 0; i < steps; i++)
	{
		vec2 at = eye.xz + direction.xz * max(along, 0.0f);
		shore = ShoreAt(at);
		float height = UwSurfaceFiltered(at, t, FootprintOnWater(direction, max(along, 0.0f)), shore, slope, face, variance);
		along = mix(along, (height - eye.y) / direction.y, i < 3 ? 1.0f : 0.6f);
	}
	return along;
}

// How far away the far scene is from the middle of the cube along a direction, or 0 where the cube shows the sky.
// The faces of the cube have a near distance of 1 (see EnvironmentCamera), and their depth is reversed.
float EnvironmentDistance(vec3 direction)
{
	float depth = textureCubeLod(s_environmentDepth, direction, 0.0f).r;
#if BGFX_SHADER_LANGUAGE_GLSL
	depth = depth * 2.0f - 1.0f;
#endif
	if (depth <= 1e-6f)
	{
		return 0.0f;
	}
	vec3 a = abs(direction);
	float axis = max(a.x, max(a.y, a.z));
	return (1.0f / depth) * length(direction) / axis;
}

// Where a point (from the middle of the cube) is against the far scene that the cube shows in its direction: 0 in front
// of the scene or where the cube shows the sky, 1 just behind the scene, 2 far behind it. The scene that hides a point
// far behind it from the middle of the cube does not always hide that point from the water.
float EnvironmentSide(vec3 q)
{
	float scene = EnvironmentDistance(q);
	float range = length(q);
	if (scene <= 0.0f || range < scene)
	{
		return 0.0f;
	}
	return range < scene + max(40.0f, scene * 0.1f) ? 1.0f : 2.0f;
}

// Where a ray from p along r first meets the far scene that the cube shows, seen from the middle of the cube, or zero
// when it meets nothing. `o` is p from the middle. The ray takes steps that grow longer
// with the distance, then two halving steps close in on the scene. The ray meets the scene only where it goes from in
// front of the scene to behind it: a point that the middle of the cube cannot see tells nothing.
vec3 EnvironmentHit(vec3 o, vec3 r)
{
	float before = 0.0f;
	float along = 6.0f;
	bool seen = EnvironmentSide(o) < 0.5f;
	for (int i = 0; i < 14; i++)
	{
		float side = EnvironmentSide(o + r * along);
		if (side < 0.5f)
		{
			seen = true;
		}
		else if (seen && side < 1.5f)
		{
			float low = before;
			float high = along;
			for (int j = 0; j < 2; j++)
			{
				float mid = (low + high) * 0.5f;
				if (EnvironmentSide(o + r * mid) > 0.5f)
				{
					high = mid;
				}
				else
				{
					low = mid;
				}
			}
			return o + r * high;
		}
		else
		{
			seen = false;
		}
		before = along;
		along *= 1.7f;
	}
	return vec3_splat(0.0f);
}

// The reflection of the far scene on the water at p along r, from the cube. The cube shows the scene from its middle,
// not from p, so the shader reads the cube in the direction of the point where the ray meets the scene. A rough
// surface takes the light from a wider cone of directions.
vec3 EnvironmentReflection(vec3 p, vec3 r, float roughness)
{
	vec3 hit = EnvironmentHit(p - u_uwEnvironment.xyz, r);
	bool met = dot(hit, hit) > 0.0f;
	// The cube has no water under its horizon: the reflection reads no lower than the horizon
	vec3 lookup = met ? normalize(hit) : r;
	lookup = normalize(vec3(lookup.x, max(lookup.y, 0.004f), lookup.z));
	// A rough surface takes four more samples round the middle one, as far out as the cone of the rough surface
	vec3 side = normalize(cross(lookup, abs(lookup.y) < 0.9f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)));
	vec3 up = cross(side, lookup);
	float spread = min(roughness, 0.6f) * 0.6f;
	vec3 colour = vec3_splat(0.0f);
	if (met)
	{
		colour = textureCubeLod(s_environment, lookup, 0.0f).rgb;
		if (roughness > 0.1f)
		{
			colour += textureCubeLod(s_environment, lookup + side * spread, 0.0f).rgb;
			colour += textureCubeLod(s_environment, lookup - side * spread, 0.0f).rgb;
			colour += textureCubeLod(s_environment, lookup + up * spread, 0.0f).rgb;
			colour += textureCubeLod(s_environment, lookup - up * spread, 0.0f).rgb;
			colour *= 0.2f;
		}
	}
	else
	{
		// A ray that meets nothing goes to the sky
		colour = textureCubeLod(s_environmentSky, lookup, 0.0f).rgb;
		if (roughness > 0.1f)
		{
			colour += textureCubeLod(s_environmentSky, lookup + side * spread, 0.0f).rgb;
			colour += textureCubeLod(s_environmentSky, lookup - side * spread, 0.0f).rgb;
			colour += textureCubeLod(s_environmentSky, lookup + up * spread, 0.0f).rgb;
			colour += textureCubeLod(s_environmentSky, lookup - up * spread, 0.0f).rgb;
			colour *= 0.2f;
		}
	}
	return colour;
}

// The glint of the sun or the moon on the water: the light that the waves reflect towards the eye, from a GGX
// microfacet model. The waves that the pixel cannot show make the roughness larger. Thus the far glint spreads into a
// long, soft path across the sea, and does not show as single bright pixels.
float SeaGlint(vec3 normal, vec3 toEye, vec3 toLight, float roughness)
{
	vec3 halfway = normalize(toEye + toLight);
	float nl = clamp(dot(normal, toLight), 0.0f, 1.0f);
	float nv = clamp(dot(normal, toEye), 1e-3f, 1.0f);
	float nh = clamp(dot(normal, halfway), 0.0f, 1.0f);
	float vh = clamp(dot(toEye, halfway), 0.0f, 1.0f);
	float a2 = roughness * roughness;
	float d = nh * nh * (a2 - 1.0f) + 1.0f;
	float distribution = a2 / (3.14159265f * d * d);
	float visibility = 0.5f / max(nl * sqrt(nv * nv * (1.0f - a2) + a2) + nv * sqrt(nl * nl * (1.0f - a2) + a2), 1e-4f);
	float fresnel = 0.02f + 0.98f * pow(1.0f - vh, 5.0f);
	return distribution * visibility * fresnel * nl;
}

// How much light the water scatters towards the eye along the ray, out to a distance: the shafts of light. The ray is
// sampled in steps, and each sample takes the light that came down to it through the surface along the light's
// direction, where the surface's bright and dark patches make the shafts.
vec3 Shafts(vec3 eye, vec3 direction, float range, float t, float jitter)
{
	const int k_Steps = 14;
	float reach = min(range, 70.0f);
	float stepLength = reach / float(k_Steps);
	vec3 towards = u_uwLight.xyz;
	vec3 colour = vec3_splat(0.0f);
	for (int i = 0; i < k_Steps; i++)
	{
		float along = (float(i) + jitter) * stepLength;
		vec3 p = eye + direction * along;
		float depth = u_uwParams.z - p.y;
		if (depth > 0.0f)
		{
			vec2 surface = p.xz + towards.xz / towards.y * depth;
			float bright = UwSurfaceLight(surface, t * (1.0f + 2.0f * u_uwWaves.y));
			// The light lost on its way down to the sample, and on its way from the sample to the eye
			vec3 lost = UwTransmittance(depth / towards.y + along);
			colour += lost * bright;
		}
	}
	// Water scatters the light mostly forwards: the shafts are brightest looking towards the light
	float cosine = dot(direction, towards);
	float g = 0.55f;
	float phase = (1.0f - g * g) / pow(max(1.0f + g * g - 2.0f * g * cosine, 1e-4f), 1.5f) * 0.08f;
	// A storm's rough surface and its clouds break the shafts up and darken them
	return colour * stepLength * phase * u_uwLightColour.rgb * (1.0f - 0.55f * u_uwWaves.y);
}

// The foam where the water is shallow: in patches that break up and drift, and in lines that run in to the shore with
// the waves. depth is how deep the water is under the point.
float Foam(vec2 p, float depth, float t)
{
	// The shallows of a gentle beach go down slowly, so the bands are thin in depth: a wide band of foam would cover them
	float shallow = 1.0f - smoothstep(0.0f, 0.25f, depth);
	if (shallow <= 0.0f)
	{
		return 0.0f;
	}
	// Two layers of noise that drift different ways, turned so that their cells do not line up. The chunks are small
	// (cells of about a metre) and cover a quarter of the water that the foam covered before.
	vec2 q = vec2(p.x * 0.8f - p.y * 0.6f, p.x * 0.6f + p.y * 0.8f);
	float patches = UwFoamNoise(q * 1.4f + vec2(t * 0.25f, -t * 0.18f)) * 0.6f + UwFoamNoise(p * 3.8f - vec2(t * 0.4f, t * 0.31f)) * 0.4f;
	float lines = 0.5f + 0.5f * sin(depth * 9.0f - t * 2.2f + UwNoise(p * 0.8f) * 4.0f);
	float edge = 1.0f - smoothstep(0.0f, 0.03f, depth);
	float foam = smoothstep(0.7f, 0.9f, patches * 0.8f + lines * 0.3f) * shallow * shallow;
	return clamp(max(foam, edge * smoothstep(0.45f, 0.7f, patches)), 0.0f, 1.0f);
}

// The storm on the sea: the rogue wave and the lightning.

// The white water of the rogue wave at a point of the surface: on its crest as it breaks, and in streaks down its
// steep face. `shore` is the shore map at the point.
float RogueFoam(vec2 p, vec4 shore, float t)
{
	if (u_uwRogue[0].w <= 0.0f || u_uwRogue[1].z <= 0.0f)
	{
		return 0.0f;
	}
	vec2 slope;
	float crest;
	UwRogueWave(p, shore.x, u_uwParams.z - shore.w, slope, crest);
	vec2 along = vec2(cos(u_uwRogue[0].z), sin(u_uwRogue[0].z));
	vec2 q = vec2(dot(p, along), dot(p, vec2(-along.y, along.x)));
	// The face is where the surface falls steeply ahead of the crest
	float steep = smoothstep(0.25f, 0.65f, -dot(slope, along));
	float churn = UwFoamNoise(q * vec2(0.6f, 0.9f) - vec2(t * 1.1f, 0.0f));
	float streaks = smoothstep(0.55f, 0.8f, UwNoise(vec2(q.x * 0.24f - t * 0.9f, q.y * 1.1f)));
	float top = smoothstep(0.6f, 0.95f, crest) * smoothstep(0.4f, 0.65f, churn);
	float face = steep * smoothstep(0.2f, 0.5f, crest) * streaks;
	// The white water that the breaking crest leaves on its back, in patches that break up
	float back = smoothstep(0.1f, 0.5f, crest) * smoothstep(0.0f, 0.15f, dot(slope, along)) * smoothstep(0.6f, 0.8f, churn);
	return clamp((top + face + back * 0.6f) * 1.2f * u_uwRogue[1].z, 0.0f, 1.0f);
}

// The extra light of the lightning on the colour of the water: the flash of the sky lights the water and what is in
// it. Under the surface the light is less the deeper the eye is.
vec3 LightningInWater()
{
	float depth = max(u_uwParams.w, 0.0f);
	return UW_FLASH_COLOUR * u_uwStrike[1].x * 0.3f * UwTransmittance(depth * 0.7f) * u_uwStrike[1].w;
}

// The flash of the sky in the reflections that the screen does not give
vec3 LightningInSky(vec3 reflected)
{
	return UW_FLASH_COLOUR * u_uwStrike[1].x * (0.55f + 0.45f * clamp(reflected.y, 0.0f, 1.0f)) * u_uwStrike[1].w;
}

// The glint of the bolt on the waves: the bolt is a line of light from the sea up to the cloud. The point of the line
// nearest to the reflected ray stands for the line, so the glint is a long path on the water towards the strike. Near
// the strike the bolt also lights the water round it.
vec3 LightningGlint(vec3 p, vec3 normal, vec3 toEye, float roughness)
{
	float bolt = u_uwStrike[0].w * u_uwStrike[1].w;
	if (bolt <= 0.0f)
	{
		return vec3_splat(0.0f);
	}
	vec3 base = u_uwStrike[0].xyz;
	vec3 r = reflect(-toEye, normal);
	// The point on the bolt nearest to the reflected ray
	vec3 w = p - base;
	float b = r.y;
	float d = dot(r, w);
	float denominator = max(1.0f - b * b, 1e-4f);
	float height = clamp((w.y - b * d) / denominator, 0.0f, 220.0f);
	vec3 toBolt = base + vec3(0.0f, height, 0.0f) - p;
	float distance = length(toBolt);
	vec3 toLight = toBolt / max(distance, 1e-3f);
	float falloff = 1.0f / (1.0f + distance * distance / (450.0f * 450.0f));
	float glint = min(SeaGlint(normal, toEye, toLight, max(roughness, 0.08f)), 20.0f);
	float near = exp(-length(w.xz) / 120.0f) * 0.35f;
	return UW_FLASH_COLOUR * bolt * (glint * 1.4f * falloff + near);
}

// The ring that a strike on the sea sends out: x, y its slope on the water, z its brightness. It flashes white at the
// point first, and then the ring spreads and fades (see underwater::StrikeRingRadius).
vec3 StrikeRing(vec2 p)
{
	float strength = u_uwStrike[1].z * u_uwStrike[1].w;
	float since = u_uwStrike[1].y;
	if (strength <= 0.0f || since > 4.0f)
	{
		return vec3_splat(0.0f);
	}
	vec2 away = p - u_uwStrike[0].xz;
	float d = length(away);
	float radius = 2.0f + 30.0f * sqrt(max(since, 0.0f));
	float width = 1.5f + 2.0f * since;
	float s = (d - radius) / width;
	float fade = 1.0f - smoothstep(0.0f, 2.6f, since);
	float ring = exp(-s * s) * fade;
	float core = (1.0f - smoothstep(0.0f, 0.3f, since)) * exp(-d / 7.0f);
	vec2 slope = away / max(d, 1e-3f) * (-2.0f * s / width) * ring * 0.6f;
	return vec3(slope * strength, (ring * 0.8f + core * 3.0f) * strength);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	float t = u_uwParams.y;
	vec3 eye = u_uwEye.xyz;
	vec3 direction = normalize(WorldAt(uv, 0.5f) - eye);
	float jitter = fract(52.9829189f * fract(dot(gl_FragCoord.xy, vec2(0.06711056f, 0.00583715f))));
	// Where the view's near plane is at this pixel, and whether it is under the surface: the line between the two
	// sides is the surface cutting the view
	vec3 nearPoint = eye + direction * max(u_uwSky.w, 0.1f) * 1.5f;
	float underBy = SurfaceHeight(nearPoint.xz, t) - nearPoint.y;
	float lineWidth = max(fwidth(underBy), 1e-4f);
	bool under = underBy > 0.0f;

	// Under the water, the water bends what is seen through it a little, moving in time
	vec2 wobble = vec2(sin(uv.y * 38.0f + t * 2.1f) + sin(uv.y * 71.0f - t * 1.3f) * 0.5f,
	                   cos(uv.x * 33.0f + t * 1.7f) + cos(uv.x * 63.0f + t * 2.6f) * 0.5f) * 0.0016f;
	vec2 sampleUv = under ? clamp(uv + wobble, vec2_splat(0.001f), vec2_splat(0.999f)) : uv;
	float depth = texture2DLod(s_sceneDepth, sampleUv, 0.0f).r;
	vec3 scene = texture2DLod(s_sceneColour, sampleUv, 0.0f).rgb;
	// Nothing drawn (the clear depth) is far away
	bool sky = depth <= 0.0f;
	vec3 world = sky ? eye + direction * 10000.0f : WorldAt(sampleUv, depth);
	float sceneDistance = length(world - eye);

	vec3 scatter = u_uwScatter.rgb;
	// The flash of the lightning lights the water for a moment
	scatter += LightningInWater();
	// Beyond the land's blocks nothing is drawn under the water: there the far floor of sand lies at the sea bed's
	// deepest, lit by what light comes down to it
	bool farFloor = sky && direction.y < 0.0f;
	vec3 floorColour = vec3(0.62f, 0.56f, 0.44f) *
	                   (u_uwLightColour.rgb * UwTransmittance(160.0f / max(u_uwLight.y, 0.2f)) * 0.6f + scatter * 0.6f);
	if (farFloor)
	{
		world = eye + direction * ((u_uwParams.z - 160.0f - eye.y) / direction.y);
		sceneDistance = length(world - eye);
	}
	vec3 colour = scene;
	// The depth that this pass writes: the scene's, and the water's surface where the water is in front of the scene
	float outDepth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	if (under)
	{
		// A little blur, more far away, as the particles in the water soften what is seen
		vec2 texel = u_viewTexel.xy * 1.5f;
		vec3 around = texture2DLod(s_sceneColour, sampleUv + vec2(texel.x, texel.y), 0.0f).rgb +
		              texture2DLod(s_sceneColour, sampleUv + vec2(-texel.x, texel.y), 0.0f).rgb +
		              texture2DLod(s_sceneColour, sampleUv + vec2(texel.x, -texel.y), 0.0f).rgb +
		              texture2DLod(s_sceneColour, sampleUv + vec2(-texel.x, -texel.y), 0.0f).rgb;
		float far = smoothstep(10.0f, 50.0f, sceneDistance);
		scene = mix(scene, around * 0.25f, 0.35f + 0.45f * far);

		// The distance along the ray to the surface, when the ray goes up to it before it meets the scene
		float toSurface = 1e9f;
		vec2 surfaceSlope = vec2_splat(0.0f);
		if (direction.y > 0.0f)
		{
			float face;
			vec4 shore;
			toSurface = max(RayToSurface(eye, direction, t, surfaceSlope, face, shore), 0.0f);
		}
		if (toSurface < sceneDistance)
		{
			// The surface seen from below. The light bends as it comes through: where the ray can go out, the sky and
			// what is above the water show through, bent by the waves; at low angles the surface is a mirror of the
			// dark water below.
			vec3 surfacePoint = eye + direction * toSurface;
			vec3 normal = UwSurfaceNormalOf(surfaceSlope, toSurface);
			vec3 leaving = refract(direction, -normal, 1.333f);
			float ripple = UwSurfaceLight(surfacePoint.xz * 1.7f, t * 1.8f);
			vec3 mirror = scatter * (0.55f + 0.35f * ripple);
			vec3 surface = mirror;
			if (dot(leaving, leaving) > 0.0f)
			{
				vec3 through = SkyIn(leaving) + LightningInSky(leaving);
				if (u_uwFlags.z > 0.5f)
				{
					// What is above the water, from where the bent ray points
					vec3 at = ScreenAt(mul(u_viewProj, vec4(surfacePoint + leaving * 60.0f, 1.0f)));
					if (at.x > 0.0f && at.x < 1.0f && at.y > 0.0f && at.y < 1.0f)
					{
						through = texture2DLod(s_sceneColour, at.xy, 0.0f).rgb;
					}
				}
				// Most of the light goes through near the middle of the window, and less towards its rim
				float window = smoothstep(0.0f, 0.25f, leaving.y);
				surface = mix(mirror, through * (1.1f + 0.4f * ripple) + u_uwLightColour.rgb * ripple * 0.2f, window);
			}
			if (u_uwFlags.y > 0.5f)
			{
				surface = FoamUnderside(surface, surfacePoint, toSurface * u_uwSea.z, scatter);
			}
			colour = mix(scatter, surface, UwTransmittance(toSurface));
		}
		else
		{
			// What is under the water: lit by the light that came down to its depth, then faded into the water's colour
			// by its distance, the red first
			float targetDepth = max(u_uwParams.z - world.y, 0.0f);
			vec3 litScene = farFloor ? floorColour : (sky ? vec3_splat(0.0f) : scene * UwTransmittance(targetDepth * 0.4f));
			vec3 left = UwTransmittance(sceneDistance);
			colour = litScene * left + scatter * (1.0f - left);
		}
		// The silt in the water between the eye and what it sees
		float siltMean;
		vec4 silt = UwSedimentAlong(eye, direction, min(sceneDistance, toSurface), t, jitter, 6, siltMean);
		colour = colour * silt.a + silt.rgb;
		// The shafts of light, broken by the moving surface; the jitter of each pixel hides the steps
		if (u_uwFlags.w > 0.5f)
		{
			colour += UwSedimentShafts(Shafts(eye, direction, min(sceneDistance, toSurface), t, jitter), siltMean, silt.a) *
			          u_uwLightColour.w;
		}

		// The colour of the water: a little less red and more contrast, and the edge of the screen darker
		colour = mix(vec3_splat(dot(colour, vec3(0.299f, 0.587f, 0.114f))), colour, 1.08f);
		colour = colour * vec3(0.94f, 1.0f, 1.03f);
		vec2 centre = uv - 0.5f;
		float vignette = 1.0f - smoothstep(0.35f, 0.85f, length(centre * vec2(1.25f, 1.0f)));
		colour *= mix(0.62f, 1.0f, vignette);
	}
	else if (direction.y < 0.0f)
	{
		// The surface seen from above, where the ray meets it before the scene. The water's height there is the sea's
		// level with the waves, higher where the water gets shallow, and the run of the waves up the beach. The
		// depth of the water is the depth under its level of what the scene shows at the pixel.
		vec2 surfaceSlope;
		float face;
		vec4 shore;
		float slopeVariance;
		float toSurface = RayToSurfaceFiltered(eye, direction, t, surfaceSlope, face, shore, slopeVariance);
		float roughness = UwRoughness(slopeVariance);
		float waterTop = eye.y + direction.y * toSurface;
		float waterDepth = waterTop - shore.w;
		float lift = waterTop - u_uwParams.z;
		vec3 surfacePoint = eye + direction * toSurface;
		// No water shows inside a boat's hull
		float insideBoat = UwInsideBoat(surfacePoint.xz);
		if (toSurface > 0.0f && (toSurface < sceneDistance || farFloor) && insideBoat < 0.999f)
		{
			vec3 ring = FloaterRings(surfacePoint.xz, t);
			// The filtered waves keep their slope far away: the roughness takes the waves that the pixel cannot show
			vec3 strike = StrikeRing(surfacePoint.xz);
			vec2 shownSlope = surfaceSlope + ring.xy + strike.xy;
			vec3 normal = u_uwSea.x > 0.5f ? normalize(vec3(-shownSlope.x, 1.0f, -shownSlope.y))
			                               : UwSurfaceNormalOf(shownSlope, toSurface);
			// How deep the water is over what the pixel shows, and over the ground under the surface
			float thickness = farFloor ? 160.0f : waterTop - world.y;
			float cliff = UwShoreCliff(shore);
			// A still pond: smooth water that mirrors the sky, brown and dark from what it holds
			float pond = UwShorePond(shore);
			roughness = mix(roughness, 0.025f, pond);
			// The scene under the surface, bent by the waves: the bend is less where the water is thin, and none
			// where it would show what is above the water
			vec3 below = world;
			vec3 through = farFloor ? floorColour : scene;
			if (u_uwFlags.z > 0.5f)
			{
				float thin = clamp((sceneDistance - toSurface) / 6.0f, 0.0f, 1.0f);
				vec2 bentUv = clamp(uv + normal.xz * 0.05f * thin, vec2_splat(0.001f), vec2_splat(0.999f));
				float bentDepth = texture2DLod(s_sceneDepth, bentUv, 0.0f).r;
				vec3 bentWorld = bentDepth <= 0.0f ? eye + normalize(WorldAt(bentUv, 0.5f) - eye) * 10000.0f
				                                   : WorldAt(bentUv, bentDepth);
				if (bentWorld.y < u_uwParams.z && bentDepth > 0.0f && !sky)
				{
					below = bentWorld;
					through = texture2DLod(s_sceneColour, bentUv, 0.0f).rgb;
				}
			}
			// The water takes the light out on its way down to the scene and back up to the surface, the red first
			// Where the original game shows its sea, the water is never thinner than a layer of the sea's colour, so
			// that the shallows a creature walks in look like water and not like wet ground
			float originalLand = OriginalLandAt(surfacePoint.xz);
			float down = max(u_uwParams.z - below.y, 0.0f);
			float path = min(length(below - surfacePoint), 400.0f);
			vec3 left = UwTransmittance(max(path + down * 0.6f, (1.0f - originalLand) * UW_SHALLOWS_TINT));
			vec3 water = through * left + scatter * (1.0f - left);
			if (pond > 0.0f)
			{
				vec3 pondLeft = exp(-vec3(0.45f, 0.55f, 0.8f) * (path + down * 0.6f + 0.3f));
				vec3 pondColour = vec3(0.07f, 0.08f, 0.04f) * (0.3f + 0.7f * u_uwLightColour.rgb * u_uwLightColour.w);
				water = mix(water, through * pondLeft + pondColour * (1.0f - pondLeft), pond);
			}
			// The silt in the water under the surface, on the way to what the pixel shows
			float siltMean;
			vec3 siltStart = vec3(surfacePoint.x, min(surfacePoint.y, u_uwParams.z) - 0.05f, surfacePoint.z);
			vec4 silt = UwSedimentAlong(siltStart, direction, min(path, 15.0f), t, jitter, 3, siltMean);
			water = water * silt.a + silt.rgb;

			// The reflection: of the scene where the screen has it, else of the sky
			vec3 reflected = reflect(direction, normal);
			reflected.y = abs(reflected.y);
			vec3 mirror = u_uwSea.w > 0.5f ? EnvironmentReflection(surfacePoint, reflected, roughness) : SkyIn(reflected);
			float screenShare = 0.0f;
			if (u_uwFlags.x > 0.5f)
			{
				vec4 found = ScreenReflection(surfacePoint, reflected, jitter);
				mirror = mix(mirror, found.rgb, found.a);
				screenShare = found.a;
			}
			// The scene in the screen already has the flash of the lightning: the sky and the far scene get it here
			mirror += LightningInSky(reflected) * (1.0f - screenShare);
			float facing = clamp(dot(-direction, normal), 0.0f, 1.0f);
			float fresnel = 0.02f + 0.98f * pow(max(1.0f - facing, 0.0f), 5.0f);
			colour = mix(water, mirror, fresnel);
			// The glint of the sun or the moon on the waves
			float glint = u_uwSea.x > 0.5f ? min(SeaGlint(normal, -direction, u_uwAbove.xyz, roughness), 12.0f)
			                               : pow(max(dot(reflected, u_uwAbove.xyz), 0.0f), 300.0f) * 3.0f;
			colour += u_uwLightColour.rgb * glint * u_uwLightColour.w;
			// The glint of the bolt, and the ring where a strike hit the sea
			colour += LightningGlint(surfacePoint, normal, -direction, roughness);
			colour += UW_FLASH_COLOUR * strike.z;

			// The foam where the water is shallow and at the edge of the run up the beach, on the crests that break
			// near the coast, round the things that float, and in a storm on the crests and in streaks along the wind
			if (u_uwFlags.y > 0.5f)
			{
				// The foam of the shallows is less where the original game shows its land, so that the edge of the water
				// does not draw a line that the original coast does not have
				float foam = Foam(surfacePoint.xz, max(min(thickness, waterDepth), 0.0f), t) * (1.0f - originalLand);
				float crest = lift / max(UwWaveTop(), 1e-3f);
				// The shore wave breaks on its face as it comes in, from 20 units out to the edge of the water
				float breaking = face * (1.0f - smoothstep(1.0f, 10.0f, shore.x)) * smoothstep(0.0f, 0.3f, waterDepth) *
				                 (1.0f - cliff);
				foam = max(foam, breaking * smoothstep(0.5f, 0.75f, UwFoamNoise(surfacePoint.xz * 2.6f + t * 0.3f)));
				// At the foot of a cliff: a thick band of boiling white water against the rock, and broad sheets of foam
				// that each crash pushes out and that break up into patches and streaks as the current carries them
				// away. The foam builds up between the waves, so it never clears at a cliff.
				if (cliff > 0.01f)
				{
					vec2 shift;
					vec2 flow = UwCurrent(vec3(surfacePoint.x, u_uwParams.z, surfacePoint.z), t, shift);
					vec2 carried = surfacePoint.xz - shift - u_uwCurrent.xy * t;
					float boil = UwFoamNoise(carried * 1.4f - t * 0.5f);
					float band = (1.0f - smoothstep(1.5f, 5.0f, shore.x)) * smoothstep(0.35f, 0.6f, boil);
					float since = fract((u_uwPhase.x + UwNoise(surfacePoint.xz * 0.008f) * 1.2f) / 6.2831853f);
					float front = 6.0f + since * (40.0f + 40.0f * u_uwWaves.y);
					float push = smoothstep(front - 25.0f, front, shore.x) * (1.0f - smoothstep(front, front + 4.0f, shore.x));
					float spread = 1.0f - smoothstep(5.0f, 25.0f + 15.0f * u_uwWaves.y, shore.x);
					float pieces = smoothstep(0.55f, 0.75f, UwFoamNoise(carried * 0.36f) * 0.7f + UwFoamNoise(carried * 1.2f) * 0.3f);
					float sheets = spread * pieces * (1.0f + push * 0.5f);
					float strength = 0.7f + 0.3f * u_uwWaves.y + 0.15f * min(u_uwWaves.x, 2.0f);
					foam = max(foam, clamp((band + sheets * 0.85f) * cliff * 1.4f, 0.0f, 1.0f) * strength + length(flow) * 0.0f);
				}
				foam = max(foam, ring.z * 0.6f * smoothstep(0.4f, 0.65f, UwFoamNoise(surfacePoint.xz * 3.0f)));
				// The foam of the wakes and the splashes, and the water line on the hulls
				foam = max(foam, max(UwWakeFoam(surfacePoint.xz, t), UwHullLine(surfacePoint.xz, t) * 0.6f));
				foam = max(foam, RogueFoam(surfacePoint.xz, shore, t));
				foam = max(foam, min(strike.z, 1.0f) * 0.5f);
				if (u_uwFlags2.x > 0.5f && u_uwWaves.y > 0.05f)
				{
					vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
					vec2 across = vec2(-wind.y, wind.x);
					// Far away a pixel covers many caps and streaks: it shows how much of the water they cover, as one shade
					vec4 seen = FootprintOnWater(direction, toSurface);
					float capsShown = u_uwSea.x > 0.5f ? 1.0f - smoothstep(1.0f, 3.0f, max(seen.x * seen.w, seen.x)) : 1.0f;
					float streaksShown = u_uwSea.x > 0.5f ? 1.0f - smoothstep(0.7f, 2.0f, UwFootprintAlong(seen, across)) : 1.0f;
					vec2 q = surfacePoint.xz;
					float caps = smoothstep(0.72f, 0.98f, crest) * smoothstep(0.6f, 0.85f, UwNoise(q * 0.7f + t * 0.4f));
					// Two layers, the second turned against the first, so the noise's square cells do not show
					vec2 streakUv = vec2(dot(q, wind) * 0.08f - t * 0.2f, dot(q, across) * 1.0f);
					vec2 turned = vec2(streakUv.x * 0.8f - streakUv.y * 0.6f, streakUv.x * 0.6f + streakUv.y * 0.8f);
					float streaks = smoothstep(0.75f, 0.92f, UwNoise(streakUv) * 0.6f + UwNoise(turned * 1.7f + 3.1f) * 0.4f);
					caps = mix(0.03f * smoothstep(0.2f, 1.0f, u_uwWaves.y), caps, capsShown);
					streaks = mix(0.04f, streaks, streaksShown);
					foam = max(foam, (caps + streaks * 0.5f) * u_uwWaves.y);
				}
				foam *= 1.0f - pond;
				vec3 foamColour = u_uwLightColour.rgb * (0.4f + 0.6f * max(u_uwAbove.y, 0.0f)) + u_uwSky.rgb * 0.7f +
				                  scatter * 0.5f;
				// The flash of the lightning lights the foam
				foamColour += UW_FLASH_COLOUR * (u_uwStrike[1].x * 0.8f + u_uwStrike[0].w * 0.3f) * u_uwStrike[1].w;
				if (u_uwFoam.z > 0.5f)
				{
					vec2 foamShift;
					UwCurrent(vec3(surfacePoint.x, u_uwParams.z, surfacePoint.z), t, foamShift);
					colour = VolumeFoam(colour, min(foamColour, vec3_splat(1.0f)), surfacePoint, direction, foam, toSurface * u_uwSea.z,
					                    foamShift + u_uwCurrent.xy * t, u_uwSky.rgb, scatter, jitter);
				}
				else
				{
					colour = mix(colour, min(foamColour, vec3_splat(1.0f)), foam * 0.85f);
				}
			}
			// Where the original game shows its land over its sea, the land shows here too, as much as there
			colour = u_uwFlags2.w > 0.5f ? UwWakeDebug(surfacePoint.xz) : colour;
			colour = mix(colour, scene, max(originalLand, insideBoat));
			if (max(originalLand, insideBoat) < 0.5f)
			{
				vec4 projected = mul(u_viewProj, vec4(surfacePoint, 1.0f));
				outDepth = max(outDepth, projected.z / projected.w);
			}
		}
		else if (!sky && world.y < UW_FILM_TOP && (world.y > -0.05f || world.y < u_uwParams.z + 0.05f) &&
		         abs(texture2DLod(s_sceneColour, sampleUv, 0.0f).a - UW_LAND_MARK) < 0.5f / 255.0f)
		{
			// The land over the water, where the original game shows its sea through the land's coast alpha. The
			// original sea shows there as a thin layer of water on the land, so that the new water shows wherever the
			// original sea shows. Things on the land do not get this layer. The sea bed that a low tide shows (under
			// the usual sea level and over the tide) does not get it either, so that the wet sand shows. The sea bed
			// that the trough of a wave shows gets it.
			float originalLand = OriginalLandAt(world.xz);
			if (originalLand < 1.0f)
			{
				vec3 left = UwTransmittance((1.0f - originalLand) * UW_SHALLOWS_TINT);
				vec3 water = scene * left + scatter * (1.0f - left);
				float facing = clamp(-direction.y, 0.0f, 1.0f);
				float fresnel = 0.02f + 0.98f * pow(max(1.0f - facing, 0.0f), 5.0f);
				water = mix(water, SkyIn(reflect(direction, vec3(0.0f, 1.0f, 0.0f))), fresnel);
				water = u_uwFlags2.w > 0.5f ? vec3(1.0f, 0.0f, 1.0f) : water;
				colour = mix(water, scene, originalLand);
			}
		}
	}
	// The surface that cuts the view, as the camera goes through it: a thin dark line with water running down it
	float meniscus = 1.0f - smoothstep(0.0f, lineWidth * 3.0f, abs(underBy));
	colour = mix(colour, scatter * 0.35f, meniscus * 0.8f);
	gl_FragColor = vec4(colour, 1.0f);
	// What blends over the water is drawn after this pass, tested against this depth
	gl_FragDepth = outDepth;
}
