#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform int useTexture;

out vec4 outColor;

void main()
{
    if (useTexture == 1) {
        vec4 texColor = texture(texture0, fragTexCoord);
        if (texColor.a < 0.5) discard;
    }
    
    float depth = gl_FragCoord.z;
    outColor = vec4(vec3(depth), 1.0);
}
