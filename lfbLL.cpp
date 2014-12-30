/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#include <assert.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <map>
#include <set>

#include <pyarlib/gpu.h>

#include "lfbLL.h"
#include "extraglenums.h"

EMBED(lfbLLGLSL, shaders/lfbLL.glsl);

//this is stupid, but it works
static TextureBuffer* justOneInt = NULL;

LFB_LL::LFB_LL()
{
	//all LFBBase shaders are embedded. when the first LFBBase is created, the embedded data is given to the shader compiler
	static bool loadEmbed = false;
	if (!loadEmbed)
	{
		Shader::include("lfbLL.glsl", RESOURCE(lfbLLGLSL));
		loadEmbed = true;
	}
	
	alloc = new GPUBuffer(GL_ATOMIC_COUNTER_BUFFER);
	headPtrs = new TextureBuffer(GL_R32UI);
	nextPtrs = new TextureBuffer(GL_R32UI);
	counts = new TextureBuffer(GL_R32UI);
	data = new TextureBuffer(lfbDataType);
}
LFB_LL::~LFB_LL()
{
	alloc->release();
	headPtrs->release();
	nextPtrs->release();
	data->release();
	counts->release();
	delete alloc;
	delete headPtrs;
	delete nextPtrs;
	delete data;
	delete counts;
}
bool LFB_LL::_resize(vec2i size)
{
	alloc->resize(sizeof(unsigned int));
	headPtrs->resize(sizeof(unsigned int) * totalPixels);
	
	if (data->setFormat(lfbDataType))
		resizePool(allocFragments); //need to resize as data type, and hence probably size has changed
	
	memory["Counter"] = alloc->size();
	memory["Head Ptrs"] = headPtrs->size();
	
	return true;
}
bool LFB_LL::resizePool(int allocs)
{
	totalFragments = allocs - 1;
	allocs = mymax(allocs, 32);
	
	//only resize if necessary
	//comparing using ->size and lfbDataStride allows for changes in lfbDataStride
	if (allocs * lfbDataStride < data->size() * LFB_UNDERALLOCATE || data->size() < allocs * lfbDataStride)
	{
		//allocate a little more than needed
		allocFragments = (int)(allocs * LFB_OVERALLOCATE);
		nextPtrs->resize(allocFragments * sizeof(unsigned int));
		data->resize(allocFragments * lfbDataStride);
		
		memory["Next Ptrs"] = nextPtrs->size();
		memory["Data"] = data->size();
		
		if (!data->object) //debugging
			printf("Error resizing pool data %ix%i=%.2fMB\n", allocFragments, lfbDataStride, allocFragments * lfbDataStride / 1024.0 / 1024.0);
		return true;
	}
	return false;
}
void LFB_LL::initBuffers()
{
	//index zero is reserved as NULL pointer, so we start the counter at one
	//yes, this means allocating all of 4 extra bytes, but it means zeroBuffer() can be reused
	static const int one = 1;
	
	CHECKERROR;
	alloc->buffer(&one, sizeof(one));
	CHECKERROR;
	
	//zero the head pointers, or rather, set each pointer to NULL
	zeroBuffer(headPtrs);
	
	if (computeCounts)
	{
		counts->resize(sizeof(unsigned int) * totalPixels);
		zeroBuffer(counts);
	}
	else
	{
		if (*counts)
			counts->release();
	}
}
void LFB_LL::setDefines(Shader& program)
{
	LFBBase::setDefines(program);
	program.define("LFB_METHOD_H", "lfbLL.glsl");
	program.define("LFB_REQUIRE_COUNTS", computeCounts);
}
bool LFB_LL::setUniforms(Shader& program, std::string suffix)
{
	if (!alloc->object || !headPtrs->object)
		return false;
	
	if (state != PRE_INIT && (!nextPtrs->object || !data->object) && allocFragments > 0)
		return false;
	
	std::string headName = "headPtrs" + suffix;
	std::string nextName = "nextPtrs" + suffix;
	std::string countsName = "counts" + suffix;
	std::string dataName = "data" + suffix;
	
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, *alloc);
	
	std::string infoStructName = "lfbInfo" + suffix;
	if (size2D.x > 0)
		program.set(infoStructName + ".size", size2D);
		//glUniform2i(glGetUniformLocation(program, ().c_str()), size2D.x, size2D.y);
	program.set(infoStructName + ".fragAlloc", allocFragments);
	//glUniform1i(glGetUniformLocation(program, (infoStructName + ".fragAlloc").c_str()), allocFragments);
	
	//writing, depending on the state, determines READ_ONLY, WRITE_ONLY and READ_WRITE TextureBuffer data
	bool writing = state!=DRAWING;
	
	//int headIndex = program.unique("image", headName);
	//int nextIndex = program.unique("image", nextName);
	//int dataIndex = program.unique("image", dataName);
	//printf("%s: %i %i %i\n", suffix.c_str(), headIndex, nextIndex, dataIndex);
	//headPtrs->bind(headIndex, headName.c_str(), program, true, writing);
	program.set(headName, *headPtrs);
	if (nextPtrs->object)
		program.set(nextName, *nextPtrs);
		//nextPtrs->bind(nextIndex, nextName.c_str(), program, !writing, writing);
	if (data->object)
		program.set(dataName, *data);
		//data->bind(dataIndex, dataName.c_str(), program, !writing, true);
	
	if (*counts)
		program.set(countsName, *counts);
	
	return true;
}
bool LFB_LL::begin()
{
	//mark the start of the frame for profiler averaging
	//if (profile) profile->begin();
	
	//parent begin - may trigger ::resize()
	LFBBase::begin();
	
	//zero alloc counter and head pointers
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	glMemoryBarrierEXT(GL_ATOMIC_COUNTER_BARRIER_BIT_EXT);
	initBuffers();
	glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
	
	if (profile) profile->time("Zero");
	
	return true; //full render, including colour
}
bool LFB_LL::count()
{
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	glMemoryBarrierEXT(GL_ATOMIC_COUNTER_BARRIER_BIT_EXT);
	
	if (profile) profile->time("Render");
	
	LFBBase::count();
	
	//read the alloc atomic counter to find the number of fragments (or pages)
	//if this number is greater than that allocated, a re-render is needed
	#if 0
	//in some strange circumstances (found during the grid tests) this is really slow
	unsigned int* d = (unsigned int*)alloc->map(true, false);
	unsigned int numAllocs = d[0];
	alloc->unmap();
	#else
	//this is stupid but works, somehow avoiding the sync/blocking issue
	if (!justOneInt)
	{
		justOneInt = new TextureBuffer(GL_R32UI);
		justOneInt->resize(alloc->size());
	}
	alloc->copy(justOneInt);
	unsigned int numAllocs = 0;
	unsigned int* d = (unsigned int*)justOneInt->map(true, false);
	if (d)
	{
		numAllocs = d[0];
		justOneInt->unmap();
	}
	else
		printf("Error: Unable to map alloc counter for linked list LFB\n");
	#endif
	
	if (profile) profile->time("Read Total");
	
	//FIXME: if pool is reducing in size a re-render is still done
	if (resizePool(numAllocs))
	{
		printf("Linked List Re-Render\n");
		initBuffers();
		return true; //please do a second pass - we didn't allocate enough
	}
	else
		return false; //render is done, no second pass needed
}
int LFB_LL::end()
{
	LFBBase::end();
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	glMemoryBarrierEXT(GL_ATOMIC_COUNTER_BARRIER_BIT_EXT);
	
	//NOTE: profile will average this time, so for it to become significant many re-renders must be done
	if (profile) profile->time("Re-Render");
	
	return totalFragments; //take one because the element zero is "null"
}
std::string LFB_LL::getName()
{
	return "LinkedListLFB";
}

bool LFB_LL::getDepthHistogram(std::vector<unsigned int>& histogram)
{
	if (!counts->size())
		return LFBBase::getDepthHistogram(histogram);
	histogram.clear();
	unsigned int* l = (unsigned int*)counts->map(true, false);
	assert(l);
	for (int i = 0; i < getTotalPixels(); ++i)
	{
		assert(l[i] < 1024);
		if (histogram.size() <= l[i])
			histogram.resize(l[i]+1, 0);
		histogram[l[i]]++;
	}
	bool ok = counts->unmap();
	assert(ok);
	return true;
}

