$input v_position, v_texcoord0, v_normal, v_color0, v_haze, v_snow, v_snowLight

#include <bgfx_shader.sh>

#include "snow_object.sh"

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_skyAlphaThreshold;
// EXPERIMENT (not for merge): xyz the direction towards the sun or the moon, w how strongly its light comes through the
// leaves (0 with the experiment off); and the colour of its light
uniform vec4 u_foliageLight;
uniform vec4 u_foliageLightColour;

void main()
{
	// A burning tree's foliage is cut away at a higher alpha, the hotter it is
	float alphaThreshold = (u_skyAlphaThreshold.y > 0.0f && v_color0.a > 0.0f) ? v_color0.a : u_skyAlphaThreshold.y;

	vec3 light = v_color0.rgb;

	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	diffuseTex.rgb = diffuseTex.rgb * light;
	if (diffuseTex.a <= alphaThreshold)
	{
		discard;
	}
	// Snow covers the tree where it shows, in its own light, over its own texture
	float snowLevel = floor(v_snow.z * 255.0f + 0.5f);
	if (snowLevel > 0.0f && SnowShows(snowLevel, texture2D(s_snowAlpha, v_snow.xy).r))
	{
		diffuseTex.rgb = texture2D(s_snow, v_snow.xy).rgb * v_snowLight;
	}
	// EXPERIMENT (not for merge): looking towards the sun, the leaves glow with the light that comes through them, and
	// their edges catch it
	if (u_foliageLight.w > 0.0f)
	{
		vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
		vec3 view = normalize(v_position.xyz - eye);
		vec3 towards = normalize(u_foliageLight.xyz);
		float through = pow(max(dot(view, towards), 0.0f), 6.0f);
		vec3 n = normalize(v_normal.xyz);
		float edge = pow(1.0f - abs(dot(n, view)), 3.0f) * (0.5f + 0.5f * dot(view, towards));
		vec3 leaf = diffuseTex.rgb / max(light, vec3_splat(0.05f));
		diffuseTex.rgb += leaf * u_foliageLightColour.rgb * (through * 0.55f + edge * 0.25f) * u_foliageLight.w;
	}
	// The distance haze is added after the texture
	// A tree fading out in the distance is blended by its alpha
	float alpha = diffuseTex.a * (1.0f - v_haze.w);
	// EXPERIMENT (not for merge): or it drops a share of its fragments in a fixed screen pattern and the rest are solid
	if (u_skyAlphaThreshold.z > 0.5f)
	{
		float pattern = fract(52.9829189f * fract(dot(gl_FragCoord.xy, vec2(0.06711056f, 0.00583715f))));
		if (1.0f - v_haze.w <= pattern)
		{
			discard;
		}
		alpha = 1.0f;
	}
	gl_FragColor = vec4(min(diffuseTex.rgb + v_haze.rgb, vec3_splat(1.0f)), alpha);
}
