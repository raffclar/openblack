$input v_position, v_texcoord0, v_normal, v_color0, v_haze, v_snow, v_snowLight

#include <bgfx_shader.sh>

// The colours of the pattern that moves
SAMPLER2D(s_diffuse, 0);
// The alpha of the pattern, in its red
SAMPLER2D(s_skinAlpha, 10);
// y: the alpha reference, of 1
uniform vec4 u_skyAlphaThreshold;

// The ghost of a thing that has gone draws the pattern over its model. A texel shows when its alpha is at the reference
// or more. The texel lays the depth and adds its colour, in the light of the model, by its alpha.
void main()
{
	float alpha = texture2D(s_skinAlpha, v_texcoord0.xy).r;
	float reference = floor(u_skyAlphaThreshold.y * 255.0f + 0.5f);
	if (floor(alpha * 255.0f + 0.5f) < reference)
	{
		discard;
	}
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb * v_color0.rgb;
	gl_FragColor = vec4(min(colour + v_haze.rgb, vec3_splat(1.0f)), alpha);
}
