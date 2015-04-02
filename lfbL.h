/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef LFB_BRAGGED_H
#define LFB_BRAGGED_H

#include "lfbBase.h"

struct TextureBuffer;

class LFB_L : public LFBBase
{
protected:
	int prefixSumsSize; //next power of two of totalPixels
	int prefixSumsHeight; //for the blend texture

public:
	TextureBuffer* offsets;
	TextureBuffer* data;
	TextureBuffer* ids;
protected:
	
	
	//blending requires swapping the current FBO in the first pass
	int backupFBOHandle;
	void backupFBO();
	void restoreFBO();
	
	void initBlending();
	void copyBlendResult();
	
	bool countUsingBlending;
	bool globalSort;
	
	unsigned int blendFBO;
	unsigned int blendTex;
	
	virtual bool _resize(vec2i size);
public:
	LFB_L();
	void useBlending(bool enable);
	void useGlobalSort(bool enable);
	virtual ~LFB_L();
	virtual void setDefines(Shader& program);
	virtual bool setUniforms(Shader& program, std::string suffix);
	virtual bool begin();
	virtual bool count();
	virtual int end();
	virtual std::string getName();
	virtual bool getDepthHistogram(std::vector<unsigned int>& histogram);
	virtual bool save(std::string filename) const;
};

#endif

