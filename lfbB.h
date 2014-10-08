/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef LFB_BASIC_H
#define LFB_BASIC_H

#include "lfbBase.h"

#define LFB_DEFAULT_NUMLAYERS 16

class TextureBuffer;

class LFB_B : public LFBBase
{
protected:
	//a query is used to count the total fragments as this is not provided with the basic LFBBase
	//does not work when drawing to multiple LFBs at once
	//NOTE: does not guarantee immediate result - query may take a few frames to update frag count
	static bool queryLock; //if multiple LFBs are used, don't attempt a second query
	bool performingQuery;
	bool waitingForQuery;
	unsigned int query;
	
	//the Z dimension of the 3D array.
	//maxFrags is simply the size of the shader-local array and is not related to numLayers
	int numLayers;
	
	TextureBuffer* counts;
	TextureBuffer* data;
	
	virtual bool _resize(vec2i size);
	
public:
	LFB_B();
	virtual ~LFB_B();
	virtual void setDefines(Shader& program);
	virtual bool setUniforms(Shader& program, std::string suffix);
	virtual bool begin();
	virtual bool count();
	
	//NOTE: fragment count may be delayed as a query is used
	//(possibly a few frames out of date)
	virtual int end();
	virtual std::string getName();
	
	//attempts to change the allocated number of layers in the 3D array. reduced on memory error
	void setNumLayers(int n);
	virtual bool getDepthHistogram(std::vector<unsigned int>& histogram);
};
#endif
