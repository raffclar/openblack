$input v_texcoord0, v_texcoord1, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): one puff of a waterspout: a soft wisp from the noise volume, faded near what is behind it

SAMPLER2D(s_sceneDepth, 1);
SAMPLER3D(s_sprayNoise, 3);

// Where a point of the screen is in the world, at a depth of the depth buffer
vec3 SpoutWorldAt(vec2 uv, float depth)
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
	vec2 corner = v_texcoord0.xy;
	float r = dot(corner, corner);
	if (r > 1.0f)
	{
		discard;
	}
	float n = texture3DLod(s_sprayNoise, v_texcoord1.xyz + vec3(corner * 0.35f, 0.0f), 0.0f).r;
	float falloff = max(1.0f - r, 0.0f);
	float cover = falloff * falloff * smoothstep(0.2f, 0.85f, n);
	float soft = 1.0f;
	if (sceneDepth > 0.0f)
	{
		soft = clamp(length(SpoutWorldAt(uv, sceneDepth) - v_world) / max(v_texcoord0.z, 1.0f), 0.0f, 1.0f);
	}
	cover = clamp(cover * soft * v_texcoord0.w, 0.0f, 1.0f);
	if (cover < 0.002f)
	{
		discard;
	}
	gl_FragColor = vec4(v_color0.rgb * (0.8f + 0.3f * n) * cover, cover);
}
