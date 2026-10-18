$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "lighting.sh"
#include "sun_visibility.sh"
#include "sky_dome.sh"

// EXPERIMENT (not for merge): the shadows of the trees, the buildings and the other things, from the light's maps,
// over the scene's colour. Each pixel finds its place in the world from the scene's depth, then its place in each map:
// the environment's maps blend from the old set to the new, and the moving things' map darkens on top. The result
// multiplies the scene's colour.

SAMPLER2D(s_sceneDepth, 0);
SAMPLER2D(s_shadowOld0, 1);
SAMPLER2D(s_shadowOld1, 2);
SAMPLER2D(s_shadowNew0, 3);
SAMPLER2D(s_shadowNew1, 4);
SAMPLER2D(s_shadowMoving, 5);
// EXPERIMENT (not for merge): the shade from the scene's depth at half the screen's size: r how much of the sky a pixel
// sees, g its distance from the eye
SAMPLER2D(s_occlusion, 6);

// The view-projections of the maps: the old and the new set's two cascades, and the moving things' map
uniform mat4 u_shadowOld0;
uniform mat4 u_shadowOld1;
uniform mat4 u_shadowNew0;
uniform mat4 u_shadowNew1;
uniform mat4 u_shadowMoving;
// x: how much of the new set shows; y: how dark a full shadow makes the scene, 0 to 1; z: 1 while the old set is
// made; w: 1 while the moving things' map is made
uniform vec4 u_shadowParams;
// x: 1 while the shade from the scene's depth shows; yz: the size of its texture; w: unused
uniform vec4 u_shadowOcclusion;
// Where a point of the screen is in the world, at a depth of the depth buffer
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

// Where a point of the world is in a map: xy as the texture is read, z its depth as the map keeps it
vec3 MapAt(mat4 viewProj, vec3 world)
{
	vec4 clip = mul(viewProj, vec4(world, 1.0f));
	vec3 ndc = clip.xyz / clip.w;
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec3(ndc.xy * 0.5f + 0.5f, ndc.z * 0.5f + 0.5f);
#else
	return vec3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z);
#endif
}

// How much of the light a map lets reach a point, from nine samples round it; -1 when the point is outside the map.
// The map keeps the depth of the nearest thing to the light, larger nearer to it.
#define SHADOW_IN(fnName, samplerName)                                                                                 \
	float fnName(mat4 viewProj, vec3 world)                                                                            \
	{                                                                                                                  \
		vec3 at = MapAt(viewProj, world);                                                                              \
		if (at.x < 0.002f || at.y < 0.002f || at.x > 0.998f || at.y > 0.998f || at.z <= 0.0f || at.z >= 1.0f)         \
		{                                                                                                              \
			return -1.0f;                                                                                              \
		}                                                                                                              \
		float lit = 0.0f;                                                                                              \
		float texel = 1.0f / 2048.0f;                                                                                  \
		for (int y = -1; y <= 1; ++y)                                                                                  \
		{                                                                                                              \
			for (int x = -1; x <= 1; ++x)                                                                              \
			{                                                                                                          \
				float stored = texture2DLod(samplerName, at.xy + vec2(float(x), float(y)) * texel, 0.0f).r;            \
				lit += stored <= at.z + 0.0003f ? 1.0f : 0.0f;                                                         \
			}                                                                                                          \
		}                                                                                                              \
		return lit / 9.0f;                                                                                             \
	}

SHADOW_IN(ShadowOld0, s_shadowOld0)
SHADOW_IN(ShadowOld1, s_shadowOld1)
SHADOW_IN(ShadowNew0, s_shadowNew0)
SHADOW_IN(ShadowNew1, s_shadowNew1)
SHADOW_IN(ShadowMoving, s_shadowMoving)

