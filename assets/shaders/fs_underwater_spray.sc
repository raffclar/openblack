$input v_texcoord0, v_texcoord1, v_color0, v_normal, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): one particle of the spray at a cliff. Its shape comes from a volume of noise that it
// moves through as it lives, as if it were a part of a cloud of water: the sheet breaks into strands from the top
// down, the core is in lumps that come apart, the drops are hard, the mist and the haze are soft wisps. The noise
// also bends each particle's normal, so the sun or the moon lights it on one side, shines through its thin parts and
// glints on the drops. Each particle fades where it comes near the rock or the water behind it, so it has no hard edge
// there.

SAMPLER2D(s_sceneDepth, 1);
SAMPLER3D(s_sprayNoise, 3);

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

// The density of the sheet at a point of the ribbon: smooth noise that turns to long strands as the sheet breaks, and
// is eaten away from the top down. `v` is how far up the ribbon the point is, 0 to 1.
float SheetDensity(vec3 p, float v, float breakup)
{
	float cloud = texture3DLod(s_sprayNoise, p, 0.0f).r;
	float strands = texture3DLod(s_sprayNoise, vec3(p.x * 3.0f, p.y * 0.5f, p.z), 0.0f).a;
	float d = mix(cloud, strands, 0.25f + 0.6f * breakup * v);
	float threshold = mix(0.2f, 0.62f, breakup) + 0.25f * v * breakup - 0.12f * (1.0f - v);
	// The top edge is ragged: the strands reach up to different heights
	float top = 0.6f + 0.4f * texture3DLod(s_sprayNoise, vec3(p.x * 2.0f, 0.3f, p.z), 0.0f).b;
	return smoothstep(threshold, threshold + 0.2f, d) * (1.0f - smoothstep(top - 0.3f, top, v));
}

// The density of a puff: lumps of round cells for the core, soft smooth noise for the mist and the haze
float PuffDensity(vec3 uvw, vec2 corner, float layer, float breakup)
{
	float r = dot(corner, corner);
	vec4 n = texture3DLod(s_sprayNoise, uvw + vec3(corner * 0.3f, 0.0f), 0.0f);
	if (layer < 1.5f)
	{
		float lumps = mix(n.g, n.r, 0.4f) * (1.0f - r);
		float threshold = mix(0.08f, 0.4f, breakup);
		return smoothstep(threshold, threshold + 0.22f, lumps);
	}
	float falloff = max(1.0f - r, 0.0f);
	if (layer < 3.5f)
	{
		return falloff * falloff * (0.35f + 0.9f * n.r);
	}
	return pow(falloff, 1.5f) * smoothstep(0.15f, 0.85f, n.r + 0.35f * n.b - 0.1f);
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
	float layer = v_texcoord0.z;
	vec3 uvw = v_texcoord1.xyz;
	float breakup = v_texcoord1.w;
	vec3 toEye = normalize(u_uwEye.xyz - v_world);
	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	float r = dot(corner, corner);

	float shape;
	vec3 normal;
	// The step along the panel or the puff that finds the slope of the density, for the normal
	float e = 0.08f;
	if (layer < 0.5f)
	{
		float v = corner.y * 0.5f + 0.5f;
		shape = SheetDensity(uvw, v, breakup);
		shape *= 1.0f - smoothstep(0.75f, 1.0f, abs(corner.x));
		shape *= smoothstep(0.0f, 0.08f, v);
		vec2 slope = vec2(SheetDensity(uvw + vec3(0.01f, 0.0f, 0.0f), v, breakup) - shape,
		                  SheetDensity(uvw + vec3(0.0f, 0.01f, 0.0f), v, breakup) - shape) / 0.01f;
		vec3 panelNormal = normalize(v_normal);
		vec3 along = normalize(cross(vec3(0.0f, 1.0f, 0.0f), panelNormal));
		vec3 rising = cross(panelNormal, along);
		normal = normalize(panelNormal - (along * slope.x + rising * slope.y) * 0.01f);
		// Light on the side the eye sees
		normal *= dot(normal, toEye) < 0.0f ? -1.0f : 1.0f;
	}
	else if (layer < 2.5f && layer > 1.5f)
	{
		// A drop: a hard round shape with a round normal
		shape = 1.0f - smoothstep(0.35f, 1.0f, r);
		normal = normalize(right * corner.x + up * corner.y + toEye * sqrt(max(1.0f - r, 0.0f)));
	}
	else
	{
		shape = PuffDensity(uvw, corner, layer, breakup);
		vec2 slope = vec2(PuffDensity(uvw, corner + vec2(e, 0.0f), layer, breakup) - shape,
		                  PuffDensity(uvw, corner + vec2(0.0f, e), layer, breakup) - shape) / e;
		vec3 sphere = right * corner.x + up * corner.y + toEye * sqrt(max(1.0f - r, 0.0f));
		normal = normalize(sphere - (right * slope.x + up * slope.y) * (layer < 1.5f ? 0.25f : 0.1f));
	}

	// Soft: faded near what is behind it, and near the water's surface
	float softRange = layer < 0.5f ? 1.5f : (layer < 1.5f ? 1.2f : (layer < 2.5f ? 0.3f : (layer < 3.5f ? 2.5f : 5.0f)));
	float soft = 1.0f;
	if (sceneDepth > 0.0f)
	{
		soft = clamp(length(SprayWorldAt(uv, sceneDepth) - v_world) / softRange, 0.0f, 1.0f);
	}
	soft *= smoothstep(u_uwParams.z - 0.3f, u_uwParams.z + softRange * 0.5f, v_world.y);
	float cover = clamp(shape * v_texcoord0.w * soft, 0.0f, 1.0f);
	if (cover < 0.002f)
	{
		discard;
	}

	// The light of the sun or the moon: on the side towards it, through the thin parts towards the eye, and glints
	vec3 towards = u_uwAbove.xyz;
	vec3 light = u_uwLightColour.rgb;
	float diffuse = 0.3f + 0.7f * max(dot(normal, towards), 0.0f);
	float thin = layer < 0.5f ? 0.7f : (layer < 1.5f ? 0.45f : (layer < 2.5f ? 0.3f : (layer < 3.5f ? 1.0f : 0.8f)));
	float through = pow(max(dot(-toEye, towards), 0.0f), 6.0f) * thin * (1.0f - 0.5f * shape);
	float shine = layer < 0.5f ? 0.8f : (layer < 1.5f ? 0.3f : (layer < 2.5f ? 2.5f : 0.0f));
	float glint = pow(max(dot(reflect(-towards, normal), toEye), 0.0f), 40.0f) * shine;
	vec3 white = vec3_splat(0.93f);
	vec3 colour = white * (v_color0.rgb + light * diffuse) + light * (through + glint);
	colour = min(colour, vec3_splat(1.6f));
	gl_FragColor = vec4(colour * cover, cover);
}
