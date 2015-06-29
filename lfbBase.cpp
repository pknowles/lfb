/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include <stdio.h>

#include <string>
#include <map>
#include <vector>
#include <set>
#include <sstream>

#include "lfbBase.h"

#include <pyarlib/includegl.h>
#include <pyarlib/shader.h>
#include <pyarlib/shaderutil.h>
#include <pyarlib/util.h>
#include <pyarlib/gpu.h>

TextureBuffer* LFBBase::zeroes = NULL;

static Shader shaderZeroes("lfbZero");

//permutations of this shader use for different MAX_FRAGS values
static std::map<int, Shader*> sortingShaders;

EMBED(lfbZeroVert, shaders/lfbZero.vert);
EMBED(lfbGLSL, shaders/lfb.glsl);
EMBED(lfbSortVert, shaders/lfbSort.vert);
EMBED(sortingGLSL, shaders/sorting.glsl);
EMBED(lfbTilesGLSL, shaders/lfbTiles.glsl);

LFBBase::LFBBase() : size2D(0, 0)
{
	//all LFBBase shaders are embedded. when the first LFBBase is created, the embedded data is given to the shader compiler
	static bool loadEmbed = false;
	if (!loadEmbed)
	{
		Shader::include("lfbZero.vert", RESOURCE(lfbZeroVert));
		Shader::include("lfb.glsl", RESOURCE(lfbGLSL));
		Shader::include("sorting.glsl", RESOURCE(sortingGLSL));
		Shader::include("lfbSort.vert", RESOURCE(lfbSortVert));
		Shader::include("lfbTiles.glsl", RESOURCE(lfbTilesGLSL));
		loadEmbed = true;
	}
	
	maxFrags = 16;

	totalPixels = 0;
	totalFragments = 0;
	allocFragments = 0;
	state = PRE_INIT;
	profile = NULL;
	
	computeCounts = false;
	
	bindless = false;
	
	isDirty = false;
	
	lastLFBDataStride = 0;
	lfbDataType = GL_RG32F;
	lfbDataStride = bytesPerPixel(lfbDataType);
	
	pack = vec2i(1);
}
LFBBase::~LFBBase()
{
}
vec2i LFBBase::getSize()
{
	return size2D;
}
bool LFBBase::resize(int size)
{
	if (pack.y > 1)
		printf("WARNING at %s:%i: a resize(int) has been called with a 2D pack\n", __FILE__, __LINE__);
	return resize(vec2i(size, 1));
}
bool LFBBase::resize(vec2i dim)
{
	dim = vec2i(ceil(dim.x, pack.x), ceil(dim.y, pack.y)) * pack;
	
	if (size2D == dim)
		return false;
	
	size2D = dim;
	totalPixels = size2D.x * size2D.y;
	isDirty = true;
	return true;
}
bool LFBBase::setPack(int i)
{
	if (pack.x == i)
		return false;
	pack = vec2i(i, 1);
	isDirty = true;
	return true;
}
bool LFBBase::setPack(vec2i i)
{
	if (pack == i)
		return false;
	pack = i;
	isDirty = true;
	return true;
}

vec2i LFBBase::getPack()
{
	return pack;
}

