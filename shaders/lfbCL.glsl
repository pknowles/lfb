/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#define LFB_RAGGED_INTERLEAVE defined_by_app

#define LFB_TYPE_RAGGED

struct LFBInfo
{
	ivec2 size; //dimensions for hash
	int depthOnly;
	int interleaveOffset; //offset into offsets to find reduced prefix sums (due to max() operation)
};
struct LFBTmp
{
	int fragCount;
	int fragOffset;
	int fragIndex;
	int stride;
	int i;
};

//Seems to be faster to interleave fragment attributes too, so we pack using floats rather than LFB_TYPE
#undef LFB_EXPOSE_DATA
#undef LFB_EXPOSE_DATA_GET
#undef LFB_EXPOSE_DATA_SET
#if LFB_BINDLESS
	#define LFB_EXPOSE_DATA float*
	#define LFB_EXPOSE_DATA_GET(buffer, index) buffer[index]
	#define LFB_EXPOSE_DATA_SET(buffer, index, val) buffer[index] = val
#else
	#if LFB_READONLY
		#define LFB_EXPOSE_DATA layout(r32f) readonly imageBuffer
	#else
		#define LFB_EXPOSE_DATA layout(r32f) imageBuffer
	#endif
	#define LFB_EXPOSE_DATA_GET(buffer, index) imageLoad(buffer, index).r
	#define LFB_EXPOSE_DATA_SET(buffer, index, val) imageStore(buffer, index, vec4(val, 0.0, 0.0, 0.0));
#endif

#define LFB_TMP_CONSTRUCTOR LFBTmp(0, 0, 0, 0, -1)

#define OFFSETS_TYPE LFB_EXPOSE_TABLE
#define COUNTS_TYPE LFB_EXPOSE_TABLE_COHERENT
#define DATA_TYPE LFB_EXPOSE_DATA

#define LFB_UNIFORMS in OFFSETS_TYPE offsets, in COUNTS_TYPE counts, DATA_TYPE data

//NOTE: uncommon to set layout() after uniform, but seems to work
#define LFB_DEC(suffix) \
uniform COUNTS_TYPE counts##suffix; \
uniform OFFSETS_TYPE offsets##suffix; \
uniform DATA_TYPE data##suffix; \
uniform LFBInfo lfbInfo##suffix; \
LFBTmp lfbTmp##suffix = LFB_TMP_CONSTRUCTOR;

#define addFragment(suffix, hash, fragDat) _addFragment(lfbInfo##suffix, lfbTmp##suffix, offsets##suffix, counts##suffix, data##suffix, hash, fragDat)

#define LFB_INIT(suffix, index) \
{ \
	lfbTmp##suffix.fragIndex = (index); \
	int run = lfbTmp##suffix.fragIndex % LFB_RAGGED_INTERLEAVE; \
	int base = (lfbTmp##suffix.fragIndex / LFB_RAGGED_INTERLEAVE); \
	int baseOffset = LFB_EXPOSE_TABLE_GET(offsets##suffix, lfbInfo##suffix.interleaveOffset + base); \
	baseOffset *= LFB_RAGGED_INTERLEAVE; \
	lfbTmp##suffix.fragOffset = baseOffset * LFB_FRAG_SIZE + run; \
	lfbTmp##suffix.fragCount = LFB_EXPOSE_TABLE_GET(counts##suffix, lfbTmp##suffix.fragIndex); \
}

#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount

#define LFB_COUNT_AT(suffix, index) \
	LFB_EXPOSE_TABLE_GET(counts##suffix, (index))


#if LFB_FRAG_SIZE == 1
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)) \
	)
#elif LFB_FRAG_SIZE == 2
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)), \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+1)) \
	)
#elif LFB_FRAG_SIZE == 3
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)), \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+1)), \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+2)) \
	)
#elif LFB_FRAG_SIZE == 4
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)), \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+1)), \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+2)), \
		LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+3)) \
	)
#endif

#define LFB_ITER_BEGIN(suffix) lfbTmp##suffix.i = 0
#define LFB_ITER_CONDITION(suffix) (lfbTmp##suffix.i < lfbTmp##suffix.fragCount)
#define LFB_ITER_INC(suffix) ++lfbTmp##suffix.i

#if LFB_L_REVERSE == 1
#define LFB_GET(suffix) LFB_LOAD(suffix, (lfbTmp##suffix.fragCount-1-lfbTmp##suffix.i))
#else
#define LFB_GET(suffix) LFB_LOAD(suffix, lfbTmp##suffix.i)
#endif

#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE fragDat)
{
	int i = LFB_EXPOSE_TABLE_ADD(counts, fragIndex, 1U);

	if (info.depthOnly == 0)
	{
		int run = fragIndex % LFB_RAGGED_INTERLEAVE;
		int base = (fragIndex / LFB_RAGGED_INTERLEAVE);
	
		int offset = LFB_EXPOSE_TABLE_GET(offsets, info.interleaveOffset + base);
		//uint maxCount = imageLoad(offsets, info.interleaveOffset + base + 1).r - offset;
		offset *= LFB_RAGGED_INTERLEAVE;
	
		//if (i > maxCount)
		{
			//imageStore(outOfMemory, 0, uvec4(1U));
			//fragColour = vec4(1,0,0,1);
		}
		//else
		{
			LFB_EXPOSE_DATA_SET(data, offset*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+0), fragDat.x);
			#if LFB_FRAG_SIZE > 1
			LFB_EXPOSE_DATA_SET(data, offset*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+1), fragDat.y);
			#endif
			#if LFB_FRAG_SIZE > 2
			LFB_EXPOSE_DATA_SET(data, offset*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+2), fragDat.z);
			#endif
			#if LFB_FRAG_SIZE > 3
			LFB_EXPOSE_DATA_SET(data, offset*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+3), fragDat.w);
			#endif
		
			//uint nodeAlloc = atomicCounterIncrement(allocOffset);
			//imageStore(order, int(nodeAlloc), uvec4(int(offset) + run + LFB_RAGGED_INTERLEAVE * i));
			//imageStore(order, int(nodeAlloc*2), uvec4(fragIndex));
			//imageStore(order, int(nodeAlloc*2+1), uvec4(triangleID));
		
			//fragColour = vec4(heat(float(nodeAlloc % 10000) / 10000.0), 1.0);
		}
	}
}
#endif




