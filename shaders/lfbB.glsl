/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#define LFB_TYPE_BASIC 1

#define LFB_BASC_INTERLEAVE 50

struct LFBInfo
{
	ivec2 size; //dimensions for hash
	int dataDepth; //number of layers
};
struct LFBTmp
{
	int fragCount;
	int fragIndex;
	int fragsOffset;
	int i;
};

#define LFB_TMP_CONSTRUCTOR LFBTmp(0, 0, -1, -1)

#define COUNTS_TYPE layout(size1x32) coherent uimageBuffer
#define DATA_TYPE layout(LFB_IMAGE_TYPE) imageBuffer

#define LFB_UNIFORMS COUNTS_TYPE counts, DATA_TYPE data

//NOTE: uncommon to set layout() after uniform, but seems to work
#define LFB_DEC(suffix) \
uniform COUNTS_TYPE counts##suffix; \
uniform DATA_TYPE data##suffix; \
uniform LFBInfo lfbInfo##suffix; \
LFBTmp lfbTmp##suffix = LFB_TMP_CONSTRUCTOR;

#define addFragment(suffix, hash, dataVec) _addFragment(lfbInfo##suffix, lfbTmp##suffix, counts##suffix, data##suffix, hash, dataVec)

//add a single fragment (vec3 data, float depth) to the lfb at position fragIndex (use lfbHash to get an index from 0 to 1 coord)
//which order to index 3D fragment data (slabs of layers or blocks of pixel lists)
//generally faster to keep pixel data together (eg blocks of pixel lists)
#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE dat)
{
	if (fragIndex > 0 && fragIndex < info.size.x * info.size.y)
	{
		uint pixelCounter = imageAtomicAdd(counts, fragIndex, 1U);
		if (pixelCounter < info.dataDepth)
		{
			//uint globalOffset = (fragIndex / LFB_BASC_INTERLEAVE) * info.dataDepth * LFB_BASC_INTERLEAVE + fragIndex % LFB_BASC_INTERLEAVE + pixelCounter * LFB_BASC_INTERLEAVE;
			uint globalOffset = fragIndex * info.dataDepth + pixelCounter;
			imageStore(data, int(globalOffset), vec4(dat LFB_FRAG_PAD));
		}
	}
}
#endif

#define LFB_INIT(suffix, index) \
lfbTmp##suffix.fragIndex = (index); \
lfbTmp##suffix.fragCount = int(imageLoad(counts##suffix, lfbTmp##suffix.fragIndex).r); \
lfbTmp##suffix.fragCount = min(lfbTmp##suffix.fragCount, min(MAX_FRAGS, lfbInfo##suffix.dataDepth)); \
lfbTmp##suffix.fragsOffset = lfbTmp##suffix.fragIndex * lfbInfo##suffix.dataDepth;

#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount
#define LFB_COUNT_AT(suffix, index) \
	int(imageLoad(counts##suffix, (index)).r)

#define LFB_LOAD(suffix, index) LFB_FRAG_TYPE(imageLoad(data##suffix, lfbTmp##suffix.fragsOffset + (index)))
#define LFB_ITER_BEGIN(suffix) lfbTmp##suffix.i = 0
#define LFB_ITER_CONDITION(suffix) (lfbTmp##suffix.i < lfbTmp##suffix.fragCount)
#define LFB_ITER_INC(suffix) ++lfbTmp##suffix.i
#define LFB_GET(suffix) LFB_LOAD(suffix, lfbTmp##suffix.fragCount-1-lfbTmp##suffix.i)

