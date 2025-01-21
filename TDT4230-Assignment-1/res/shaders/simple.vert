#version 430 core

in layout(location = 0) vec3 position;
in layout(location = 1) vec3 normal_in;
in layout(location = 2) vec2 textureCoordinates_in;

uniform layout(location = 3) mat4 MVP;
uniform layout(location = 4) mat4 model_matrix;
uniform layout(location = 5) mat3 normal_matrix;

out layout(location = 0) vec3 normal_out;
out layout(location = 1) vec2 textureCoordinates_out;
out layout(location = 2) vec3 frag_pos_out;

void main()
{
    // To frag shader
    //normal_out = normal_in;
    normal_out = normalize(normal_matrix * normal_in);
    textureCoordinates_out = textureCoordinates_in;
    frag_pos_out = vec3(model_matrix * vec4(position, 1.0f));

    gl_Position = MVP * vec4(position, 1.0f);
}
