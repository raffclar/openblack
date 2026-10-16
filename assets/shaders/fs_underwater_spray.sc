$input v_texcoord0, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): one particle of the spray at a cliff. The sheet and the core are lumps with broken
// edges, the drops are hard, and the mist is soft. Each particle fades where it comes near the rock or the water
// behind it, so it has no hard edge there.

SAMPLER2D(s_sceneDepth, 1);

// Where a point of the screen is in the world, at a depth of the depth buffer
vec3 SprayWorldAt(vec2 uv, float depth)
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
	float sceneDepth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	// The depth is reversed: nearer is larger
	if (sceneDepth > gl_FragCoord.z)
	{
		discard;
	}
	float layer = v_texcoord0.z;
	vec2 corner = v_texcoord0.xy;
	float r = dot(corner, corner);
	float shape;
	if (layer < 1.5f)
	{
		// A lump with a broken edge: the noise eats into it
		float lumps = UwNoise(corner * 3.0f + v_world.xz * 0.7f + v_world.y * 0.5f);
		shape = smoothstep(0.0f, 0.35f, (1.0f - r) - (lumps - 0.5f) * 0.6f);
	}
	else if (layer < 2.5f)
	{
		shape = 1.0f - smoothstep(0.4f, 1.0f, r);
	}
	else
	{
		shape = max(1.0f - r, 0.0f);
		shape *= shape;
	}
	// Soft: faded near what is behind it
	float soft = 1.0f;
	if (sceneDepth > 0.0f)
	{
		soft = clamp(length(SprayWorldAt(uv, sceneDepth) - v_world) / 1.2f, 0.0f, 1.0f);
	}
	float cover = shape * v_texcoord0.w * soft;
	gl_FragColor = vec4(v_color0.rgb * cover, cover);
}
