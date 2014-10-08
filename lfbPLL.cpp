/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#include <assert.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <map>
#include <set>

#include <pyarlib/gpu.h>

#include "lfbPLL.h"
#include "extraglenums.h"

EMBED(lfbPLLGLSL, shaders/lfbPLL.glsl);

LFB_PLL::LFB_PLL()
{
	//all LFBBase shaders are embedded. when the first LFBBase is created, the embedded data is given to the shader compiler
	static bool loadEmbed = false;
	if (!loadEmbed)
	{
		Shader::include("lfbPLL.glsl", RESOURCE(lfbPLLGLSL));
		loadEmbed = true;
	}
	
	semaphores = new TextureBuffer(GL_R32UI);
	counts = new TextureBuffer(GL_R32UI);
	pageSize = 4;
}
LFB_PLL::~LFB_PLL()
{
	delete semaphores;
	delete counts;
}
bool LFB_PLL::_resize(vec2i size)
{
	LFB_LL::_resize(size);
	
	semaphores->resize(sizeof(unsigned int) * totalPixels);
	counts->resize(sizeof(unsigned int) * totalPixels);
	
	memory["Semaphores"] = semaphores->size();
	memory["Counts"] = counts->size();
	
	zeroBuffer(semaphores);
	
	return true;
}
bool LFB_PLL::resizePool(int allocs)
{
	//NOTE: we don't have access to the exact number of fragments
	//      totalFragments will commonly be higher than the true number
	return LFB_LL::resizePool(allocs * pageSize);
}
void LFB_PLL::initBuffers()
{
	LFB_LL::initBuffers();
	zeroBuffer(counts);
	//don't need to zero semaphores each render
}
void LFB_PLL::setDefines(Shader& program)
{
	LFBBase::setDefines(program);
	program.define("LFB_PAGE_SIZE", intToString(pageSize));
	program.define("LFB_METHOD_H", "lfbPLL.glsl");
}
bool LFB_PLL::setUniforms(Shader& program, std::string suffix)
{
	if (!LFB_LL::setUniforms(program, suffix))
		return false;

	if (!semaphores->object || !counts->object)
		return false;
		
	std::string semaphoresName = "semaphores" + suffix;
	std::string countsName = "counts" + suffix;
	
	std::string infoStructName = "lfbInfo";
	glUniform1i(glGetUniformLocation(program, (infoStructName + suffix + ".pageSize").c_str()), pageSize);
	
	bool writing = state!=DRAWING;
	
	//linked lists already uses bind points 0, 1, 2
	if (writing)
		//semaphores->bind(program.unique("image", semaphoresName), semaphoresName.c_str(), program, true, true);
		program.set(semaphoresName, *semaphores);
	//counts->bind(program.unique("image", countsName), countsName.c_str(), program, true, writing);
	program.set(countsName, *counts);

	return true;
}
std::string LFB_PLL::getName()
{
	return "LFBBase Pages";
}

bool LFB_PLL::getDepthHistogram(std::vector<unsigned int>& histogram)
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

