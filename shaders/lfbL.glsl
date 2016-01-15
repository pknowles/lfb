/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

struct LFBInfo
{
	ivec2 size; //dimensions for hash
	int depthOnly; //1 for first pass, don't actually add fragment
};
struct LFBTmp
{
	int fragCount;
	int fragOffset;
	int fragIndex;
	int i;
};

//the order in which the linearized lfb is accessed
//can be reversed to make it the same as the linked list lfb
//#define LFB_L_REVERSE 1

#define LFB_TMP_CONSTRUCTOR LFBTmp(0, 0, 0, -1)

#define OFFSETS_TYPE LFB_EXPOSE_TABLE_COHERENT
#define DATA_TYPE LFB_EXPOSE_DATA

#define LFB_UNIFORMS in OFFSETS_TYPE offsets, DATA_TYPE data

//NOTE: uncommon to set layout() after uniform, but seems to work
#define LFB_DEC(suffix) \
uniform OFFSETS_TYPE offsets##suffix; \
uniform DATA_TYPE data##suffix; \
uniform LFBInfo lfbInfo##suffix; \
LFBTmp lfbTmp##suffix = LFB_TMP_CONSTRUCTOR;

#define lfbHash(suffix, coord) _lfbHash(lfbInfo##suffix, coord)
#define addFragment(suffix, hash, dat) _addFragment(lfbInfo##suffix, lfbTmp##suffix, offsets##suffix, data##suffix, hash, dat)
#define loadFragments(suffix, hash, fragslist) _loadFragments(lfbInfo##suffix, lfbTmp##suffix, offsets##suffix, data##suffix, hash, fragslist)
#define writeFragments(suffix, fragslist) _writeFragments(lfbInfo##suffix, lfbTmp##suffix, offsets##suffix, data##suffix, fragslist)
#define iterFragments(suffix, hash, fragOut) _iterFragments(lfbInfo##suffix, lfbTmp##suffix, offsets##suffix, data##suffix, hash, fragOut)

#define LFB_INIT(suffix, index) \
lfbTmp##suffix.fragIndex = (index); \
lfbTmp##suffix.fragOffset = 0; \
if (lfbTmp##suffix.fragIndex > 0) \
	lfbTmp##suffix.fragOffset = LFB_EXPOSE_TABLE_GET(offsets##suffix, lfbTmp##suffix.fragIndex - 1); \
	lfbTmp##suffix.fragCount = LFB_EXPOSE_TABLE_GET(offsets##suffix, lfbTmp##suffix.fragIndex) - lfbTmp##suffix.fragOffset; \
	lfbTmp##suffix.fragCount = min(lfbTmp##suffix.fragCount, MAX_FRAGS);

#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount

#define LFB_COUNT_AT(suffix, index) ( \
	LFB_EXPOSE_TABLE_GET(offsets##suffix, (index)) \
		- ((index) > 0 ? LFB_EXPOSE_TABLE_GET(offsets##suffix, (index) - 1) : 0) \
	)

#define LFB_LOAD(suffix, index) \
	LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.fragOffset + (index))
	
#define LFB_STORE(suffix, index, dat) \
	LFB_EXPOSE_DATA_SET(data##suffix, lfbTmp##suffix.fragOffset + (index), dat)

#define LFB_ITER_BEGIN(suffix) lfbTmp##suffix.i = 0
#define LFB_ITER_CONDITION(suffix) (lfbTmp##suffix.i < lfbTmp##suffix.fragCount)
#define LFB_ITER_INC(suffix) ++lfbTmp##suffix.i

#if LFB_L_REVERSE == 1
#define LFB_GET(suffix) LFB_LOAD(suffix, lfbTmp##suffix.fragCount-1-lfbTmp##suffix.i)
#define LFB_SET(suffix, dat) LFB_STORE(suffix, lfbTmp##suffix.fragCount-1-lfbTmp##suffix.i, dat)
#else
#define LFB_GET(suffix) LFB_LOAD(suffix, lfbTmp##suffix.i)
#define LFB_SET(suffix, dat) LFB_STORE(suffix, lfbTmp##suffix.i, dat)
#endif

#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE dat)
{
	int globalOffset = LFB_EXPOSE_TABLE_ADD(offsets, fragIndex, 1U);
	
	if (info.depthOnly == 0)
		LFB_EXPOSE_DATA_SET(data, globalOffset, dat);
}
#endif





