/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include <assert.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <map>
#include <set>

#include <pyarlib/gpu.h>

#include "lfbB.h"

EMBED(lfbBGLSL, shaders/lfbB.glsl);

bool LFB_B::queryLock = false;

LFB_B::LFB_B()
{
	//all LFBBase shaders are embedded. when the first LFBBase is created, the embedded data is given to the shader compiler
	static bool loadEmbed = false;
	if (!loadEmbed)
	{
		Shader::include("lfbB.glsl", RESOURCE(lfbBGLSL));
		loadEmbed = true;
	}
	
	numLayers = LFB_DEFAULT_NUMLAYERS;
	counts = new TextureBuffer(GL_R32UI);
	data = new TextureBuffer(lfbDataType);
	query = 0;
	waitingForQuery = false;
	performingQuery = false;
}
LFB_B::~LFB_B()
{
	counts->release();
	data->release();
	delete counts;
	delete data;
	counts = NULL;
	data = NULL;
	if (query)
		glDeleteQueries(1, &query);
}
bool LFB_B::_resize(vec2i size)
{
	bool resized = false;
	
	resized = true;
	counts->resize(sizeof(unsigned int) * totalPixels);
	memory["Counts"] = counts->size();

	allocFragments = totalPixels * numLayers;
	
	data->setFormat(lfbDataType);
	
	//attempt reallocation if needed
	if (data->size() / lfbDataStride != allocFragments)
	{
		resized = true;
		//common for the brute force method to cause out-of-memory
		while (!data->resize(lfbDataStride * allocFragments))
		{
			//if resize failed, halve layers and retry
			numLayers >>= 1;
			printf("Falling back to %i layers\n", numLayers);
			allocFragments = totalPixels * numLayers;
			if (allocFragments == 0)
			{
				//just in case even 1 layer is too much
				//data remains unallocated and setUniforms will return false
				break;
			}
		}
	}
		
	memory["Data"] = data->size();
		
	if (!query)
		glGenQueries(1, &query);
	
	return resized;
}
void LFB_B::setDefines(Shader& program)
{
	LFBBase::setDefines(program);
	program.define("LFB_METHOD_H", "lfbB.glsl");
}
bool LFB_B::setUniforms(Shader& program, std::string suffix)
{
	if (!counts->object || !data->object)
		return false;
	CHECKERROR;

	std::string infoStructName = "lfbInfo" + suffix;
	if (size2D.x > 0)
		glUniform2i(glGetUniformLocation(program, (infoStructName + ".size").c_str()), size2D.x, size2D.y);
	glUniform1i(glGetUniformLocation(program, (infoStructName + ".dataDepth").c_str()), numLayers);
	CHECKERROR;
	
	bool writing = state!=DRAWING;
	
	std::string countsName = "counts" + suffix;
	std::string dataName = "data" + suffix;
	
	//bind(bind point index, name, shader program, bool read, bool write)
	//counts->bind(program.unique("image", countsName), countsName.c_str(), program, true, writing);
	program.set(countsName, *counts);

	//data->bind(program.unique("image", dataName), dataName.c_str(), program, !writing, true);
	program.set(dataName, *data);
	
	CHECKERROR;
	return true;
}
bool LFB_B::begin()
{
	CHECKERROR;
	if (profile) profile->begin();
	
	LFBBase::begin();
	
	//zero lookup tables
	zeroBuffer(counts);
	glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
	
	if (profile) profile->time("Zero");
	
	CHECKERROR;
	//don't start a new query until the result has been read from the current one
	//NOTE: ATI doesn't appear to like a sample query and time query at the same time
	//      so profile->time is not called between begin/end query
	if (!waitingForQuery && !queryLock)
	{
		queryLock = true;
		performingQuery = true;
		glBeginQuery(GL_SAMPLES_PASSED, query);
		CHECKERROR;
	}
	return true; //full render, including colour
}
bool LFB_B::count()
{
	CHECKERROR;
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);

	LFBBase::count();
	
	//end query if one was running
	if (performingQuery)
	{
		glEndQuery(GL_SAMPLES_PASSED);
		performingQuery = false;
		waitingForQuery = true;
		queryLock = false;
	}
	CHECKERROR;

	if (profile) profile->time("Render");
	
	//read query results if available. if not, try again next frame
	if (waitingForQuery)
	{
		int available;
		glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
		if (available)
		{
			glGetQueryObjectuiv(query, GL_QUERY_RESULT, (GLuint*)&totalFragments);
			waitingForQuery = false;
		}
	}
	CHECKERROR;

	if (profile) profile->time("Read Total");
	
	//the maximum depth complexity is unknown so
	//no point resizing or re-rendering
	return false;
}
int LFB_B::end()
{
	LFBBase::end();
	return totalFragments;
}
void LFB_B::setNumLayers(int n)
{
	numLayers = n;
}
std::string LFB_B::getName()
{
	return "BasicLFB";
}

bool LFB_B::getDepthHistogram(std::vector<unsigned int>& histogram)
{
	if (!counts->size())
		return LFBBase::getDepthHistogram(histogram);
	histogram.clear();
	unsigned int* l = (unsigned int*)counts->map(true, false);
	for (int i = 0; i < getTotalPixels(); ++i)
	{
		if (histogram.size() <= l[i])
			histogram.resize(l[i]+1, 0);
		histogram[l[i]]++;
	}
	counts->unmap();
	return true;
}
