/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef LFB_LINKED_H
#define LFB_LINKED_H

#define LFB_UNDERALLOCATE 0.5 //will reallocate when fragments falls below this amount allocated. must be < 1
#define LFB_OVERALLOCATE 1.2 //will allocate this amount of fragments when reallocating. must be >= 1

#include "lfbBase.h"

struct GPUBuffer;
struct TextureBuffer;

class LFB_LL : public LFBBase
{
protected:
public: //for cuda testing
	GPUBuffer* alloc; //data for the atomic counter
	TextureBuffer* headPtrs; //per-pixel head pointers
	TextureBuffer* nextPtrs; //per-fragment next pointers
	TextureBuffer* data; //fragment data
	TextureBuffer* counts;
	virtual bool resizePool(size_t allocs); //resizes data/nextPtrs. useful virtual for linked pages
	virtual void initBuffers(); //zeroes buffers - is potentially called from count() as well as begin()
	virtual bool _resize(vec2i size);
	bool lfbNeedsCounts; //used by LFB_PLL
public:
	LFB_LL();
	virtual ~LFB_LL();
	bool forceAlloc(size_t size);
	virtual void setDefines(Shader& program);
	virtual bool setUniforms(Shader& program, std::string suffix);
	virtual bool begin();
	virtual bool count();
	virtual size_t end();
	virtual std::string getName();
	virtual bool getDepthHistogram(std::vector<unsigned int>& histogram);
};

#endif
