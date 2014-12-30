#version 420

#include "lfb.glsl"

LFB_DEC(toSort);

LFB_FRAG_TYPE frags[MAX_FRAGS];
#define FRAGS(i) frags[i] //used in sorting.glsl

#include "sorting.glsl"

void main()
{
	int index = gl_VertexID;
	
	LFB_INIT(toSort, index);
	int fragCount = 0;
	LFB_FOREACH(toSort, frag)
		if (fragCount < MAX_FRAGS)
			frags[fragCount++] = frag;
	}
	
	sort_insert(fragCount);
	
	fragCount = 0;
	LFB_FOREACH(toSort, frag)
		if (fragCount < MAX_FRAGS)
			LFB_SET(toSort, frags[fragCount++]);
	}
}
