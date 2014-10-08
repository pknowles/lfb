#version 420

#include "lfb.glsl"

LFB_DEC(toSort);

vec4 frags[MAX_FRAGS];

void main()
{
	int i = gl_VertexID;
	int numFrags = loadFragments(toSort, i, frags);
	sortFragments(frags, numFrags);
	writeFragments(toSort, frags);
}
