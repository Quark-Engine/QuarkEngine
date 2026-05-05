#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

uniform vec3 lightDir;
uniform vec4 lightColor;
uniform vec4 ambient;
uniform vec3 viewPos;

uniform mat4 lightVP;
uniform sampler2D shadowMap;

uniform int shadowMapResolution;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec3 l = -lightDir;

    float NdotL = max(dot(normal, l), 0.0);
    lightDot += lightColor.rgb*NdotL;

    float specCo = 0.0;
    if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(l), normal))), 16.0);
    specular += specCo;

    finalColor = (texelColor*((colDiffuse + vec4(specular, 1.0))*vec4(lightDot, 1.0)));

    vec4 fragPosLightSpace = lightVP*vec4(fragPosition, 1);

    fragPosLightSpace.xyz /= fragPosLightSpace.w;
    fragPosLightSpace.xyz = (fragPosLightSpace.xyz + 1.0)/2.0;

    vec2 sampleCoords = fragPosLightSpace.xy;
    float curDepth = fragPosLightSpace.z;

    float bias = max(0.0002*(1.0 - dot(normal, l)), 0.00002) + 0.00001;
    int shadowCounter = 0;
    const int numSamples = 9;

    vec2 texelSize = vec2(1.0/float(shadowMapResolution));
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float sampleDepth = texture(shadowMap, sampleCoords + texelSize*vec2(x, y)).r;
            if (curDepth - bias > sampleDepth) shadowCounter++;
        }
    }
    finalColor = mix(finalColor, vec4(0, 0, 0, 1), float(shadowCounter)/float(numSamples));

    finalColor += texelColor*(ambient/10.0)*colDiffuse;
    finalColor = pow(finalColor, vec4(1.0/2.2));
}