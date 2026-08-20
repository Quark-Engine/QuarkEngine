#version 330

out vec4 finalColor;

void main()
{
    finalColor = vec4(gl_FragCoord.z);
}