$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0); // graphics::GameFont's atlas (R8 coverage)

// The menus' text with the original fonts (render mode 16: alpha = texture x diffuse, test GREATEREQUAL 5)
void main()
{
	float alpha = texture2D(s_texture, v_texcoord0.xy).r * v_color0.a;
	if (alpha < 5.0f / 255.0f)
	{
		discard;
	}
	gl_FragColor = vec4(v_color0.rgb, alpha);
}
