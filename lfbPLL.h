/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef LFB_PAGES_H
#define LFB_PAGES_H

#include "lfbLL.h"

struct TextureBuffer;

class LFB_PLL : public LFB_LL
{
protected:
	int pageSize;
	
	//semaphoers are used to stop two shaders allocating the same page
	//in addition, they force other shaders to wait until one shader has finished allocating a page
	//currently shaders do not know whether their page has been allocated until they receive a semaphore lock
	//this could be accomplished if a page index was stored with each page, however the basic linked list method would still be faster
	TextureBuffer* semaphores;
	
	//counts give offsets within each page. after gaining a semaphore lock, the page index is count % pageSize.
	//if this index is zero, the current shader must allocate a new page.
	TextureBuffer* counts;
	
	virtual bool resizePool(int allocs); //a hook to increase allocs to allocs * pageSize
	virtual void initBuffers(); //zeroes counts
	
	virtual bool _resize(vec2i size);
public:
	LFB_PLL();
	int getPageSize() {return pageSize;}
	virtual ~LFB_PLL();
	virtual void setDefines(Shader& program);
	virtual bool setUniforms(Shader& program, std::string suffix);
	//NOTE: LFB_PLL->end() will generally overestimate
	//fragment count as the exact number is unknown
	virtual std::string getName();
	virtual bool getDepthHistogram(std::vector<unsigned int>& histogram);
};

#endif
