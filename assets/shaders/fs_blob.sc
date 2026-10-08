$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// human_shadow.raw, already cut to 4 bits when it loads (Texture2DLoader, graphics::argb4444::Cut): the original keeps
// byte & 0xF0 as the alpha of a black texel and D3D filters those nibbles, so nothing is quantised here
SAMPLER2D(s_diffuse, 0);

// Render mode 6: colour = texture x diffuse (black), alpha = texture alpha x diffuse alpha, SRCALPHA / INVSRCALPHA
void main()
{
	float alpha = texture2D(s_diffuse, v_texcoord0.xy).r;
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, alpha * v_color0.a);
}
