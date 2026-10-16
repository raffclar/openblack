$input v_texcoord0, v_texcoord1, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): one bubble or one part of the cloud of air under the water at a cliff. A bubble is clear
// in the middle with a bright rim and a small highlight; the cloud is soft white wisps from the noise volume. Both fade
// near what is behind them and into the water's fog with distance.

SAMPLER2D(s_sceneDepth, 1);
SAMPLER3D(s_sprayNoise, 3);

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
	float cover;
	vec3 colour;
	if (v_texcoord0.z < 0.5f)
	{
		// The rim, where the light skims the bubble's surface, and the highlight of the light above
		float rim = smoothstep(0.55f, 0.95f, r) * (1.0f - smoothstep(0.95f, 1.0f, r));
		float spot = 1.0f - smoothstep(0.0f, 0.06f, dot(corner - vec2(-0.35f, 0.4f), corner - vec2(-0.35f, 0.4f)));
		cover = 0.08f + 0.75f * rim + 0.9f * spot;
		colour = v_color0.rgb * (0.9f + 0.6f * spot);
	}
	else
	{
		float n = texture3DLod(s_sprayNoise, v_texcoord1.xyz + vec3(corner * 0.3f, 0.0f), 0.0f).r;
		float falloff = max(1.0f - r, 0.0f);
		cover = falloff * falloff * smoothstep(0.25f, 0.8f, n);
		colour = v_color0.rgb * 1.1f;
	}
	// In the water's fog: the colour goes to the water's own with distance, and the bubble fades
	if (v_texcoord1.w > 0.5f)
	{
		vec3 through = UwTransmittance(v_color0.w);
		colour = mix(u_uwScatter.rgb, colour, through);
		cover *= dot(through, vec3_splat(0.3333f));
	}
	cover = clamp(cover * v_texcoord0.w, 0.0f, 1.0f);
	if (cover < 0.002f)
	{
		discard;
	}
	gl_FragColor = vec4(colour * cover, cover);
}
