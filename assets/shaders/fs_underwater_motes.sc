$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): a mote in the water, a soft round dot of light added over the scene. It is hidden where
// the scene is nearer than it.

SAMPLER2D(s_sceneDepth, 1);

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	// The depth is reversed: nearer is larger
	if (texture2D(s_sceneDepth, uv).r > gl_FragCoord.z)
	{
		discard;
	}
	float r = dot(v_texcoord0.xy, v_texcoord0.xy);
	float glow = max(1.0f - r, 0.0f);
	glow = glow * glow;
	// Motes add their light; the debris is blended over what is behind it, premultiplied
	float cover = v_color0.a > 0.5f ? smoothstep(0.0f, 0.3f, glow) * v_texcoord0.w : 0.0f;
	vec3 shade = v_color0.a > 0.5f ? v_color0.rgb * cover : v_color0.rgb * glow * v_texcoord0.w;
	gl_FragColor = vec4(shade, cover);
}
