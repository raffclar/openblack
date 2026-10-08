$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);    // sky.raw, cut to 4 bits per channel at load (ARGB4444, like the original)
SAMPLER2D(s_alpha, 1);      // skya.raw, the same
SAMPLER2D(s_reflection, 2); // what is under the sea: the mirrored sky and land, the moon's reflection, the hand glow

uniform vec4 u_seaColour; // rgb: landscape light table[255] (the sea vertex colour); white if unavailable
uniform vec4 u_seaParams; // x: tiling period, y: frame counter, zw: 0.9 * normalised horizontal camera forward
uniform vec4 u_seaRows;   // the screen rows: x first row y, y count n, z 1 / depth of row 0, w its step per row
uniform vec4 u_seaMode;   // x: 0 rows, 1 level-0 quad; y: row 0 gets alpha 0x20; zw: wind drift offset
uniform vec4 u_seaCamera; // xyz: camera origin

// The point of the sea plane y = 0 under the centre of screen pixel (px, py), py counted from the top; w = 0 when the
// pixel is above the horizon
vec4 PlanePoint(float px, float py)
{
	vec2 ndc = vec2((px + 0.5f) / u_viewRect.z * 2.0f - 1.0f, 1.0f - (py + 0.5f) / u_viewRect.w * 2.0f);
	vec4 p = mul(u_invViewProj, vec4(ndc, 0.5f, 1.0f));
	vec3 direction = p.xyz / p.w - u_seaCamera.xyz;
	if (direction.y >= 0.0f)
	{
		return vec4_splat(0.0f);
	}
	return vec4(u_seaCamera.xyz + direction * (-u_seaCamera.y / direction.y), 1.0f);
}

float ViewDepth(vec3 p)
{
	return mul(u_view, vec4(p, 1.0f)).z;
}

// The UV of even vertex row r at pixel column px: the plane point of the row moved along the camera
// forward by the ripple, (off + xz) / P
vec2 RowUv(float r, float px)
{
	vec3 p = PlanePoint(px, u_seaRows.x + 2.0f * r).xyz;
	// the row's 1 / depth, affine in the screen y from the range vertices (a step added per row)
	float iz = u_seaRows.z + r * u_seaRows.w;
	// row r uses the phase (frame + 2 (r + 1)) & 15 of a 16-entry sine table (sin(i pi / 8)); full amplitude beyond
	// depth 70, (1 / iz - 30) * 0.025 of it beyond 30, none closer
	float phase = mod(u_seaParams.y + 2.0f * (r + 1.0f), 16.0f);
	float s = sin(phase * 3.14159265f / 8.0f);
	float amplitude = iz < 1.0f / 70.0f ? s : (iz < 1.0f / 30.0f ? (1.0f / iz - 30.0f) * 0.025f * s : 0.0f);
	return (u_seaMode.zw + p.xz + u_seaParams.zw * amplitude) / u_seaParams.x;
}

// The alpha of vertex row r (both its vertices, from the depth of the left one): 255 up to view depth
// 7000, down to 80 at 14000, rounded to the nearest; row 0 has 0x20 when it is inside
// the screen
float RowAlpha(float r)
{
	if (r < 0.5f && u_seaMode.y > 0.5f)
	{
		return 32.0f / 255.0f;
	}
	float d = ViewDepth(PlanePoint(0.0f, u_seaRows.x + 2.0f * r).xyz);
	float a = d > 14000.0f ? 80.0f : (d < 7000.0f ? 255.0f : floor(255.0f - (d - 7000.0f) * (175.0f / 7000.0f) + 0.5f));
	return a / 255.0f;
}

void main()
{
	// what the sea is blended over: the reflection target, drawn with the same size and projection
	vec2 reflectionUv = (gl_FragCoord.xy - u_viewRect.xy) / u_viewRect.zw;

	vec2 uv;
	float vertexAlpha;
	if (u_seaMode.x < 0.5f)
	{
		float px = floor(gl_FragCoord.x - u_viewRect.x);
#if BGFX_SHADER_LANGUAGE_GLSL
		float py = floor(u_viewRect.w - (gl_FragCoord.y - u_viewRect.y));
#else
		float py = floor(gl_FragCoord.y - u_viewRect.y);
#endif
		vec4 here = PlanePoint(px, py);
		if (here.w == 0.0f)
		{
			discard; // above the horizon: the sky
		}
		// The triangles join vertex rows 0..n, 2 px apart from the first row: outside them there is no sea, only what
		// was drawn before it (the sky and land under the water)
		float k = py - u_seaRows.x;
		if (k < 0.0f || k >= 2.0f * u_seaRows.y)
		{
			uv = vec2_splat(0.0f);
			vertexAlpha = 0.0f;
		}
		else
		{
			// No perspective correction (rhw = 1): the UV and alpha go linearly from a vertex row to the next. The odd
			// rows repeat the UV of the row before, so the texture line is the same for 2 px, then blends over 2 px.
			float r = floor(k / 2.0f);
			float halfway = k - 2.0f * r; // 0 or 1: the pixel on the row or halfway to the next one
			if (mod(r, 2.0f) < 0.5f)
			{
				uv = RowUv(r, px);
			}
			else
			{
				uv = halfway < 0.5f ? RowUv(r - 1.0f, px) : 0.5f * (RowUv(r - 1.0f, px) + RowUv(r + 1.0f, px));
			}
			vertexAlpha = halfway < 0.5f ? RowAlpha(r) : 0.5f * (RowAlpha(r) + RowAlpha(r + 1.0f));
		}
	}
	else
	{
		// The level-0 quad: UV (x + 70000) / 2800 and (70000 - z) / 2800 from the corners' {0, 50}, plus its own drift;
		// vertex alpha 255, no ripple, no fade
		uv = vec2(v_texcoord0.x + 70000.0f, 70000.0f - v_texcoord0.y) / u_seaParams.x + u_seaMode.zw;
		vertexAlpha = 1.0f;
	}

	// Light: the time-of-day full-light colour, landscape light table entry 255
	vec3 light = u_seaColour.rgb;
	vec3 diffuse_colour = light * texture2D(s_diffuse, uv).rgb;
	vec3 reflect_colour = texture2D(s_reflection, reflectionUv).rgb;

	// Material mode 5: COLOR = sky.raw * diffuse, ALPHA = skya.raw * vertex alpha, blended SRCALPHA / INVSRCALPHA over
	// what is under the sea
	float alpha = texture2D(s_alpha, uv).r * vertexAlpha;

	gl_FragColor = vec4(mix(reflect_colour, diffuse_colour, alpha), 1.0f);
}
