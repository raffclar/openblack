$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "fog.sh"
#include "sky_dome.sh"
#include "world_curve.sh"

// EXPERIMENT (not for merge): the fog over the scene and the water, before the clouds. Each pixel reads the summed fog
// at the distance of its scene, between the two slices round it. Where the screen shows the sky, a thinner share of the
// fog shows, so that the sky stays clear. Blended as rgb + the scene times a.

SAMPLER2D(s_fogSum, 0);
SAMPLER2D(s_sceneDepth, 1);

// x: how much of the fog shows over the sky; yzw: unused
uniform vec4 u_fogApply;
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
	sky = sky || OnSkyDome(world);
	float range = sky ? u_fogGrid.w : length(world - eye);
	// Above the water, the water's surface on the curved world is where the scene is. The sea is a part of the scene at
	// its full distance, also past the grid's far end. Thus its fog eases to the sky's share with the distance and does
	// not change in a step where the sea passes the grid's far end. A ray over the horizon does not meet the sea.
	float sea = u_fogAmbient.w;
	if (eye.y > sea && dir.y < 0.0f)
	{
		float toSea = CurvedSeaDistance(eye.y - sea, dir);
		if (toSea >= 0.0f && (sky || toSea < range))
		{
			range = toSea;
			sky = false;
		}
	}
	float slice = clamp(FogSliceAt(range), 0.0f, u_fogGrid.z - 1.0f);
	float first = floor(slice);
	float second = min(first + 1.0f, u_fogGrid.z - 1.0f);
	vec4 fog = mix(texture2DLod(s_fogSum, FogTileUv(uv, first), 0.0f), texture2DLod(s_fogSum, FogTileUv(uv, second), 0.0f),
	               slice - first);
	// Nearer than the first slice, the fog is a share of the first slice's
	float nearShare = clamp(range / FogSliceDistance(0.0f), 0.0f, 1.0f);
	fog = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), fog, nearShare);
	// A ray along the horizon goes through all of the fog's layer, so the sky at the horizon has all of the fog, as the
	// far sea under it has. Higher up the sky has a thinner share, so that it stays clear. Past the grid's far end a
	// scene over the horizon eases to the share of the sky behind it. Thus the fog grows with the distance and the far
	// sea and the sky have the same fog at the horizon, with no line between them.
	float skyShare = mix(1.0f, u_fogApply.x, smoothstep(0.0f, 0.06f, dir.y));
	float share = sky ? skyShare : mix(1.0f, skyShare, smoothstep(u_fogGrid.w, u_fogGrid.w * 4.0f, range));
	fog = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), fog, share);
	gl_FragColor = fog;
}