// The surface's direction at a pixel, from the nearer of the two neighbours along each axis of the screen
vec3 SurfaceNormal(vec2 uv, vec3 world)
{
	vec2 pixel = u_viewTexel.xy;
	vec3 right = WorldAt(uv + vec2(pixel.x, 0.0f), texture2DLod(s_sceneDepth, uv + vec2(pixel.x, 0.0f), 0.0f).r) - world;
	vec3 left = world - WorldAt(uv - vec2(pixel.x, 0.0f), texture2DLod(s_sceneDepth, uv - vec2(pixel.x, 0.0f), 0.0f).r);
	vec3 down = WorldAt(uv + vec2(0.0f, pixel.y), texture2DLod(s_sceneDepth, uv + vec2(0.0f, pixel.y), 0.0f).r) - world;
	vec3 up = world - WorldAt(uv - vec2(0.0f, pixel.y), texture2DLod(s_sceneDepth, uv - vec2(0.0f, pixel.y), 0.0f).r);
	vec3 across = dot(right, right) < dot(left, left) ? right : left;
	vec3 along = dot(down, down) < dot(up, up) ? down : up;
	vec3 n = normalize(cross(along, across));
	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	return dot(n, eye - world) < 0.0f ? -n : n;
}

// How much of the sky a pixel sees from the shade at half size: of the four texels round it, those whose distance is
// near this pixel's count the most, so that no shade spills over an edge
float Occlusion(vec2 uv, float range)
{
	vec2 size = u_shadowOcclusion.yz;
	vec2 texel = uv * size - 0.5f;
	vec2 base = floor(texel);
	vec2 f = texel - base;
	float sum = 0.0f;
	float weights = 0.0f;
	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 2; ++x)
		{
			vec2 offset = vec2(float(x), float(y));
			vec4 found = texture2DLod(s_occlusion, (base + offset + 0.5f) / size, 0.0f);
			float place = (x == 0 ? 1.0f - f.x : f.x) * (y == 0 ? 1.0f - f.y : f.y);
			float weight = place / (0.02f + abs(found.g - range) / max(range, 1.0f)) + 1e-5f;
			sum += found.r * weight;
			weights += weight;
		}
	}
	return sum / weights;
}

// The near cascade where the point is in it, else the far one, else full light
float Pick(float nearCascade, float farCascade)
{
	return nearCascade >= 0.0f ? nearCascade : (farCascade >= 0.0f ? farCascade : 1.0f);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	float depth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	if (depth <= 0.0f)
	{
		// The sky has no shadow
		gl_FragColor = vec4_splat(1.0f);
		return;
	}
	vec3 world = WorldAt(uv, depth);
	if (OnSkyDome(world))
	{
		gl_FragColor = vec4_splat(1.0f);
		return;
	}
	float light = Pick(ShadowNew0(u_shadowNew0, world), ShadowNew1(u_shadowNew1, world));
	if (u_shadowParams.z > 0.5f && u_shadowParams.x < 1.0f)
	{
		float old = Pick(ShadowOld0(u_shadowOld0, world), ShadowOld1(u_shadowOld1, world));
		light = mix(old, light, u_shadowParams.x);
	}
	if (u_shadowParams.w > 0.5f)
	{
		float moving = ShadowMoving(u_shadowMoving, world);
		light = min(light, moving >= 0.0f ? moving : 1.0f);
	}
	// From high up the shadows of the land are smaller than a pixel, and the maps do not cover the far view. The
	// shadows and the shade of the land go away as the eye climbs from 12000 to 25000 units.
	vec3 eyeAt = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	float high = smoothstep(12000.0f, 25000.0f, eyeAt.y);
	light = mix(light, 1.0f, high);
	// The light of the sky and of the land in the shade (EXPERIMENT, not for merge): the sky's light where the hills,
	// the things round the point and its folds do not hide it, and the light that the lit land sends back. Without it
	// the shade is a fixed part of the light.
	vec3 ambient = vec3_splat(1.0f);
	if (u_lightParams.y > 0.5f)
	{
		vec3 n = SurfaceNormal(uv, world);
		float open = SkyOpennessAt(world.xz);
		if (u_shadowOcclusion.x > 0.5f)
		{
			vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
			open *= Occlusion(uv, length(world - eye));
		}
		open = mix(open, 1.0f, high);
		ambient = LightShadeTint(LightSkyAt(n) * open + LightBounceAt(world, n));
	}
	// The direct light's share of the scene's light is u_shadowParams.y; the rest is the shade's
	vec3 shade = ambient * (1.0f - u_shadowParams.y) + vec3_splat(u_shadowParams.y * light);
	gl_FragColor = vec4(shade, 1.0f);
}
