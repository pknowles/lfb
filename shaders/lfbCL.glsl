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

#define LFB_TMP_CONSTRUCTOR LFBTmp(0, 0, 0, 0, -1)

#if LFB_READONLY
#define OFFSETS_TYPE layout(size1x32) readonly uimageBuffer
#define DATA_TYPE layout(r32f) readonly imageBuffer
#else
#define OFFSETS_TYPE layout(size1x32) coherent uimageBuffer
#define DATA_TYPE layout(r32f) imageBuffer
#endif

#define COUNTS_TYPE layout(size1x32) coherent uimageBuffer

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
	int baseOffset = int(imageLoad(offsets##suffix, lfbInfo##suffix.interleaveOffset + base).r); \
	baseOffset *= LFB_RAGGED_INTERLEAVE; \
	lfbTmp##suffix.fragOffset = baseOffset * LFB_FRAG_SIZE + run; \
	lfbTmp##suffix.fragCount = int(imageLoad(counts##suffix, lfbTmp##suffix.fragIndex).r); \
}

#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount

#define LFB_COUNT_AT(suffix, index) \
	int(imageLoad(counts##suffix, (index)).r)


#if LFB_FRAG_SIZE == 1
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)).x \
	)
#elif LFB_FRAG_SIZE == 2
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)).x, \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+1)).x \
	)
#elif LFB_FRAG_SIZE == 3
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)).x, \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+1)).x, \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+2)).x \
	)
#elif LFB_FRAG_SIZE == 4
#define LFB_LOAD(suffix, index) \
	LFB_FRAG_TYPE( \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+0)).x, \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+1)).x, \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+2)).x, \
		imageLoad(data##suffix, lfbTmp##suffix.fragOffset + LFB_RAGGED_INTERLEAVE * ((index)*LFB_FRAG_SIZE+3)).x \
	)
#endif

#define LFB_ITER_BEGIN(suffix) lfbTmp##suffix.i = 0
#define LFB_ITER_CONDITION(suffix) lfbTmp##suffix.i < lfbTmp##suffix.fragCount
#define LFB_ITER_INC(suffix) ++lfbTmp##suffix.i
#define LFB_GET(suffix) LFB_LOAD(suffix, (lfbTmp##suffix.fragCount-1-lfbTmp##suffix.i))

#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE fragDat)
{
	int i = int(imageAtomicAdd(counts, fragIndex, 1U));

	if (info.depthOnly == 0)
	{
		int run = fragIndex % LFB_RAGGED_INTERLEAVE;
		int base = (fragIndex / LFB_RAGGED_INTERLEAVE);
	
		uint offset = imageLoad(offsets, info.interleaveOffset + base).r;
		//uint maxCount = imageLoad(offsets, info.interleaveOffset + base + 1).r - offset;
		offset *= LFB_RAGGED_INTERLEAVE;
	
		//if (i > maxCount)
		{
			//imageStore(outOfMemory, 0, uvec4(1U));
			//fragColour = vec4(1,0,0,1);
		}
		//else
		{
			imageStore(data, int(offset)*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+0), vec4(fragDat.x,0,0,0));
			#if LFB_FRAG_SIZE > 1
			imageStore(data, int(offset)*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+1), vec4(fragDat.y,0,0,0));
			#endif
			#if LFB_FRAG_SIZE > 2
			imageStore(data, int(offset)*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+2), vec4(fragDat.z,0,0,0));
			#endif
			#if LFB_FRAG_SIZE > 3
			imageStore(data, int(offset)*LFB_FRAG_SIZE + run + LFB_RAGGED_INTERLEAVE * (i*LFB_FRAG_SIZE+3), vec4(fragDat.w,0,0,0));
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




