$input a_position, a_texcoord0, a_color0, a_color1
$output v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

// The original's world triangles, plain and indexed (src/Graphics/WorldTriangles.h): the
// vertices are already in the world (the world matrix is the identity for the particle meshes;
// the fragment meshes transform them themselves) and already lit on the CPU (the land light and the
// model light), so the vertex only goes through the camera. fs_object does the
// rest, as for the models: v_position.w and v_texcoord0.zw are the specular, a_color1 (0 for the plain triangles;
// the indexed ones copy each vertex's).
void main()
{
	v_position = vec4(a_position.xyz, a_color1.b);
	v_texcoord0 = vec4(a_texcoord0, a_color1.rg);
	v_normal = vec3_splat(0.0f);
	v_color0 = a_color0;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