//FIXME: using copy() used to be fast but I don't think it is anymore
void LFBBase::zeroBuffer(TextureBuffer* buffer, int size)
{
	if (buffer->size() == 0)
		return; //nothing to do
	
	if (size <= 0)
		size = buffer->size();

	CHECKERROR;
	if (!zeroes)
	{
		zeroes = new TextureBuffer(GL_R32UI);
	}
	if (size > zeroes->size())
	{
		zeroes->resize(size);

		int currentProgram;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		if (currentProgram != 0)
			printf("Warning: program active before init zero buffer %s:%i\n", __FILE__, __LINE__);
		
		//generate zero buffer
		glEnable(GL_RASTERIZER_DISCARD);
		shaderZeroes.use();
		//zeroes->bind(0, "tozero", shaderZeroes, false, true);
		shaderZeroes.set("tozero", *zeroes);
		
		glDrawArrays(GL_POINTS, 0, zeroes->size() / sizeof(unsigned int));
		glMemoryBarrierEXT(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
		
		shaderZeroes.unuse();
		glDisable(GL_RASTERIZER_DISCARD);
		
		#if 0
		//at one point I thought the above was failing
		unsigned int* d = (unsigned int*)zeroes->map(true, true);
		for (int i = 0; i < zeroes->size() / (int)sizeof(unsigned int); ++i)
		{
			//printf("%i %i\n", i, d[i]);
			d[i] = 0;
		}
		zeroes->unmap();
		#endif
	}
	
	memory["Zeroes"] = zeroes->size();
		
	//FIXME: really could do multiple copies and allocate less zero data
	CHECKERROR;
	zeroes->copy(buffer, 0, 0, size);
	CHECKERROR;
}
int LFBBase::getTotalPixels()
{
	return totalPixels;
}
int LFBBase::getTotalFragments()
{
	return totalFragments;
}
int LFBBase::getMaxFrags()
{
	return maxFrags;
}
void LFBBase::setMaxFrags(int n)
{
	maxFrags = mymax(2, n);
}
void LFBBase::setDefines(Shader& program)
{
	program.define("_MAX_FRAGS", maxFrags);
	program.define("LFB_FRAG_SIZE", lfbDataStride/sizeof(float));
	program.define("LFB_BINDLESS", bindless);
	
	//LFB_READONLY could also be defined here, however
	//in case additional operations need to be performed, such
	//as writing after sorting, LFB_READONLY is left for each shader to define itself
}
bool LFBBase::begin()
{
	if (isDirty)
	{
		_resize(size2D);
		isDirty = false;
		printf("LFB Resized %ix%i (pack %ix%i, max %i, stride %i = vec%if)\n", size2D.x, size2D.y, pack.x, pack.y, maxFrags, lfbDataStride, lfbDataStride/4);
	}
	
	if (totalPixels == 0)
		printf("Warning: rendering to empty LFBBase!\n");
	state = FIRST_PASS;
	
	return false; //is a full colour pass needed, or just depth complexity
}
bool LFBBase::count()
{
	state = SECOND_PASS;
	return false; //is a second pass needed? no by default
}
int LFBBase::end()
{
	state = DRAWING;
	return 0; //number of fragments rendered
}
void LFBBase::sort()
{
	//a shader which simply calls loadFragments, sortFragments and writeFragments
	Shader* sorter = NULL;
	if (sortingShaders.find(maxFrags) == sortingShaders.end())
		sortingShaders[maxFrags] = sorter = new Shader("lfbSort");
	else
		sorter = sortingShaders[maxFrags];
	CHECKERROR;
	setDefines(*sorter);
	if (!sorter->use())
		exit(0);
	setUniforms(*sorter, "toSort");
	glEnable(GL_RASTERIZER_DISCARD);
	glDrawArrays(GL_POINTS, 0, totalPixels);
	glDisable(GL_RASTERIZER_DISCARD);
	sorter->unuse();
	CHECKERROR;
}
bool LFBBase::requireCounts(bool enable)
{
	if (computeCounts == enable)
		return false;
		
	computeCounts = enable;
	isDirty = true;
	return true;
}
bool LFBBase::useBindlessGraphics(bool enable)
{
	if (bindless == enable)
		return false;
	bindless = enable;
	return true;
}
std::string LFBBase::getMemoryInfo()
{
	std::stringstream ret;
	int total = 0;
	for (std::map<std::string, int>::iterator it = memory.begin(); it != memory.end(); ++it)
	{
		total += it->second;
		ret << it->first << ": " << humanBytes(it->second, false) << std::endl;
	}
	ret << "Total" << ": " << humanBytes(total, false) << std::endl;
	return ret.str();
}

int LFBBase::getFormat()
{
	return lfbDataType;
}

bool LFBBase::setFormat(int f)
{
	if (lfbDataType == f)
		return false;
	lfbDataType = f;
	lfbDataStride = bytesPerPixel(lfbDataType);
	isDirty = true;
	return true;
}

size_t LFBBase::getMemoryUsage()
{
	size_t total = 0;
	std::map<std::string, int>::iterator it;
	for (it = memory.begin(); it != memory.end(); ++it)
		total += it->second;
	return total;
}
	
bool LFBBase::getDepthHistogram(std::vector<unsigned int>& histogram)
{
	return false;
}

bool LFBBase::save(std::string filename) const
{
	printf("LFB::save() not implemented for this type\n");	
	return false;
}

bool LFBBase::load(std::string filename)
{
	printf("LFB::load() not implemented for this type\n");	
	return false;
}

