#version 430 core

#define LIGHT_SOURCES 3
#define BALL_RADIUS 3

in layout(location = 0) vec3 normal_in;
in layout(location = 1) vec2 texture_coordinates_in;
in layout(location = 2) vec3 frag_pos_in;

// NOTE: My vertex shader uses uniform at locations 3 and 4.
uniform layout(location = 6) vec3 view_position;
uniform layout(location = 7) vec3 light_positions[LIGHT_SOURCES];
uniform vec3 ball_position;

out vec4 color;

float rand(vec2 co) { return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453); }
float dither(vec2 uv) { return (rand(uv)*2.0-1.0) / 256.0; }

vec3 reject(vec3 from, vec3 onto) {
    return from - onto*dot(from, onto)/dot(onto, onto);
}

void main()
{
    vec3 normal = normalize(normal_in);
    color = vec4(0.5 * normal + 0.5, 1.0);

    // Ambient
    vec3 ambient_light = vec3(0.05, 0.05, 0.05);
    vec3 ambient = ambient_light;// * color.rgb;

    // Attenuation
    float l_a = 0.25;
    float l_b = 0.05;
    float l_c = 0.005;

    vec3 diffuse = vec3(0.0, 0.0, 0.0);
    vec3 specular = vec3(0.0, 0.0, 0.0);
    
    for (int i = 0; i < LIGHT_SOURCES; i++) {
        vec3 light_position = light_positions[i];

        // Shadow calculation
        float shadow_factor = 0; // 0 means no shadow, 1 means maximum shadow
        vec3 frag_light = frag_pos_in - light_position;
        vec3 frag_ball  = frag_pos_in - ball_position;
        // Check if light is closer to frag than ball and not pointing opposite directions
        if (length(frag_light) >= length(frag_ball) && dot(frag_ball, frag_light) > 0) {
            float reject_len = length(reject(frag_ball, frag_light));
            if (reject_len < BALL_RADIUS) {
                shadow_factor = 0.75;
            }
        }

        vec3 light_color = vec3(1.0, 1.0, 1.0); // White light for now
        light_color *= (1 - shadow_factor);

        // PHONG
        // Attenuation
        float dist = distance(light_position, frag_pos_in);
        float L = 1 / (l_a + dist * l_b + dist * dist * l_c);

        // Diffuse
        vec3 light_dir = normalize(light_position - frag_pos_in);
        float intensity = max(dot(light_dir, normal), 0.0);
        diffuse += L * intensity * light_color;

        // Specular
        vec3 reflect_dir = reflect(-light_dir, normal);
        vec3 surface_eye = normalize(view_position - frag_pos_in);
        float shininess = 32.0;
        float spec_intensity = pow(max(dot(reflect_dir, surface_eye), 0.0), shininess);
        specular += L * spec_intensity * light_color;
    }

    vec3 phong = ambient + diffuse + specular;
    color = vec4(phong, 1.0) + dither(texture_coordinates_in);
}
