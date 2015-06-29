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

#define HEAD_TYPE LFB_EXPOSE_TABLE_COHERENT
#define NEXT_TYPE LFB_EXPOSE_TABLE
#define COUNTS_TYPE LFB_EXPOSE_TABLE
#define DATA_TYPE LFB_EXPOSE_DATA

//I have no idea how to deal with hard coded layout(binding = n) atomic counters. for the moment writing to two LL-LFBs at once is simply not supported!
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
	lfbTmp##suffix.fragCount = LFB_EXPOSE_TABLE_GET(counts##suffix, lfbTmp##suffix.fragIndex);
#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount
#define LFB_COUNT_AT(suffix, index) \
	LFB_EXPOSE_TABLE_GET(counts##suffix, index)
#else
#define LFB_INIT(suffix, index) \
	lfbTmp##suffix.fragIndex = (index);
#endif

#define LFB_ITER_BEGIN(suffix) lfbTmp##suffix.node = LFB_EXPOSE_TABLE_GET(headPtrs##suffix, lfbTmp##suffix.fragIndex)
#define LFB_ITER_CONDITION(suffix) (lfbTmp##suffix.node != 0)
#define LFB_ITER_INC(suffix) lfbTmp##suffix.node = LFB_EXPOSE_TABLE_GET(nextPtrs##suffix, lfbTmp##suffix.node)
#define LFB_GET(suffix) LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.node)
#define LFB_SET(suffix, frag) LFB_EXPOSE_DATA_SET(data##suffix, lfbTmp##suffix.node, frag)

#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE fragData)
{
	int nodeAlloc = int(atomicCounterIncrement(allocOffset));
	if (nodeAlloc < info.fragAlloc) //don't overflow
	{
		int currentHead = LFB_EXPOSE_TABLE_EXCHANGE(headPtrs, fragIndex, nodeAlloc);
		LFB_EXPOSE_TABLE_SET(nextPtrs, nodeAlloc, currentHead);
		LFB_EXPOSE_DATA_SET(data, nodeAlloc, fragData);
		
		#if LFB_REQUIRE_COUNTS
		LFB_EXPOSE_TABLE_ADD(counts, fragIndex, 1U);
		#endif
	}
}
#endif



