$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // the font atlas (R8 coverage, GameFont)

// Text of the original fonts, render mode 16: colour and alpha = texture x diffuse, SRCALPHA /
// INVSRCALPHA, alpha test GREATEREQUAL 5
void main()
{
	float alpha = texture2D(s_diffuse, v_texcoord0.xy).r * v_color0.a;
	if (alpha < 5.0f / 255.0f)
	{
		discard;
	}
	gl_FragColor = vec4(v_color0.rgb, alpha);
}
