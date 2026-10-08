$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
	mat4 model;
	// the w of the first three columns carries other data for the object shaders (vs_object: alpha, UV offset, sink
	// offset), not part of the transform
	model[0] = vec4(i_data0.xyz, 0.0f);
	model[1] = vec4(i_data1.xyz, 0.0f);
	model[2] = vec4(i_data2.xyz, 0.0f);
	model[3] = vec4(i_data3.xyz, 1.0f); // w: the window light of houses (vs_object)

	vec4 position = instMul(model, mul(u_model[0], vec4(a_position.x, 0.0f, a_position.y, 1.0f)));
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	gl_Position = mul(u_modelViewProj, position);
	gl_Position.z = 0.0f;
}
