#version 330

uniform sampler2D screen_texture;
uniform float time;
uniform float darken_screen_factor;
uniform int advanced;

in vec2 texcoord;

layout(location = 0) out vec4 color;

vec2 distort(vec2 uv) 
{
    if (advanced == 1) {
        uv.y += cos(time + 10 * uv.x) * 0.0005 +
                sin(time + uv.x * 40) * 0.0005 +
                cos(time + uv.x * 100) * 0.0007;
    }
    return uv;
}

vec4 color_shift(vec4 in_color) 
{
    if (advanced == 1) {
        in_color -= vec4(0.1, 0.05, 0.0, 0.0);
    }
	return in_color;
}

vec4 fade_color(vec4 in_color) 
{
	if (darken_screen_factor > 0)
		in_color -= darken_screen_factor * vec4(0.8, 0.8, 0.8, 0);
	return in_color;
}

void main()
{
	vec2 coord = distort(texcoord);

    vec4 in_color = texture(screen_texture, coord);
    color = color_shift(in_color);
    color = fade_color(color);
}