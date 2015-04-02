/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#include <assert.h>
#include <stdio.h>
#include <ostream>
#include <fstream>

#include <string>
#include <vector>
#include <map>
#include <set>

#include <zlib.h>

#include <pyarlib/gpu.h>

#include "lfbL.h"
#include "prefixSums.h"

EMBED(lfbLGLSL, shaders/lfbL.glsl);
EMBED(lfbCopyVERT, shaders/lfbCopy.vert);
EMBED(lfbRaggedSortVERT, shaders/lfbRaggedSort.vert);
EMBED(prefixSumsVERT, shaders/prefixSums.vert);

//when blending, this shader copies the blend texture into the TextureBuffer
static Shader shaderCopy("lfbCopy");
static Shader shaderSort("lfbRaggedSort");

void LFB_L::backupFBO()
{
	//the user may have bound an FBO, to render the transparency to a
	//texture. this stores whatever FBO is bound for replacement after blending
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &backupFBOHandle);
}
void LFB_L::restoreFBO()
{
	glBindFramebuffer(GL_FRAMEBUFFER, backupFBOHandle);
}

LFB_L::LFB_L()
{
	//all LFBBase shaders are embedded. when the first LFBBase is created, the embedded data is given to the shader compiler
	static bool loadEmbed = false;
	if (!loadEmbed)
	{
		Shader::include("lfbL.glsl", RESOURCE(lfbLGLSL));
		Shader::include("lfbCopy.vert", RESOURCE(lfbCopyVERT));
		Shader::include("lfbRaggedSort.vert", RESOURCE(lfbRaggedSortVERT));
		Shader::include("prefixSums.vert", RESOURCE(prefixSumsVERT));
		loadEmbed = true;
	}
	
	countUsingBlending = false;
	globalSort = false;
	prefixSumsSize = 0;
	
	offsets = new TextureBuffer(GL_R32UI);
	data = new TextureBuffer(lfbDataType);
	ids = new TextureBuffer(GL_R32UI);
	
	blendFBO = 0;
	blendTex = 0;
}
LFB_L::~LFB_L()
{
	ids->release();
	offsets->release();
	data->release();
	delete ids;
	delete offsets;
	delete data;
	ids = NULL;
	offsets = NULL;
	data = NULL;
	glDeleteFramebuffers(1, &blendFBO);
	glDeleteTextures(1, &blendTex);
}
void LFB_L::useBlending(bool enable)
{
	//check this was called outside rendering
	if (state == FIRST_PASS)
	{
		printf("Error: cannot call LFB_L.useBlending() while rendering\n");
		return;
	}
	else if (enable && totalPixels && size2D.y < 2)
	{
		printf("Error: cannot call useBlending(true) before resize(x, y). Must be 2D!\n");
		countUsingBlending = false;
	}
	else
		countUsingBlending = enable;
	
	//as an alternative to atomicAdd()-ing to counts, blending can be used
	if (totalPixels && countUsingBlending)
	{
		//create FBO and blend texture
		if (!blendFBO)
			glGenFramebuffers(1, &blendFBO);
		if (!blendTex)
			glGenTextures(1, &blendTex);
	
		//set up blendTex
		prefixSumsHeight = size2D.y + ceil(prefixSumsSize - totalPixels, size2D.x);
		assert(prefixSumsHeight * size2D.x >= prefixSumsSize);
		glBindTexture(GL_TEXTURE_2D, blendTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		CHECKERROR;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size2D.x, prefixSumsHeight, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		CHECKERROR;
		glBindTexture(GL_TEXTURE_2D, 0);
		
		memory["Blend"] = size2D.x * prefixSumsHeight * 4;
	
		//attach blendTex to blendFBO	
		CHECKERROR;
		glBindFramebuffer(GL_FRAMEBUFFER, blendFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blendTex, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		CHECKERROR;
	}
	else if (blendFBO || blendTex)
	{
		//free blending data
		glDeleteFramebuffers(1, &blendFBO);
		glDeleteTextures(1, &blendTex);
		blendFBO = 0;
		blendTex = 0;
		memory.erase("Blend");
	}
}
void LFB_L::useGlobalSort(bool enable)
{
	globalSort = enable;
	
	if (!globalSort && ids)
		ids->resize(0);
}
bool LFB_L::_resize(vec2i size)
{
	assert(totalPixels > 0);
	
	//the prefix sum algorithm used requires 2^n data
	prefixSumsSize = nextPowerOf2(totalPixels);
	offsets->resize(sizeof(unsigned int) * (prefixSumsSize + 1));
	
	memory["Offsets"] = offsets->size();

	//a resize of the blend texture is needed
	useBlending(countUsingBlending);
	
	return true; //needed to resize
}
void LFB_L::setDefines(Shader& program)
{
	LFBBase::setDefines(program);
	program.define("COUNT_USING_BLENDING", intToString((int)countUsingBlending));
	program.define("LFB_METHOD_H", "lfbL.glsl");
	program.define("GLOBAL_SORT", intToString((int)globalSort));
}
bool LFB_L::setUniforms(Shader& program, std::string suffix)
{
	if (!offsets->object)
		return false;
		
	if ((state == SECOND_PASS || state == DRAWING) && (!data->object || (globalSort && !ids->object)) && allocFragments > 0)
		return false;
	
	//writing, depending on the state, determines READ_ONLY, WRITE_ONLY and READ_WRITE TextureBuffer data
	bool writing = state!=DRAWING;

	//TextureBuffer::unbindAll();
	
	std::string offsetsName = "offsets" + suffix;
	std::string dataName = "data" + suffix;
	std::string idsName = "fragIDs" + suffix;

	std::string infoStructName = "lfbInfo";
	if (size2D.x > 0)
		glUniform2i(glGetUniformLocation(program, (infoStructName + suffix + ".size").c_str()), size2D.x, size2D.y);
	glUniform1i(glGetUniformLocation(program, (infoStructName + suffix + ".depthOnly").c_str()), (state==FIRST_PASS?1:0));
		
	if (state != SORTING)
		program.set(offsetsName, *offsets);
		//offsets->bind(program.unique("image", offsetsName), offsetsName.c_str(), program, true, writing);
	if (data->size() > 0)
		program.set(dataName, *data);
		//data->bind(program.unique("image", dataName), dataName.c_str(), program, !writing, writing);
	
	if (globalSort)
	{
		if (state == SORTING || state == SECOND_PASS)
			if (ids->size() > 0)
				program.set(idsName, *ids);
				//ids->bind(program.unique("image", idsName), idsName.c_str(), program, true, writing);
	}
	
	return true;
}
void LFB_L::initBlending()
{
	backupFBO();
		
	glBindFramebuffer(GL_FRAMEBUFFER, blendFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blendTex, 0);
	glPushAttrib(GL_COLOR_BUFFER_BIT);
	glClearColor(0,0,0,0);
	//clear the entire offset table
	//(including areas outside the current viewport)
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
}
void LFB_L::copyBlendResult()
{
	//cleanup blending state
	glPopAttrib();
	restoreFBO();

	//copy blending results (the count data) into offsets
	shaderCopy.use();
	glBindTexture(GL_TEXTURE_2D, blendTex);
	shaderCopy.set("blendCount", 0); //set the sampler2D
	//offsets->bind(1, "offsets", shaderCopy, false, true);
	shaderCopy.set("offsets", *offsets);
	shaderCopy.set("width", size2D.x);
	glEnable(GL_RASTERIZER_DISCARD);
	glDrawArrays(GL_POINTS, 0, prefixSumsSize);
	glDisable(GL_RASTERIZER_DISCARD);
	glBindTexture(GL_TEXTURE_2D, 0);
	shaderCopy.unuse();
}
bool LFB_L::begin()
{
	//mark the start of the frame for profiler averaging
	//if (profile) profile->begin();
	
	//parent begin - may trigger ::_resize()
	LFBBase::begin();
	
	//zero lookup tables
	if (!countUsingBlending)
	{
		zeroBuffer(offsets);
		glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
	}
	
	if (profile) profile->time("Zero");
	
	//set up blending FBO
	if (countUsingBlending)
	{
		initBlending();
		if (profile) profile->time("Clear");
	}
	
	return false; //depth-only render
}
bool LFB_L::count()
{
	CHECKERROR;
	//finish all the imageAtomicAdds for fragment counting
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	LFBBase::count();
	
	if (profile) profile->time("Count");
	
	if (countUsingBlending)
	{
		copyBlendResult();
		if (profile) profile->time("Copy");
	}
	
	//perform parallel prefix sums
	totalFragments = prefixSumsInPlace(offsets);
	
	if (profile) profile->time("PSums");
	
	
	//Counts are not stored as they can be reconstructed
	//as the difference between consecutive offsets. Having the total
	//stored at the end of the offsets saves checking offset array bounds in the shader.
	//However, after rendering they have shifted which means a zero needs to be stored at array index -1 to avoid the if statement
	//The simplest solution was to keep the if statment (very minimal performance overhead)
	/*
	//copy the total into the offset data
	glBindBuffer(GL_TEXTURE_BUFFER, *offsets); //the * operator dereferences data which then returns its bufferObject
	glBufferSubData(
		GL_TEXTURE_BUFFER,
		(prefixSumsSize)*sizeof(unsigned int),
		sizeof(unsigned int),
		&totalFragments);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	*/
	
	//allocate data
	if (allocFragments > totalFragments * 4.0 || allocFragments < totalFragments)
		allocFragments = totalFragments + totalFragments / 2;
	
	if (globalSort)
		allocFragments = ceil(totalFragments, 8) * 8;
	
	data->setFormat(lfbDataType);
	data->resize(allocFragments * lfbDataStride);
	if (globalSort)
		ids->resize(allocFragments * sizeof(int));
	else
		ids->resize(0);
	memory["Data"] = data->size();
	
	if (profile) profile->time("Alloc");
	
	return true; //always need to do a second pass
}
int LFB_L::end()
{
	glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
	if (profile) profile->time("Render");
	
	state = SORTING;

	//global sort
	if (globalSort && allocFragments > 0)
	{
		assert(allocFragments % 8 == 0);
		shaderSort.use();
		setUniforms(shaderSort, "");
		glDrawArrays(GL_POINTS, 0, allocFragments / 8);
		shaderSort.unuse();
		if (profile) profile->time("Sort");
	}
	
	LFBBase::end();
	return totalFragments;
}
std::string LFB_L::getName()
{
	return "LinearizedLFB";
}

bool LFB_L::getDepthHistogram(std::vector<unsigned int>& histogram)
{
	if (!offsets->size())
		return LFBBase::getDepthHistogram(histogram);
	histogram.clear();
	unsigned int* l = (unsigned int*)offsets->map(true, false);
	unsigned int p = 0;
	for (int i = 0; i < getTotalPixels(); ++i)
	{
		assert(offsets->size() > i * (int)sizeof(unsigned int));
		unsigned int v = l[i] - p;
		p = l[i];
		if (histogram.size() <= v)
			histogram.resize(v+1, 0);
		histogram[v]++;
	}
	offsets->unmap();
	return true;
}

bool LFB_L::save(std::string filename) const
{
	if (!offsets->size())
		return false;
	
	std::ofstream ofile(filename.c_str(), std::ios::binary);
	if (!ofile)
		return false;
	
	int pixels = size2D.x * size2D.y;
	std::vector<int> counts(pixels);
	unsigned int* endoffsets = (unsigned int*)offsets->map(true, false);
	unsigned int p = 0;
	for (int i = 0; i < pixels; ++i)
	{
		assert(offsets->size() > i * (int)sizeof(unsigned int));
		counts[i] = endoffsets[i] - p;
		p = endoffsets[i];
	}
	
	const int COMPRESS_KEY = 0x001;
	const int COMPRESS_VAL_NONE = 0x0;
	const int COMPRESS_VAL_ZLIB = 0x1;
	
	const int LAYOUT_KEY = 0x002;
	const int LAYOUT_VAL_PIXELS = 0x000; //63% data compression
	const int LAYOUT_VAL_SHEETS = 0x001; //54% data compression
	
	bool enableCompression = true;
	int layout = LAYOUT_VAL_SHEETS;
	
	//header
	ofile.write("LFB", 3);
	ofile.write((char*)&size2D.x, sizeof(int)); //must have sizex
	ofile.write((char*)&size2D.y, sizeof(int)); //must have sizex
	ofile.write((char*)&lfbDataStride, sizeof(int)); //must have data stride
	int headerStart = (int)ofile.tellp();
	int dataStart = 0;
	ofile.write((char*)&dataStart, sizeof(int));
	
	ofile.write((char*)&COMPRESS_KEY, sizeof(int));
	if (enableCompression)
		ofile.write((char*)&COMPRESS_VAL_ZLIB, sizeof(int));
	else
		ofile.write((char*)&COMPRESS_VAL_NONE, sizeof(int));
		
	ofile.write((char*)&LAYOUT_KEY, sizeof(int));
	ofile.write((char*)&layout, sizeof(int));
	
	//go back and write dataStart, for code to skip the header
	dataStart = (int)ofile.tellp();
	ofile.seekp(headerStart);
	ofile.write((char*)&dataStart, sizeof(int)); //for future attribs
	ofile.seekp(dataStart);
	
	//per-pixel counts - sizex * sizey or (
	if (enableCompression)
	{
		uLongf compressLen = compressBound(counts.size() * sizeof(unsigned int));
		std::vector<char> compressedCounts(compressLen);
		//printf("BEFORE %i\n", compressLen);
		compress((Bytef*)&compressedCounts[0], &compressLen, (Bytef*)&counts[0], counts.size() * sizeof(unsigned int));
		//printf("AFTER %i\n", compressLen);
		int64_t compressLen64 = compressLen;
		ofile.write((char*)&compressLen64, sizeof(int64_t));
		ofile.write((const char*)&compressedCounts[0], compressLen);
	}
	else
		ofile.write((const char*)&counts[0], counts.size() * sizeof(unsigned int));
	//printf("end counts %i\n", (int)ofile.tellp());
	
	//packed data - sum(counts) * data stride
	//FIXME: this has a fairly terrible compression ratio
	float* d = (float*)data->map(true, false);
	int datSize = endoffsets[pixels-1] * lfbDataStride;
	
	
	int attribs = lfbDataStride/sizeof(float);
	std::vector<float> shuffled;
	if (layout == LAYOUT_VAL_SHEETS)
	{
		shuffled.reserve(datSize);
		for (int attrib = 0; attrib < attribs; ++attrib)
		{
			for (int sheet = 0;; ++sheet)
			{
				bool found = false;
				for (int i = 0; i < pixels; ++i)
				{
					if (counts[i] > sheet) //FIXME: REALLY SLOOOOW!!!
					{
						found = true;
						shuffled.push_back(d[(endoffsets[i]-counts[i]+sheet)*attribs+attrib]);
					}
				}
				if (!found)
					break;
			}
		}
		assert(shuffled.size()*sizeof(float) == datSize);
	}
	
	if (enableCompression)
	{
		uLongf compressLen = compressBound(datSize);
		std::vector<char> compressedData(compressLen);
		//printf("BEFORE %i\n", datSize);
		if (shuffled.size())
			compress((Bytef*)&compressedData[0], &compressLen, (Bytef*)&shuffled[0], datSize);
		else
			compress((Bytef*)&compressedData[0], &compressLen, (Bytef*)d, datSize);
		//printf("AFTER %i\n", compressLen);
		int64_t compressLen64 = compressLen;
		ofile.write((char*)&compressLen64, sizeof(int64_t));
		ofile.write((char*)&compressedData[0], compressLen);
	}
	else
	{
		if (shuffled.size())
			ofile.write((char*)&shuffled[0], datSize);
		else
			ofile.write((char*)d, datSize);
	}
	//printf("end data %i\n", (int)ofile.tellp());
	
	ofile.close();
	data->unmap();
	offsets->unmap();
	
	return true;
}

