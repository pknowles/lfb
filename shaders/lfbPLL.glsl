/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#define LFB_PAGE_SIZE application_should_define_page_size

struct LFBInfo
{
	ivec2 size; //dimensions for hash
	int fragAlloc;
};
struct LFBTmp
{
	int fragCount;
	int fragIndex;
	
	//page pointer
	int node;
	
	//i iterates over page offsets (backwards, starting at fragCount - 1)
	//mod LFB_PAGE_SIZE gives sub page index
	int i;
	
	//if we have hit MAX_FRAGS, we still need to keep the offsets intact (head points to the last page which may not be full)
	//i % LFB_PAGE_SIZE (the offset) cannot change so instead, drop fragments from the list tail using max(0, lfbTmp##suffix.fragCount - MAX_FRAGS)
	int imin;
};

//NOTE: Never re-render if LinkedLFB::count() returns false

#define LFB_TMP_CONSTRUCTOR LFBTmp(0, 0, -1, 0, 0)

//I have no idea how to deal with hard coded layout(binding = n) atomic counters. for the moment this is simply not supported!
#define DECLARE_ALLOC_COUNTER(name) layout(binding = 0, offset = 0) uniform atomic_uint allocOffset;
#define HEAD_TYPE LFB_EXPOSE_TABLE_COHERENT
#define NEXT_TYPE LFB_EXPOSE_TABLE
#define DATA_TYPE LFB_EXPOSE_DATA
#define COUNT_TYPE LFB_EXPOSE_TABLE_COHERENT
#define SEMAPHORE_TYPE LFB_EXPOSE_TABLE_COHERENT

#define LFB_UNIFORMS in HEAD_TYPE headPtrs, NEXT_TYPE nextPtrs, DATA_TYPE data, COUNT_TYPE counts, SEMAPHORE_TYPE semaphores

