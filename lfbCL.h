/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef LFB_RAGGED_H
#define LFB_RAGGED_H

#include "lfbBase.h"

class TextureBuffer;

class LFB_CL : public LFBBase
{
protected:
	int prefixSumsPixels; //next power of two of totalPixels
	int prefixSumsSize; //total memory required for prefix sums on totalPixels
	int interleavePixels; //number of pixels to interleave (some memory overhead, faster)
	int interleaveOffset;
	
	TextureBuffer* counts;
	TextureBuffer* offsets;
	TextureBuffer* data;
	
	void preparePack(); //call after counts are known. does prefix sums and allocates memory
	
	virtual bool _resize(vec2i size);
	
	unsigned int blendFBO;
	unsigned int blendTex;
public:
	LFB_CL();
	void useBlending(bool enable);
	void useGlobalSort(bool enable);
	virtual ~LFB_CL();
	virtual void setDefines(Shader& program);
	virtual bool setUniforms(Shader& program, std::string suffix);
	virtual bool begin();
	virtual bool count();
	virtual int end();
	virtual std::string getName();
	virtual bool getDepthHistogram(std::vector<unsigned int>& histogram);
};

#endif

