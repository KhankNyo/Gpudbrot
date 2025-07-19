#version 460 core

#define COLOR_PALETTE_SIZE 16

uniform float u_ScreenToWorldScaleFactor;
uniform float u_WorldBottom;
uniform float u_WorldLeft;
uniform vec3 u_ColorPalette[COLOR_PALETTE_SIZE];
uniform int u_IterationCount;
out vec4 FragColor;

void main()
{
    float MaxValueSquared = 4.0f;
    float Zx = 0;
    float Zy = 0;
    float Zix = gl_FragCoord.x * u_ScreenToWorldScaleFactor + u_WorldLeft;
    float Ziy = gl_FragCoord.y * u_ScreenToWorldScaleFactor + u_WorldBottom;

    /* calculate whether the current Zi* is in the set or not */
    int i;
    for (i = 0; 
         i < u_IterationCount
         && (Zx*Zx + Zy*Zy) < MaxValueSquared;
         i++)
    {
        float Tmp = Zx*Zx - Zy*Zy + Zix;
        Zy = 2.0*Zy*Zx + Ziy;
        Zx = Tmp;
    }

    /* determine the color */
    vec3 Color = u_ColorPalette[i & (COLOR_PALETTE_SIZE - 1)] * float(i < u_IterationCount);

    FragColor = vec4(Color, 1.0f);
}
