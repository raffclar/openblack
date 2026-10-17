$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "fog.sh"
#include "clouds_shadow.sh"
#include "sun_visibility.sh"

// EXPERIMENT (not for merge): the fog of each cell of the grid and the light that it scatters towards the eye. The
// light of the sun or the moon reaches the cell through the hills' shadow, the clouds' shadow and the things' shadows;
// the sky lights the fog too. Each frame moves the sample a little along the cell's depth with the blue noise, and
// blends the result with the last frame's cell at the same place in the world.
// rgb: the light scattered for each unit; a: how much light the fog takes out for each unit.

SAMPLER2D(s_fogPrevious, 0);
SAMPLER2D(s_shadowNew0, 1);
SAMPLER2D(s_shadowNew1, 2);
SAMPLER2D(s_shadowMoving, 3);
SAMPLER3D(s_fogNoise, 4);
SAMPLER2D(s_fogBlueNoise, 5);
// The shore map: w is the height of the ground at each corner of the land
SAMPLER2D(s_shore, 6);
// The sky alone round the eye (the cube that the water reflects), for the light of the sky in the fog
SAMPLERCUBE(s_fogSky, 7);

// The view-projections of the light's maps: the newest set's two cascades and the moving things' map
uniform mat4 u_fogShadow0;
uniform mat4 u_fogShadow1;
uniform mat4 u_fogShadowMoving;
// The view-projection of the last frame
uniform mat4 u_fogPreviousViewProj;
// xyz: the eye of the last frame; w: 1 while the last frame's grid holds fog
uniform vec4 u_fogPrevious;
// x: 1 while the maps of the environment are made; y: 1 while the moving things' map is made; z: how much of the
// last frame's grid to keep; w: 1 while the cube of the sky is ready
uniform vec4 u_fogShadows;

// The light of the sky: the mean of the cube of the sky round the horizon and above, never darker than the haze
vec3 SkyLight()
{
	if (u_fogShadows.w < 0.5f)
	{
		return u_fogAmbient.rgb;
	}
	vec3 sum = textureCubeLod(s_fogSky, vec3(0.0f, 1.0f, 0.0f), 0.0f).rgb;
	sum += textureCubeLod(s_fogSky, vec3(0.9f, 0.3f, 0.0f), 0.0f).rgb;
	sum += textureCubeLod(s_fogSky, vec3(-0.9f, 0.3f, 0.0f), 0.0f).rgb;
	sum += textureCubeLod(s_fogSky, vec3(0.0f, 0.3f, 0.9f), 0.0f).rgb;
	sum += textureCubeLod(s_fogSky, vec3(0.0f, 0.3f, -0.9f), 0.0f).rgb;
	return max(sum / 5.0f * 0.8f, u_fogAmbient.rgb);
}

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

vec3 ProjectTo(mat4 viewProj, vec3 world)
{
	vec4 clip = mul(viewProj, vec4(world, 1.0f));
	vec3 ndc = clip.xyz / clip.w;
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec3(ndc.xy * 0.5f + 0.5f, ndc.z * 0.5f + 0.5f);
#else
	return vec3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z);
#endif
}

// How much light one of the light's maps lets reach a point, from one sample; -1 outside the map
#define FOG_SHADOW_IN(fnName, samplerName)                                                                             \
	float fnName(mat4 viewProj, vec3 world)                                                                            \
	{                                                                                                                  \
		vec3 at = ProjectTo(viewProj, world);                                                                          \
		if (at.x < 0.002f || at.y < 0.002f || at.x > 0.998f || at.y > 0.998f || at.z <= 0.0f || at.z >= 1.0f)         \
		{                                                                                                              \
			return -1.0f;                                                                                              \
		}                                                                                                              \
		return texture2DLod(samplerName, at.xy, 0.0f).r <= at.z + 0.0003f ? 1.0f : 0.0f;                               \
	}

FOG_SHADOW_IN(FogShadow0, s_shadowNew0)
FOG_SHADOW_IN(FogShadow1, s_shadowNew1)
FOG_SHADOW_IN(FogShadowMoving, s_shadowMoving)

float HenyeyGreenstein(float cosAngle, float g)
{
	float g2 = g * g;
	float d = 1.0f + g2 - 2.0f * g * cosAngle;
	return (1.0f - g2) / (4.0f * 3.14159265f * d * sqrt(d));
}

