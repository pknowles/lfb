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
	pageSize = 4;
	
	lfbNeedsCounts = true;
}
LFB_PLL::~LFB_PLL()
{
	delete semaphores;
}
bool LFB_PLL::_resize(vec2i size)
{
	LFB_LL::_resize(size);
	
	semaphores->resize(sizeof(unsigned int) * totalPixels);
	
	memory["Semaphores"] = semaphores->size();
	
	zeroBuffer(semaphores); //should only have to do this once
	
	return true;
}
bool LFB_PLL::resizePool(int allocs)
{
	//NOTE: we don't have access to the exact number of fragments
	//      totalFragments will commonly be higher than the true number
	return LFB_LL::resizePool(allocs * pageSize);
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

	if (!semaphores->object)
		return false;
		
	std::string semaphoresName = "semaphores" + suffix;
	
	std::string infoStructName = "lfbInfo";
	glUniform1i(glGetUniformLocation(program, (infoStructName + suffix + ".pageSize").c_str()), pageSize);
	
	bool writing = state!=DRAWING;
	
	int exposeAs = bindless ? Shader::BINDLESS : Shader::IMAGE_UNIT;
	
	//linked lists already uses bind points 0, 1, 2
	if (writing)
		//semaphores->bind(program.unique("image", semaphoresName), semaphoresName.c_str(), program, true, true);
		program.set(exposeAs, semaphoresName, *semaphores);

	return true;
}
std::string LFB_PLL::getName()
{
	return "LFBBase Pages";
}

