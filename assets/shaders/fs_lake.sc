$input v_texcoord0, v_world

#include <bgfx_shader.sh>

// The water of a lake above the sea of a generated land. It has the look of the sea (see fs_water) at the lake's own
// level, and it is blended over the land that is drawn before it, by the coast alpha of the land under it.

// The sea's texture, and the sea's reflection
SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_reflection, 1);

// The uniforms of the sea, as fs_water has them
uniform vec4 u_seaColour;
uniform vec4 u_seaParams;
uniform vec4 u_seaRows;
uniform vec4 u_seaMode;
uniform vec4 u_seaCamera;
uniform vec4 u_seaDrift;

// The point of the water at height level under the centre of screen pixel (px, py), py counted from the top. The
// camera is above the water where the lake shows, so the line of sight goes down to it.
vec3 PlanePoint(float px, float py, float level)
{
	vec2 ndc = vec2((px + 0.5f) / u_viewRect.z * 2.0f - 1.0f, 1.0f - (py + 0.5f) / u_viewRect.w * 2.0f);
	vec4 p = mul(u_invViewProj, vec4(ndc, 0.5f, 1.0f));
	vec3 direction = p.xyz / p.w - u_seaCamera.xyz;
	float down = min(direction.y, -0.0001f);
	return u_seaCamera.xyz + direction * ((level - u_seaCamera.y) / down);
}

float ViewDepth(vec3 p)
{
	return mul(u_view, vec4(p, 1.0f)).z;
}

// The texture coordinates of row r at pixel column px, as the sea's rows have them
vec2 RowUv(float r, float px, float level)
{
	vec3 p = PlanePoint(px * u_seaMode.z, u_seaRows.x + 2.0f * r, level);
	float inverseDepth = 1.0f / max(ViewDepth(p), 1.0f);
	float step = mod(u_seaParams.y + 2.0f * (r + 1.0f), 16.0f);
	float s = sin(step * 3.14159265f / 8.0f);
	float amplitude = inverseDepth < 1.0f / 70.0f ? s : (inverseDepth < 1.0f / 30.0f ? (1.0f / inverseDepth - 30.0f) * 0.025f * s : 0.0f);
	return (u_seaDrift.xy + (p.xz + u_seaParams.zw * amplitude)) / u_seaParams.x;
}

// The alpha of row r, as the sea's rows have it
float RowAlpha(float r, float level)
{
	float depth = ViewDepth(PlanePoint(0.0f, u_seaRows.x + 2.0f * r, level));
	float alpha = depth > 14000.0f ? 80.0f : (depth < 7000.0f ? 255.0f : floor(255.0f - (depth - 7000.0f) * (175.0f / 7000.0f) + 0.5f));
	return alpha / 255.0f;
}

// The coast alpha of the land, 0 to 1, for land at this height above the water's level. It is the alpha that the land
// at the same place above the sea gets (see block_texture::CoastAlpha), with no noise. The level of a lake is a quarter
// of an altitude under a whole altitude. That whole altitude is the first one above the water, as altitude 4 is above
// the sea, where the land is opaque. The altitude under it is as altitude 3 at the sea, where the sea shows a little.
float CoastAlpha(float aboveLevel)
{
	float h = floor(255.0f * (aboveLevel / 0.67f + 3.75f));
	if (h < 256.0f)
	{
		return 0.0f;
	}
	if (h >= 1024.0f)
	{
		return 1.0f;
	}
	if (h < 512.0f)
	{
		return 0.0f;
	}
	if (h > 950.0f)
	{
		return 1.0f;
	}
	float quotient = (15.0f * h - 14250.0f) / 438.0f;
	float step = clamp(15.0f + (quotient < 0.0f ? ceil(quotient) : floor(quotient)), 0.0f, 15.0f);
	// The steps of the coast alpha: 0, 0, 0, 1, 2, 4, 6, 8, 9, 10, 10, 11, 12, 13, 14, 15
	float alpha = step < 3.0f ? 0.0f : (step < 5.0f ? step - 2.0f : (step < 8.0f ? 2.0f * step - 6.0f : (step < 10.0f ? step + 1.0f : (step < 11.0f ? 10.0f : step))));
	return alpha / 15.0f;
}

void main()
{
	float level = v_texcoord0.y;
	vec2 reflectionUv = (gl_FragCoord.xy - u_viewRect.xy) / u_viewRect.zw;
	float px = floor(gl_FragCoord.x - u_viewRect.x);
#if BGFX_SHADER_LANGUAGE_GLSL
	float py = floor(u_viewRect.w - (gl_FragCoord.y - u_viewRect.y));
#else
	float py = floor(gl_FragCoord.y - u_viewRect.y);
#endif

	vec2 uv;
	float rowAlpha;
	if (u_seaMode.x > 0.5f)
	{
		// The still sea's texture, wholly opaque
		uv = u_seaDrift.zw + vec2(v_world.x + 70000.0f, 70000.0f - v_world.z) / u_seaParams.x;
		rowAlpha = 1.0f;
	}
	else
	{
		// The rows of the screen, two pixels apart, as the sea's
		float k = max(py - u_seaRows.x, 0.0f);
		float r = floor(k / 2.0f);
		float halfway = k - 2.0f * r;
		if (mod(r, 2.0f) < 0.5f)
		{
			uv = RowUv(r, px, level);
		}
		else
		{
			uv = halfway < 0.5f ? RowUv(r - 1.0f, px, level) : 0.5f * (RowUv(r - 1.0f, px, level) + RowUv(r + 1.0f, px, level));
		}
		rowAlpha = halfway < 0.5f ? RowAlpha(r, level) : 0.5f * (RowAlpha(r, level) + RowAlpha(r + 1.0f, level));
	}

	// The sea's texture in the sea's colour, blended by its alpha over the reflection. The reflection is the sky that the
	// sea mirrors. The sky has no height, so the pixel of the sea's mirror under the lake's pixel shows it.
	vec4 texel = texture2D(s_diffuse, uv);
	vec3 sea = u_seaColour.rgb * texel.rgb;
	vec3 beneath = texture2D(s_reflection, reflectionUv).rgb;
	vec3 water = mix(beneath, sea, texel.a * rowAlpha);
	// The land under the water shows through by its coast alpha. At the edge of the surface, the water fades to nothing.
	gl_FragColor = vec4(water, 1.0f - CoastAlpha(v_texcoord0.x - level));
}