//NOTE: uncommon to set layout() after uniform, but seems to work
#define LFB_DEC(suffix) \
DECLARE_ALLOC_COUNTER(allocOffset##suffix); \
uniform HEAD_TYPE headPtrs##suffix; \
uniform NEXT_TYPE nextPtrs##suffix; \
uniform DATA_TYPE data##suffix; \
uniform COUNT_TYPE counts##suffix; \
uniform SEMAPHORE_TYPE semaphores##suffix; \
uniform LFBInfo lfbInfo##suffix; \
LFBTmp lfbTmp##suffix = LFB_TMP_CONSTRUCTOR;

#define addFragment(suffix, hash, frag) _addFragment(lfbInfo##suffix, lfbTmp##suffix, allocOffset, headPtrs##suffix, nextPtrs##suffix, data##suffix, counts##suffix, semaphores##suffix, hash, frag)

/*
#define lfbHash(suffix, coord) _lfbHash(lfbInfo##suffix, coord)
#define loadFragments(suffix, hash, fragslist) _loadFragments(lfbInfo##suffix, lfbTmp##suffix, headPtrs##suffix, nextPtrs##suffix, data##suffix, counts##suffix, hash, fragslist)
#define writeFragments(suffix, fragslist) _writeFragments(lfbInfo##suffix, lfbTmp##suffix, headPtrs##suffix, nextPtrs##suffix, data##suffix, counts##suffix, fragslist)
#define iterFragments(suffix, hash, fragOut) _iterFragments(lfbInfo##suffix, lfbTmp##suffix, headPtrs##suffix, nextPtrs##suffix, data##suffix, counts##suffix, hash, fragOut)


//converts a 0 to 1 coord (similar to tex coord) to lfb index
int _lfbHash(LFBInfo info, vec2 coord)
{
	ivec2 ipos = ivec2(coord * info.size);
	return ipos.y * info.size.x + ipos.x;
}
*/

#define LFB_INIT(suffix, index) \
	lfbTmp##suffix.fragIndex = (index); \
	lfbTmp##suffix.fragCount = LFB_EXPOSE_TABLE_GET(counts##suffix, lfbTmp##suffix.fragIndex); \
	lfbTmp##suffix.imin = max(0, lfbTmp##suffix.fragCount - MAX_FRAGS);
#define LFB_COUNT(suffix) lfbTmp##suffix.fragCount
#define LFB_COUNT_AT(suffix, index) \
	LFB_EXPOSE_TABLE_GET(counts##suffix, index)


#define LFB_ITER_BEGIN(suffix) \
	{ \
		lfbTmp##suffix.node = LFB_EXPOSE_TABLE_GET(headPtrs##suffix, lfbTmp##suffix.fragIndex); \
		lfbTmp##suffix.i = lfbTmp##suffix.fragCount - 1; \
	}
	
#define LFB_ITER_CONDITION(suffix) (lfbTmp##suffix.node != 0 && lfbTmp##suffix.i >= lfbTmp##suffix.imin)
#define LFB_ITER_INC(suffix) \
	lfbTmp##suffix.node = (lfbTmp##suffix.i-- % LFB_PAGE_SIZE == 0) ? LFB_EXPOSE_TABLE_GET(nextPtrs##suffix, lfbTmp##suffix.node) : lfbTmp##suffix.node
#define LFB_GET(suffix) \
	LFB_EXPOSE_DATA_GET(data##suffix, lfbTmp##suffix.node * LFB_PAGE_SIZE + (lfbTmp##suffix.i % LFB_PAGE_SIZE))
#define LFB_SET(suffix, frag) \
	LFB_EXPOSE_DATA_SET(data##suffix, lfbTmp##suffix.node * LFB_PAGE_SIZE + (lfbTmp##suffix.i % LFB_PAGE_SIZE), frag)

#if !LFB_READONLY
void _addFragment(LFBInfo info, inout LFBTmp tmp, atomic_uint allocOffset, LFB_UNIFORMS, int fragIndex, LFB_FRAG_TYPE frag)
{
	uint curPage = 0;
	uint pageOffset;
	uint pageIndex;
	int canary = 1000;
	bool waiting = true;
	while (waiting && canary --> 0)
	{
		//acquire semaphore
		int lock = LFB_EXPOSE_TABLE_EXCHANGE(semaphores, fragIndex, 1U);
		if (lock != 1)
		{
			curPage = LFB_EXPOSE_TABLE_GET(headPtrs, fragIndex);

			//get number of fragments at this pixel
			//ran into stupid errors with imageLoad here. should be fine with
			//memoryBarrier() but it looks like it's not working (yet?)
			//pageIndex = imageLoad(counts, fragIndex).r;
			pageIndex = LFB_EXPOSE_TABLE_ADD(counts, fragIndex, 1U); //the alternative

			//get the current page to write to or create one
			pageOffset = pageIndex % LFB_PAGE_SIZE;

			//create a new page if needed
			if (pageOffset == 0)
			{
				int nodeAlloc = int(atomicCounterIncrement(allocOffset));
				if (nodeAlloc * LFB_PAGE_SIZE < info.fragAlloc)
				{
					LFB_EXPOSE_TABLE_SET(nextPtrs, nodeAlloc, curPage);
					LFB_EXPOSE_TABLE_SET(headPtrs, fragIndex, nodeAlloc);
					curPage = nodeAlloc;
				}
				else
					curPage = 0;
			}

			//since imageAtomicAdd is used earlier, this is not needed
			//if (curPage > 0)
			//	imageStore(counts, fragIndex, uvec4(pageIndex + 1));
			//memoryBarrier();

			//release semaphore
			//LFB_EXPOSE_TABLE_SET(semaphores, fragIndex, 0U);
			LFB_EXPOSE_TABLE_EXCHANGE(semaphores, fragIndex, 0U);
			waiting = false;
		}
	}

	//write data to the page
	if (curPage > 0)
		LFB_EXPOSE_DATA_SET(data, int(curPage * LFB_PAGE_SIZE + pageOffset), frag);
}
#endif

/*
bool _iterFragments(in LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, in int fragIndex, out vec4 frag)
{
	if (tmp.node == -1 || tmp.fragIndex != fragIndex) //pre-iteration ID
	{
		tmp.fragIndex = fragIndex;
		
		//need frag count as the head page may not be full
		tmp.fragCount = int(imageLoad(counts, tmp.fragIndex).r);
		
		//i iterates over page offsets
		tmp.i = tmp.fragCount - 1;
	
		tmp.node = int(imageLoad(headPtrs, tmp.fragIndex).r);
	}
	
	//if we have hit MAX_FRAGS, we still need to keep the offsets intact (head points to the tail page which may not be full)
	//i % LFB_PAGE_SIZE (the offset) cannot change so instead, drop fragments from the list tail using imin
	int imin = max(0, tmp.fragCount - MAX_FRAGS);
	
	if (tmp.node == 0 || tmp.i < imin)
	{
		tmp.node = -1; //end. reset for another iteration
		return false;
	}
	else
	{
		int offset = tmp.i % LFB_PAGE_SIZE;
		frag = imageLoad(data, tmp.node * LFB_PAGE_SIZE + offset);
		if (offset == 0) //hit the end of the page, grab the next one
			tmp.node = int(imageLoad(nextPtrs, tmp.node).r);
		--tmp.i;
	}
	return true;
}

int _loadFragments(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, int fragIndex, inout vec4 frags[MAX_FRAGS])
{
	//need frag count as the head page may not be full
	tmp.fragCount = int(imageLoad(counts, fragIndex).r);
	
	//i iterates over page offsets
	int i = tmp.fragCount;
	
	//if we have hit MAX_FRAGS, we still need to keep the offsets intact (head points to the tail page which may not be full)
	//i % LFB_PAGE_SIZE (the offset) cannot change so instead, drop fragments from the list tail using imin
	tmp.fragCount = min(tmp.fragCount, MAX_FRAGS);
	int imin = i - tmp.fragCount;
	
	//grab the head node and start traversing the list
	int f = 0;
	int node = int(imageLoad(headPtrs, fragIndex).r);
	while (i > imin && node > 0)
	{
		--i;
		int offset = i % LFB_PAGE_SIZE;
		frags[i-imin] = imageLoad(data, node * LFB_PAGE_SIZE + offset);
		if (offset == 0) //hit the end of the page, grab the next one
			node = int(imageLoad(nextPtrs, node).r);
	}
	return tmp.fragCount;
}

#if !LFB_READONLY
void _writeFragments(LFBInfo info, inout LFBTmp tmp, LFB_UNIFORMS, in vec4 frags[MAX_FRAGS])
{
	//NOTE: DOES NOT WORK!!!!!!!!!!
	int allocCount = int(imageLoad(counts, tmp.fragIndex).r);
	int i = allocCount;
	allocCount = min(allocCount, tmp.fragCount);
	int imin = i - allocCount;
	int node = int(imageLoad(headPtrs, tmp.fragIndex).r);
	while (i > imin && node != 0)
	{
		--i;
		int offset = i % LFB_PAGE_SIZE;
		imageStore(data, node * LFB_PAGE_SIZE + offset, frags[i-imin]);
		if (offset == 0)
			node = int(imageLoad(nextPtrs, node).r);
	}
}
#endif
*/
