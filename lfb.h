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
		LFB_TYPE_B, //basic (3D array)
		LFB_TYPE_LL, //linked lists
		LFB_TYPE_PLL, //paged linked lists (Crassin spinlock)
		LFB_TYPE_L, //linearized (prefix sum scan)
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
