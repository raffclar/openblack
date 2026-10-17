$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "atmosphere.sh"
SAMPLER2D(s_atmosAerial, 0);
#include "aerial_haze.sh"
#include "sky_dome.sh"
#include "world_curve.sh"

// EXPERIMENT (not for merge): the haze over the scene and the water. Each pixel reads the haze's grid at the distance of
// its scene, between the two slices round it. The sky has the air's light in it already and gets no haze. The low tier
// has no grid: its haze is the horizon's colour over the distance, from the air at the eye. Blended as rgb + the scene
// times a.

SAMPLER2D(s_sceneDepth, 1);

// x: 1 for the grid, 0 for the low tier's haze; y: the low tier's haze for each world unit; z: the sine of the air's
// sun over the horizon; w: unused
uniform vec4 u_hazeApply;
// rgb: the colour of the horizon on the screen, for the low tier; w: unused
uniform vec4 u_hazeHorizon;
// The game's own sun or moon: xyz the direction towards it, w how strongly its glare shows; and its light's colour
uniform vec4 u_hazeSun;
uniform vec4 u_hazeSunColour;

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

// How much of the sun shows past the scene: five samples of the depth round its place on the screen, each 1 where the
// sky is there. Off the screen the sun is taken as open.
float SunOpen(vec3 eye)
{
	vec4 clip = mul(u_viewProj, vec4(eye + u_hazeSun.xyz * 10000.0f, 1.0f));
	if (clip.w <= 0.0f)
	{
		return 0.0f;
	}
	vec2 ndc = clip.xy / clip.w;
#if BGFX_SHADER_LANGUAGE_GLSL
	vec2 at = ndc * 0.5f + 0.5f;
#else
	vec2 at = vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
#endif
	if (at.x < 0.0f || at.y < 0.0f || at.x > 1.0f || at.y > 1.0f)
	{
		return 1.0f;
	}
	float open = 0.0f;
	for (int i = 0; i < 5; ++i)
	{
		float angle = float(i) * 1.2566371f;
		vec2 offset = i == 0 ? vec2_splat(0.0f) : vec2(cos(angle), sin(angle)) * 0.012f;
		vec2 uv = clamp(at + offset * vec2(u_viewTexel.x / u_viewTexel.y, 1.0f), vec2_splat(0.0f), vec2_splat(1.0f));
		float depth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
		vec3 p = WorldAt(uv, depth);
		bool sky = depth <= 0.0f || OnSkyDome(p);
		open += sky ? 0.2f : 0.0f;
	}
	return open;
}

// The glare round the sun: a wide soft bloom and a tighter bright core, as the eye and the lens see a bright sun
vec3 SunGlare(vec3 dir, float open)
{
	float facing = max(dot(dir, normalize(u_hazeSun.xyz)), 0.0f);
	float glare = 0.18f * pow(facing, 12.0f) + 0.35f * pow(facing, 160.0f) + 0.5f * pow(facing, 2000.0f);
	return u_hazeSunColour.rgb * glare * u_hazeSun.w * open;
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	float depth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	bool sky = depth <= 0.0f;
	vec3 dir = normalize(WorldAt(uv, 0.5f) - eye);
	vec3 world = WorldAt(uv, depth);
	// The sky's dome writes its depth: a point on it is the sky
	sky = sky || OnSkyDome(world);
	float range = sky ? 1e9f : length(world - eye);
	// Above the water, the water's surface is where the scene is. The far sea curves away with the world, and a ray over
	// the horizon does not meet it.
	float sea = u_atmosParams.w;
	if (eye.y > sea && dir.y < 0.0f)
	{
		float toSea = CurvedSeaDistance(eye.y - sea, dir);
		if (toSea >= 0.0f && toSea < range)
		{
			range = toSea;
			sky = false;
		}
	}
	vec3 glare = u_hazeSun.w > 0.0f ? SunGlare(dir, SunOpen(eye)) : vec3_splat(0.0f);
	if (sky)
	{
		gl_FragColor = vec4(glare, 1.0f);
		return;
	}
	vec4 haze;
	if (u_hazeApply.x > 0.5f)
	{
		haze = AtmosHazeAlong(uv, eye, dir, range, u_hazeApply.z);
	}
	else
	{
		float through = exp(-range * u_hazeApply.y);
		haze = vec4(u_hazeHorizon.rgb * (1.0f - through), through);
	}
	gl_FragColor = vec4(haze.rgb + glare, haze.a);
}
