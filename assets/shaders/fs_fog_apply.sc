$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "fog.sh"

// EXPERIMENT (not for merge): the fog over the scene, the water and the clouds. Each pixel reads the summed fog at the
// distance of its scene, between the two slices round it. Where the screen shows the sky, a thinner share of the fog
// shows, so that the sky and the clouds stay clear. Blended as rgb + the scene times a.

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
	float range = sky ? u_fogGrid.w : length(WorldAt(uv, depth) - eye);
	// Above the water, the water's surface is where the scene is
	float sea = u_fogAmbient.w;
	if (eye.y > sea && dir.y < 0.0f)
	{
		range = min(range, (eye.y - sea) / -dir.y);
		sky = sky && range >= u_fogGrid.w;
	}
	float slice = clamp(FogSliceAt(range), 0.0f, u_fogGrid.z - 1.0f);
	float first = floor(slice);
	float second = min(first + 1.0f, u_fogGrid.z - 1.0f);
	vec4 fog = mix(texture2DLod(s_fogSum, FogTileUv(uv, first), 0.0f), texture2DLod(s_fogSum, FogTileUv(uv, second), 0.0f),
	               slice - first);
	// Nearer than the first slice, the fog is a share of the first slice's
	float nearShare = clamp(range / FogSliceDistance(0.0f), 0.0f, 1.0f);
	fog = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), fog, nearShare);
	if (sky)
	{
		fog = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), fog, u_fogApply.x);
	}
	gl_FragColor = fog;
}
