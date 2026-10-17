$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// EXPERIMENT (not for merge): the shade where the things round a point and its own folds hide the sky, at half the
// screen's size, from the scene's depth (horizon-based). Each pixel finds its place and its surface's direction from the
// depth, then looks along four directions on the screen, turned by the blue noise, for the highest point over its
// surface within a few units. The higher those points, the less of the sky the pixel sees. r: how much of the sky it
// sees, 0 to 1; g: its distance from the eye.

SAMPLER2D(s_sceneDepth, 0);
SAMPLER2D(s_aoNoise, 1);

// x: how far the search goes, in world units; y: the pixels of the full screen that one unit covers at a distance of
// one unit; zw: the size of the full screen
uniform vec4 u_aoParams;

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

vec3 PointAt(vec2 uv)
{
	return WorldAt(uv, texture2DLod(s_sceneDepth, uv, 0.0f).r);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	float depth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	if (depth <= 0.0f)
	{
		gl_FragColor = vec4(1.0f, 1e6f, 0.0f, 1.0f);
		return;
	}
	vec3 p = WorldAt(uv, depth);
	float range = length(p - eye);
	// The surface's direction from the nearer of the two neighbours along each axis of the full screen
	vec2 pixel = 1.0f / u_aoParams.zw;
	vec3 right = PointAt(uv + vec2(pixel.x, 0.0f)) - p;
	vec3 left = p - PointAt(uv - vec2(pixel.x, 0.0f));
	vec3 down = PointAt(uv + vec2(0.0f, pixel.y)) - p;
	vec3 up = p - PointAt(uv - vec2(0.0f, pixel.y));
	vec3 across = dot(right, right) < dot(left, left) ? right : left;
	vec3 along = dot(down, down) < dot(up, up) ? down : up;
	vec3 n = normalize(cross(along, across));
	if (dot(n, eye - p) < 0.0f)
	{
		n = -n;
	}

	float radius = u_aoParams.x;
	float reach = min(radius * u_aoParams.y / max(range, 1.0f), 80.0f);
	float turn = texture2DLod(s_aoNoise, gl_FragCoord.xy / 64.0f, 0.0f).r * 6.2831853f;
	float jitter = texture2DLod(s_aoNoise, gl_FragCoord.yx / 64.0f + 0.37f, 0.0f).r;
	float hidden = 0.0f;
	for (int d = 0; d < 4; ++d)
	{
		float angle = turn + float(d) * 1.5707963f;
		vec2 dir = vec2(cos(angle), sin(angle)) * pixel;
		float highest = 0.0f;
		for (int s = 0; s < 4; ++s)
		{
			float stepPixels = max(reach * (float(s) + jitter) / 4.0f, 1.0f);
			vec2 at = uv + dir * stepPixels;
			if (at.x < 0.0f || at.y < 0.0f || at.x > 1.0f || at.y > 1.0f)
			{
				break;
			}
			vec3 v = PointAt(at) - p;
			float len2 = dot(v, v);
			float falloff = max(1.0f - len2 / (radius * radius), 0.0f);
			highest = max(highest, dot(v, n) * inversesqrt(max(len2, 1e-6f)) * falloff);
		}
		hidden += highest;
	}
	float open = clamp(1.0f - hidden / 4.0f, 0.0f, 1.0f);
	gl_FragColor = vec4(open, range, 0.0f, 1.0f);
}
