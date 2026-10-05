UBO(0) uniform Draw
{
	mat4 uViewProj;
	mat4 uModel;
	mat4 uGameModel;
	vec4 uHorizon;     // kolor strefy horyzontu, w = głębokość (0 = brak)
	vec4 uLightDirAmb; // kierunek światła podglądu, w = ambient
	vec4 uTint;
	vec4 uParams;      // x = t interpolacji, y = przesunięcie z do układu gry, z = jasność bazowa B
	ivec4 uFlags;      // x = tryb oświetlenia podglądu, y = alpha test, z = oświetlenie jak w grze,
	                   // w = tryb rysowania linku (MeshLink::flags, docs/formaty/3db.md §3.2); 103 = brzegi trybu 3
};
