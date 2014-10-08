/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#include <assert.h>
#include <stdio.h>

#include <string>
#include <map>
#include <vector>
#include <set>
#include <ostream>
#include <fstream>

#include <pyarlib/includegl.h>
#include <pyarlib/shaderutil.h>
#include <pyarlib/shader.h>
#include <pyarlib/util.h>
#include <pyarlib/gpu.h>

#include "prefixSums.h"

static Shader shaderPSums("prefixSums");
static Shader shaderPSumsM("prefixSumsM");

static TextureBuffer* justOneInt = NULL;

static int doPrefixSums(TextureBuffer* data, bool inPlace, int offset, int count)
{
	//read and check data's info
	assert(data->format == GL_R32UI);
	int sizeInBytes = data->size();
	int sizeInInts = sizeInBytes / sizeof(unsigned int);
	int sumsSize = 1 << ilog2(sizeInInts);
	
	if (inPlace && sizeInInts != sumsSize + 1)
		printf("Warning: strange prefix sums size for inPlace\n");
	
	if (!inPlace)
	{
		assert(sizeInInts >= offset + count * 2 - 1);
		if (count != 1 << ilog2(count))
			printf("Warning: strange prefix sums size\n");
	}
	
	int positionOfTotal;
	if (inPlace)
		positionOfTotal = (sumsSize-1);
	else
		positionOfTotal = offset + count * 2 - 2;
	
	//everything is done in the vertex shader. ignore fragments
	glEnable(GL_RASTERIZER_DISCARD);
	
	//first sums pass - "up-sweep"
	if (inPlace)
	{
		shaderPSums.use();
		//data->bind(0, "sums", shaderPSums, true, true);
		shaderPSums.set("sums", *data);
		shaderPSums.set("pass", 0);
		for (int step = 2; step <= sumsSize; step <<= 1)
		{
			shaderPSums.set("stepSize", step);
			int kernelSize = sumsSize/step;
			glDrawArrays(GL_POINTS, 0, kernelSize);
			glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
		}
	}
	else
	{
		if (!shaderPSumsM.use())
			exit(0);
		//data->bind(0, "sums", shaderPSumsM, true, true);
		shaderPSumsM.set("sums", *data);
		shaderPSumsM.set("pass", 0);
		int o = offset;
		int size = count;
		while (size > 1)
		{
			//printf("sum %i+%i => %i+%i\n", o, size, o+size, size/2);
			shaderPSumsM.set("offsetRead", o);
			shaderPSumsM.set("offsetWrite", o + size);
			glDrawArrays(GL_POINTS, 0, size / 2);
			glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
			o += size;
			size >>= 1;
	
			//printf("+%i ", o);
			//data->printRange(o, mymin(size, 50));
		}
	}
	
	//the bit that's "slow"
	//could be caused by sync/blocking issues or the entire buffer is being mapped (less likely)
	//as a workaround, "justOneInt" is used as a temporary
	#if 0
	glBindBuffer(GL_TEXTURE_BUFFER, *data);
	CHECKERROR;
	//unsigned int* sums = (unsigned int*)glMapBufferRange(GL_TEXTURE_BUFFER, (sumsSize-1)*sizeof(unsigned int), sizeof(unsigned int), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
	unsigned int* sums = (unsigned int*)glMapBufferRange(GL_TEXTURE_BUFFER, 0, sumsSize*sizeof(unsigned int), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
	CHECKERROR;

	//int totalFragments = sums[0];

	//sums[0] = 0; //this line would replace the glBufferSubData block below
	glUnmapBuffer(GL_TEXTURE_BUFFER);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	CHECKERROR;
	#endif
	
	//save the total sum
	if (!justOneInt)
	{
		//glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
		justOneInt = new TextureBuffer(GL_R32UI);
		justOneInt->resize(sizeof(unsigned int));
	}
	//FIXME: this is still reallly stupidly slow
	data->copy(justOneInt, positionOfTotal*sizeof(unsigned int), 0, sizeof(unsigned int));
	
	//need to keep the total sum at the end. used so the counts can be computed based on the deltas
	if (sizeInInts > positionOfTotal + 1)
	{
		data->copy(data, positionOfTotal*sizeof(unsigned int), (positionOfTotal+1)*sizeof(unsigned int), sizeof(unsigned int));
	}
	else
		printf("can't get total\n");
	
	//printf("Getting %i\n", positionOfTotal);

	//write zero to the very last position
	glBindBuffer(GL_TEXTURE_BUFFER, *data); //the * operator dereferences data which then returns its bufferObject
	unsigned int zero = 0;
	glBufferSubData(
		GL_TEXTURE_BUFFER,
		positionOfTotal*sizeof(unsigned int),
		sizeof(unsigned int),
		&zero);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	
	//second sums pass - "down-sweep"
	if (inPlace)
	{
		shaderPSums.set("pass", 1);
		for (int step = sumsSize; step >= 2; step >>= 1)
		{
			shaderPSums.set("stepSize", step);
			int kernelSize = sumsSize/step;
			glDrawArrays(GL_POINTS, 0, kernelSize);
			glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
		}
		shaderPSums.unuse();
	}
	else
	{
		//data->bind(0, "sums", shaderPSumsM, true, true);
		shaderPSumsM.set("pass", 1);
		int o = offset + count * 2 - 2;
		int size = 1;
		while (size < count)
		{
			size <<= 1;
			o -= size;
			//printf("unsum %i+%i => %i+%i\n", o, size, o+size, size/2);
			shaderPSumsM.set("offsetRead", o);
			shaderPSumsM.set("offsetWrite", o + size);
			glDrawArrays(GL_POINTS, 0, size / 2);
			glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	
			//printf("+%i ", o);
			//data->printRange(o, mymin(size, 50));
		}
		shaderPSumsM.unuse();
	}
	
	//read the saved total sum
	unsigned int prefixSumsTotal;
	prefixSumsTotal = *(unsigned int*)justOneInt->map(true, false);
	justOneInt->unmap();
	CHECKERROR;
	
	//data->printRange(offset, 50);
	
	//printf("Total = %i\n", prefixSumsTotal);

	//cleanup and return total sum	
	glDisable(GL_RASTERIZER_DISCARD);
	return prefixSumsTotal;		
}

unsigned int prefixSumsMip(TextureBuffer* data, int offset, int count)
{
	return doPrefixSums(data, false, offset, count);
}

unsigned int prefixSumsInPlace(TextureBuffer* data)
{
	int numInts = data->size() / sizeof(unsigned int) - 1;
	if (numInts < 1)
		return 0;
	return doPrefixSums(data, true, 0, numInts);
}
