/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#include <assert.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <map>
#include <set>

#include <pyarlib/gpu.h>

#include "lfbCL.h"
#include "prefixSums.h"

EMBED(lfbCLGLSL, shaders/lfbCL.glsl);
EMBED(lfbCopyCLVERT, shaders/lfbCopy.vert);
EMBED(lfbAlignCLVERT, shaders/lfbCLAlign.vert);
EMBED(prefixSumsCLVERT, shaders/prefixSums.vert);
EMBED(prefixSumsCLMVERT, shaders/prefixSumsM.vert);

//when blending, this shader copies the blend texture into the TextureBuffer
static Shader shaderCopy("lfbCopy");
static Shader shaderAlign("alignCounts");

LFB_CL::LFB_CL()
{
	interleavePixels = 16;

	//all LFBBase shaders are embedded. when the first LFBBase is created, the embedded data is given to the shader compiler
	static bool loadEmbed = false;
	if (!loadEmbed)
	{
		Shader::include("lfbCL.glsl", RESOURCE(lfbCLGLSL));
		Shader::include("lfbCopy.vert", RESOURCE(lfbCopyCLVERT));
		Shader::include("prefixSums.vert", RESOURCE(prefixSumsCLVERT));
		Shader::include("prefixSumsM.vert", RESOURCE(prefixSumsCLMVERT));
		Shader::include("alignCounts.vert", RESOURCE(lfbAlignCLVERT));
		loadEmbed = true;
	}
	
	prefixSumsSize = 0;
	interleaveOffset = 0;
	allocFragments = 0;
	
	counts = new TextureBuffer(GL_R32UI);
	offsets = new TextureBuffer(GL_R32UI);
	data = new TextureBuffer(GL_R32F); //NOTE: NOT lfbDataType anymore
	
	blendFBO = 0;
	blendTex = 0;
}
LFB_CL::~LFB_CL()
{
	counts->release();
	offsets->release();
	data->release();
	delete counts;
	delete offsets;
	delete data;
	counts = NULL;
	offsets = NULL;
	data = NULL;
	glDeleteFramebuffers(1, &blendFBO);
	glDeleteTextures(1, &blendTex);
}
bool LFB_CL::_resize(vec2i size)
{
	assert(totalPixels > 0);
	
	//the prefix sum algorithm used requires 2^n data
	prefixSumsPixels = nextPowerOf2(totalPixels);
	prefixSumsSize = prefixSumsPixels * 2; // - 1 // need +1 extra int to store the total
	counts->resize(sizeof(unsigned int) * prefixSumsSize);
	offsets->resize(sizeof(unsigned int) * prefixSumsSize);
	
	int totalTiles = ceil(totalPixels, interleavePixels);
	
	//shuffle->resize(sizeof(unsigned int) * totalPixels);
	//strides->resize(sizeof(unsigned int) * totalTiles);
	
	//init buffers to zero. with predicted counts, this isn't needed each frame
	zeroBuffer(offsets, prefixSumsPixels * sizeof(unsigned int));
	zeroBuffer(counts, prefixSumsPixels * sizeof(unsigned int));
	
	//allocFragments = 1; //FIXME: currently need this as I'm using resize(1, 1) as a release() or near-to
	
	//data->setFormat(lfbDataType);
	//data->resize(allocFragments * lfbDataStride);
	//memory["Data"] = data->size();
	
	memory["Counts"] = counts->size();
	memory["Offsets"] = offsets->size();
	
	return true; //needed to resize
}
void LFB_CL::setDefines(Shader& program)
{
	LFBBase::setDefines(program);
	program.define("LFB_METHOD_H", "lfbCL.glsl");
	program.define("LFB_RAGGED_INTERLEAVE", intToString(interleavePixels));
}
bool LFB_CL::setUniforms(Shader& program, std::string suffix)
{
	//printf("Setting uniforms for %s\n", program.name().c_str());
	if (program.error())
		printf("WARNING: program %s failed to compile!!!\n", program.name().c_str());

	if (!offsets->size() || !counts->object)
		return false;
		
	if ((state == SECOND_PASS || state == DRAWING) && !data->object && allocFragments > 0)
		return false;
	
	//writing, depending on the state, determines READ_ONLY, WRITE_ONLY and READ_WRITE TextureBuffer data
	bool writing = state!=DRAWING;

	//TextureBuffer::unbindAll();
	
	std::string countsName = "counts" + suffix;
	std::string offsetsName = "offsets" + suffix;
	std::string dataName = "data" + suffix;

	std::string infoStructName = "lfbInfo";
	if (size2D.x > 0)
		glUniform2i(glGetUniformLocation(program, (infoStructName + suffix + ".size").c_str()), size2D.x, size2D.y);
	glUniform1i(glGetUniformLocation(program, (infoStructName + suffix + ".depthOnly").c_str()), (state==FIRST_PASS?1:0));
	
	//if (state != SORTING)
	{
		//counts->bind(program.unique("image", countsName), countsName.c_str(), program, true, writing);
		//offsets->bind(program.unique("image", offsetsName), offsetsName.c_str(), program, true, writing);
		program.set(countsName, *counts);
		program.set(offsetsName, *offsets);
	}
	if (data->size() > 0)
	{
		//data->bind(program.unique("image", dataName), dataName.c_str(), program, !writing, writing);
		program.set(dataName, *data);
	}
	
	//glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, *alloc);
	//order->bind(program.unique("image", "order"), "order", program, !writing, writing);

	//printf("interleaveOffset %i\n", interleaveOffset);
	glUniform1i(glGetUniformLocation(program, (infoStructName + suffix + ".interleaveOffset").c_str()), interleaveOffset);
	return true;
}
bool LFB_CL::begin()
{
	//printf("begin()\n");
	//mark the start of the frame for profiler averaging
	//if (profile) profile->begin(); //NOTE: removed so user can call this!
	
	//parent begin - may trigger ::resize()
	LFBBase::begin();
	
	//preparePack();
	
	//if (profile) profile->time("Zero");
	
	return false; //depth-only render
}
bool LFB_CL::count()
{
	//printf("count()\n");
	CHECKERROR;
	//finish all the imageAtomicAdds for fragment counting
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	LFBBase::count();
	
	//printf("Counts(%i) ", counts->bufferObject);
	//counts->printRange(0, 50);
	//printf("Offsets(%i) ", offsets->bufferObject);
	//offsets->printRange(0, 50);
	
	if (profile) profile->time("Count");
	
	preparePack();
	
	return true;
}
void LFB_CL::preparePack()
{
	//swap counts and offsets
	std::swap(counts, offsets);
	//printf("Swap counts <-> offsets\n");
		
	//printf("preparePack()\n");
	int readOffset = 0;
	int writeOffset = prefixSumsPixels;
	int mipSize = writeOffset;
	
	//counts->printRange(0, 50);
	//offsets->printRange(0, 50);
	
	assert(isPowerOf2(interleavePixels));
	
	glEnable(GL_RASTERIZER_DISCARD);
	//take max() for runs of interleavePixels pixels
	shaderAlign.use();
	shaderAlign.set("vpSize", size2D);
	
	//offsets->bind(shaderAlign.unique("image", "offsets"), "offsets", shaderAlign, true, true);
	//counts->bind(shaderAlign.unique("image", "tozero"), "tozero", shaderAlign, true, true);
	shaderAlign.set("offsets", *offsets);
	shaderAlign.set("counts", *counts);
	
	shaderAlign.set("pass", 0);
	for (int i = 2; i <= interleavePixels; i <<= 1)
	{
		//printf("%i. maxing %i+%i => %i+%i\n", i, readOffset, writeOffset - readOffset, writeOffset, mipSize/2);
		shaderAlign.set("offsetRead", readOffset);
		shaderAlign.set("offsetWrite", writeOffset);
		glDrawArrays(GL_POINTS, 0, mipSize/2);
		glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
		readOffset = writeOffset;
		mipSize /= 2;
		writeOffset += mipSize;
		
		//printf("+%i ", readOffset);
		//counts->printRange(readOffset, 50);
	}
	shaderAlign.unuse();
	glDisable(GL_RASTERIZER_DISCARD);
	if (profile) profile->time("Align");
	
	interleaveOffset = readOffset; //save this as the final data will start here
	
	//offsets->printRange(interleaveOffset, 50);
	
	//perform parallel prefix sums
	totalFragments = prefixSumsMip(offsets, readOffset, mipSize);
	totalFragments *= interleavePixels; //summing max() of interleavePixels range
	
	//offsets->printRange(interleaveOffset, offsets->size()/sizeof(unsigned int) - interleaveOffset);
	
	//printf("Total Frags = %i\n", totalFragments);
	
	if (profile) profile->time("PSums");
	
	//Counts are not stored as they can be reconstructed
	//as the difference between consecutive offsets. Having the total
	//stored at the end of the offsets saves checking offset array bounds in the shader.
	//However, after rendering they have shifted which means a zero needs to be stored at array index -1 to avoid the if statement
	//The simplest solution was to keep the if statment (very minimal performance overhead)
	/*
	//copy the total into the offset data
	glBindBuffer(GL_TEXTURE_BUFFER, *offsets); //the * foperator dereferences data which then returns its bufferObject
	glBufferSubData(
		GL_TEXTURE_BUFFER,
		(prefixSumsSize)*sizeof(unsigned int),
		sizeof(unsigned int),
		&totalFragments);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	*/
	
	//allocate data
	#if 1
	if (allocFragments > totalFragments * 4.0 || allocFragments < totalFragments)
		allocFragments = totalFragments + totalFragments / 4;
	#else
	allocFragments = totalFragments;
	#endif
	
	if (allocFragments <= 0)
	{
		//printf("ERROR: FRAGS WAS %i\n", allocFragments);
		allocFragments = 32;
	}
	if (allocFragments * lfbDataStride > 1500000000)
	{
		printf("ERROR: FRAGS WAS %i\n", allocFragments);
		allocFragments = 1500000000;
	}
	
	//data->setFormat(lfbDataType); //NO!!! need per-float access level for coalescing large fragments 
	
	data->resize(allocFragments * lfbDataStride);
	if (CHECKERROR)
		printf("%i\n", allocFragments * lfbDataStride);
	memory["Data"] = data->size();
	
	if (profile) profile->time("Alloc");
	
	//zeroBuffer(counts, prefixSumsPixels * sizeof(unsigned int));
	//if (profile) profile->time("Zero Counts");
	
	
	
	//static const int one = 0;
	//glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, *alloc);
	//glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), &one, GL_DYNAMIC_DRAW);
	//glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
	//order->resize(allocFragments * 2 * sizeof(unsigned int));
	//zeroBuffer(order);
}
int LFB_CL::end()
{
	//printf("end()\n");
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	if (profile) profile->time("Render");
	
	state = SORTING;

	//order->printRange(0, 1000);
	//unsigned int* o = (unsigned int*)order->map(true, false);
	//for (int i = 0; i < 1000; ++i)
	{
	//	printf("%i: %i,%i\n", o[i*2+1], o[i*2]%size2D.x, o[i*2]/size2D.x);
	}
	//order->unmap();

	LFBBase::end();
	return totalFragments;
}
std::string LFB_CL::getName()
{
	return "CoalescedLinearizedLFB";
}

bool LFB_CL::getDepthHistogram(std::vector<unsigned int>& histogram)
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

