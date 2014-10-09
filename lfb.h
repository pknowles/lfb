/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#ifndef LFB_H
#define LFB_H

#include "lfbBase.h"

class Shader;
class Profiler;

class LFB
{
public:
	enum LFBType {
		LFB_TYPE_B,
		LFB_TYPE_LL,
		LFB_TYPE_PLL,
		LFB_TYPE_L,
		LFB_TYPE_CL,
	};
private:
	int maxFrags;
	vec2i pack;
	GLenum format;
	LFBBase* instance;
	LFBType current;
	void initCurent();
public:
	LFB();
	operator LFBBase*();
	LFBBase* operator->();
	void setType(LFBType type);
	LFBType getType();
	void capture(void (*scene)(Shader*), Shader* depthonly, Shader* shader, std::string lfbName, Profiler* profiler = NULL);
	void release();
};

#endif
