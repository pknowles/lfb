/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

struct LFBInfo
{
	ivec2 size; //dimensions for hash
	int fragAlloc;
};
struct LFBTmp
{
	int fragCount;
	int fragIndex;
	int node;
};

//NOTE: Never re-render if LinkedLFB::count() returns false

#define LFB_TYPE_LINKED

#define LFB_TMP_CONSTRUCTOR LFBTmp(0, 0, -1)

#if LFB_READONLY
#define DECLARE_ALLOC_COUNTER(name)
#define HEAD_TYPE layout(size1x32) readonly uimageBuffer
#define NEXT_TYPE layout(size1x32) readonly uimageBuffer
#define COUNTS_TYPE layout(size1x32) readonly uimageBuffer
#define DATA_TYPE layout(LFB_IMAGE_TYPE) readonly imageBuffer
#else
#define HEAD_TYPE layout(size1x32) coherent uimageBuffer
#define NEXT_TYPE layout(size1x32) uimageBuffer
#define COUNTS_TYPE layout(size1x32) uimageBuffer
#define DATA_TYPE layout(LFB_IMAGE_TYPE) imageBuffer
#endif

//I have no idea how to deal with hard coded layout(binding = n) atomic counters. for the moment this is simply not supported!
layout(binding = 0, offset = 0) uniform atomic_uint allocOffset;

#define LFB_UNIFORMS in HEAD_TYPE headPtrs, NEXT_TYPE nextPtrs, COUNTS_TYPE counts, DATA_TYPE data

//NOTE: uncommon to set layout() after uniform, but seems to work
#define LFB_DEC(suffix) \
uniform HEAD_TYPE headPtrs##suffix; \
uniform NEXT_TYPE nextPtrs##suffix; \
uniform COUNTS_TYPE counts##suffix; \
uniform DATA_TYPE data##suffix; \
uniform LFBInfo lfbInfo##suffix; \
LFBTmp lfbTmp##suffix = LFB_TMP_CONSTRUCTOR;

#define addFragment(suffix, hash, fragData) _addFragment(lfbInfo##suffix, lfbTmp##suffix, headPtrs##suffix, nextPtrs##suffix, counts##suffix, data##suffix, hash, fragData)

#if LFB_REQUIRE_COUNTS
#define LFB_INIT(suffix, index) \
	lfbTmp##suffix.fragIndex = (index); \
	lfbTmp##suffix.fragCount = int(imageLoad(counts##suffix, lfbTmp##suffix.fragIndex).r);
#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount
#define LFB_COUNT_AT(suffix, index) \
	int(imageLoad(counts##suffix, (index)).r)
#else
#define LFB_INIT(suffix, index) \
	lfbTmp##suffix.fragIndex = (index);
#endif

#define LFB_ITER_BEGIN(suffix) lfbTmp##suffix.node = int(imageLoad(headPtrs##suffix, lfbTmp##suffix.fragIndex).r)
#define LFB_ITER_CONDITION(suffix) (lfbTmp##suffix.node != 0)
#define LFB_ITER_INC(suffix) lfbTmp##suffix.node = int(imageLoad(nextPtrs##suffix, lfbTmp##suffix.node).r)
#define LFB_GET(suffix) LFB_FRAG_TYPE(imageLoad(data##suffix, lfbTmp##suffix.node));
#define LFB_SET(suffix, frag) imageStore(data##suffix, lfbTmp##suffix.node, vec4(frag LFB_FRAG_PAD));

#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE fragData)
{
	uint nodeAlloc = atomicCounterIncrement(allocOffset);
	if (nodeAlloc < info.fragAlloc) //don't overflow
	{
		uint currentHead = imageAtomicExchange(headPtrs, fragIndex, nodeAlloc).r;
		imageStore(nextPtrs, int(nodeAlloc), uvec4(currentHead));
		imageStore(data, int(nodeAlloc), vec4(fragData LFB_FRAG_PAD));
		
		#if LFB_REQUIRE_COUNTS
		imageAtomicAdd(counts, fragIndex, 1U);
		#endif
	}
}
#endif