void main()
{
	vec2 cells = u_fogGrid.xy;
	vec2 texel = floor(gl_FragCoord.xy);
	vec2 tile = floor(texel / cells);
	vec2 cell = texel - tile * cells;
	float slice = tile.y * FOG_TILES_ACROSS + tile.x;
	vec2 uv = (cell + 0.5f) / cells;

	// The sample moves along the cell's depth with the blue noise, other each frame
	float jitter = fract(texture2DLod(s_fogBlueNoise, (cell + 0.5f) / 64.0f, 0.0f).r + u_fogDrift.w * 0.61803398875f);
	float range = FogSliceDistance(slice + jitter - 0.5f);
	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	vec3 dir = normalize(WorldAt(uv, 0.5f) - eye);
	vec3 p = eye + dir * range;

	// The fog's layers
	float sea = u_fogAmbient.w;
	float y = p.y - sea;
	float ground = texture2DLod(s_shore, (p.xz / 10.0f + 0.5f) / 513.0f, 0.0f).w;
	float overGround = p.y - max(ground, sea);
	vec3 drifting = p - vec3(u_fogDrift.x, u_fogDrift.z * 0.4f, u_fogDrift.y);
	float mist = texture3DLod(s_fogNoise, drifting / 190.0f, 0.0f).r * 0.6f + texture3DLod(s_fogNoise, drifting / 61.0f, 0.0f).g * 0.4f;
	float haze = u_fogLayers.x * exp(-max(y, 0.0f) / u_fogLayers.y);
	// The ground fog lies in the valleys and over the water, under its top, in banks that the wind moves
	float top = u_fogLayers.w + (mist - 0.5f) * 10.0f;
	float groundFog = u_fogLayers.z * (1.0f - smoothstep(top - 8.0f, top + 6.0f, y)) * smoothstep(0.2f, 0.75f, mist + 0.15f);
	// A thin mist close over the ground all day, which the shafts of light show in the trees
	float lowMist = (0.0014f + u_fogLayers.z * 0.12f) * exp(-max(overGround, 0.0f) / 14.0f) * (0.5f + mist);
	float sigma = (haze + groundFog + lowMist) * step(-0.5f, overGround) * smoothstep(-0.5f, 0.5f, y);

	// The light of the sun or the moon through the shadows
	float visibility = SunVisibilityAbove(p) * CloudShadeAt(p.xz);
	if (u_fogShadows.x > 0.5f)
	{
		float nearMap = FogShadow0(u_fogShadow0, p);
		float lit = nearMap >= 0.0f ? nearMap : FogShadow1(u_fogShadow1, p);
		visibility *= lit >= 0.0f ? lit : 1.0f;
	}
	if (u_fogShadows.y > 0.5f)
	{
		float moving = FogShadowMoving(u_fogShadowMoving, p);
		visibility *= moving >= 0.0f ? moving : 1.0f;
	}
	float cosAngle = dot(dir, u_fogLight.xyz);
	// Most of the light goes on forward, so that the shafts show against the sun
	float phase = mix(HenyeyGreenstein(cosAngle, 0.55f), HenyeyGreenstein(cosAngle, -0.2f), 0.3f) * 4.0f * 3.14159265f * 0.7f;
	vec3 light = u_fogLightColour.rgb * u_fogLightColour.w * visibility * phase + SkyLight();
	vec4 result = vec4(light * sigma, sigma);

	// The last frame's cell at the same place in the world
	if (u_fogPrevious.w > 0.5f)
	{
		vec3 before = ProjectTo(u_fogPreviousViewProj, p);
		float beforeSlice = floor(FogSliceAt(length(p - u_fogPrevious.xyz)) + 0.5f);
		if (before.x > 0.0f && before.y > 0.0f && before.x < 1.0f && before.y < 1.0f && beforeSlice >= 0.0f &&
		    beforeSlice < u_fogGrid.z)
		{
			vec4 previous = texture2DLod(s_fogPrevious, FogTileUv(before.xy, beforeSlice), 0.0f);
			result = mix(result, previous, u_fogShadows.z);
		}
	}
	gl_FragColor = result;
}
