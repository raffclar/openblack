$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "atmosphere.sh"

// EXPERIMENT (not for merge): the haze over the scene and the water. Each pixel reads the haze's grid at the distance of
// its scene, between the two slices round it. The sky has the air's light in it already and gets no haze. The low tier
// has no grid: its haze is the horizon's colour over the distance, from the air at the eye. Blended as rgb + the scene
// times a.

SAMPLER2D(s_atmosAerial, 0);
SAMPLER2D(s_sceneDepth, 1);

// x: 1 for the grid, 0 for the low tier's haze; y: the low tier's haze for each world unit; zw: unused
uniform vec4 u_hazeApply;
// rgb: the colour of the horizon on the screen, for the low tier; w: unused
uniform vec4 u_hazeHorizon;
// The sky's dome: xy its middle across the land; z how far across and w how high a point must be to be on it
uniform vec4 u_hazeDome;

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

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	float depth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	bool sky = depth <= 0.0f;
	vec3 dir = normalize(WorldAt(uv, 0.5f) - eye);
	vec3 world = WorldAt(uv, depth);
	// The sky's dome writes its depth: a point on it is the sky
	sky = sky || length(world.xz - u_hazeDome.xy) > u_hazeDome.z || world.y > u_hazeDome.w;
	float range = sky ? 1e9f : length(world - eye);
	// Above the water, the water's surface is where the scene is
	float sea = u_atmosParams.w;
	if (eye.y > sea && dir.y < 0.0f)
	{
		float toSea = (eye.y - sea) / -dir.y;
		if (toSea < range)
		{
			range = toSea;
			sky = false;
		}
	}
	if (sky)
	{
		gl_FragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}
	vec4 haze;
	if (u_hazeApply.x > 0.5f)
	{
		float slice = clamp(AtmosSliceAt(range), 0.0f, ATMOS_AERIAL_SLICES - 1.0f);
		float first = floor(slice);
		float second = min(first + 1.0f, ATMOS_AERIAL_SLICES - 1.0f);
		haze = mix(texture2DLod(s_atmosAerial, AtmosAerialUv(uv, first), 0.0f),
		           texture2DLod(s_atmosAerial, AtmosAerialUv(uv, second), 0.0f), slice - first);
		// Nearer than the first slice, the haze is a share of the first slice's
		float nearShare = clamp(range / AtmosSliceDistance(0.0f), 0.0f, 1.0f);
		haze = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), haze, nearShare);
		// Past the last slice the haze goes on to the horizon's colour
		float farther = max(range - ATMOS_AERIAL_FARTHEST, 0.0f) * u_hazeApply.y;
		haze.rgb *= u_atmosSun.rgb;
		float beyond = exp(-farther);
		haze = vec4(haze.rgb + haze.a * u_hazeHorizon.rgb * (1.0f - beyond), haze.a * beyond);
	}
	else
	{
		float through = exp(-range * u_hazeApply.y);
		haze = vec4(u_hazeHorizon.rgb * (1.0f - through), through);
	}
	gl_FragColor = haze;
}
