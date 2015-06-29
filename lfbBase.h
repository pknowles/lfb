/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef LFB_BASE_H
#define LFB_BASE_H

#include <pyarlib/vec.h>
#include <pyarlib/matrix.h>
#include <pyarlib/profile.h>
#include <pyarlib/shaderutil.h>
#include <pyarlib/shader.h>
#include <pyarlib/util.h>
#include <pyarlib/resources.h>

#include <map>
#include <string>

struct TextureBuffer;

class LFBBase
{
private:
	//disable copying - destructor frees GL memory
	LFBBase(const LFBBase& copy) {}
	void operator=(const LFBBase& copy) {}
	
	//zeroed data to quickly zero buffers. this is resized to the maximum needed
	static TextureBuffer* zeroes;
	
	int lastLFBDataStride; //to override resize() returning false on data type change
	
protected:
public: //FIXME: for testing

	//states change with begin() count() and end() calls
	enum State {
		PRE_INIT, //begin hasn't been called
		FIRST_PASS, //between begin and count
		SECOND_PASS, //between count and end
		SORTING, //during end
		DRAWING, //after end (or before next begin call)
	};
	
	State state;
	
	int maxFrags; //max depth complexity
	
	//only set these via setFormat()!
	int lfbDataType; //GL data type, eg GL_RGBA32F
	int lfbDataStride; //bytes per fragment

	vec2i size2D;
	
	//increases size to the next multiple of this. required for efficient memory indexing
	vec2i pack;
	
	bool isDirty;
	
	bool computeCounts; //for linked lists, this can be avoided but some algorithms need it
	
	bool bindless; //instead of image_load_store, use bindless graphics
	
	int totalPixels; //size2D.x * size2D.y
	int totalFragments; //is not always accurate but will never be less than exact
	int allocFragments; //data allocated, for some implementations will allocate more to reduce allocation frequency
	
	void zeroBuffer(TextureBuffer* buffer, int size = 0); //coppies zeros to buffer. if size is positive, only zeroes part of the buffer
	
	virtual bool _resize(vec2i dim) {return false;} //called on begin() when isDirty
	
public:

	std::map<std::string, int> memory; //info only
	int getTotalPixels();
	int getTotalFragments();
	int getMaxFrags();

	//for debugging - to time algorithm steps. point this to an instance
	Profiler* profile;

	LFBBase();
	virtual ~LFBBase();
	bool resize(int size); //resize buffers such as counts and head pointers
	bool resize(vec2i dim); //resize buffers such as counts and head pointers
	vec2i getSize();
	void setMaxFrags(int n); //preferably change this once as altering maxFrags will cause a recompile for the #define
	virtual void setDefines(Shader& program); //call before shader.use(). unless defines change, shader will not recompile
	virtual bool setUniforms(Shader& program, std::string suffix) =0; //return false if out-of-memory or other error. use suffix to bind multiple LFBs
	bool setPack(int i);
	bool setPack(vec2i i);
	vec2i getPack();
	virtual bool begin(); //return true to render colour, false for depth only
	virtual bool count(); //return true to render second pass
	virtual int end();
	virtual void sort();
	virtual std::string getName() =0; //just for debugging
	bool requireCounts(bool enable = true);
	bool useBindlessGraphics(bool enable = true);
	size_t getMemoryUsage();
	std::string getMemoryInfo();
	int getFormat();
	bool setFormat(int f); //default: GL_RG32F. GL_RGBA32F is the only other one I've tested with
	virtual bool getDepthHistogram(std::vector<unsigned int>& histogram);
	
	/*
	The LFB file format...
	3-char header "LFB"
	4-byte int size-x
	4-byte int size-y
	4-byte int data-stride-in-bytes
	4-byte int data-start-offset (use to calc remaining header size)
	a list of optional 4-byte key / 4 byte value pairs. see following and cpp file
	if compressed,
		8-byte int64 compressed counts size in bytes
		count data (decompress to list of 4-byte ints)
		8-byte int64 compressed frag data size in bytes
		all the lfb fragment data (decompress to whatever the data is. application dependent)
	else
		raw count data (size-x * size-y)
		raw frag data (sum(counts) * data-stride-in-bytes)
	
	if layout "pixels",
		data is ordered as lists of pixels
	if layout "sheets",
		data is ordered by (see cpp file):
		1. splitting fragments into 4-byte groups
		2. grouping fragments by their list index (all pixe[0], pixel[1] etc.)
		3. in pixel (x first, then y) order
	*/
	
	virtual bool save(std::string filename) const;
	virtual bool load(std::string filename);
};

#endif
