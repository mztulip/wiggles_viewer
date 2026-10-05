UBO(0) uniform Draw
{
	mat4 uViewProj;
	mat4 uWorld;
	vec4 uLightDirAmb; // kierunek światła podglądu, w = ambient
	vec4 uTint;
	ivec4 uFlags;      // x = oświetlenie jak w grze, y = przebieg detali .l2m
};
