

#include <string>
#include <map>
#include <vector>
#include <set>
#include <sstream>

#include <pyarlib/profile.h>
#include <pyarlib/shader.h>

#include "lfb.h"
#include "lfbB.h"
#include "lfbLL.h"
#include "lfbPLL.h"
#include "lfbL.h"
#include "lfbCL.h"

LFB::LFB()
{
	maxFrags = 64;
	current = LFB_TYPE_LL;
	instance = NULL;
	format = GL_RG32F;
	pack = vec2i(1);
}

void LFB::initCurent()
{
	switch (current)
	{
	case LFB_TYPE_B: instance = new LFB_B(); break;
	case LFB_TYPE_LL: instance = new LFB_LL(); break;
	case LFB_TYPE_PLL: instance = new LFB_PLL(); break;
	case LFB_TYPE_L: instance = new LFB_L(); break;
	case LFB_TYPE_CL: instance = new LFB_CL(); break;
	}
	
	printf("Created a %s\n", instance->getName().c_str());
	
	instance->setPack(pack);
	instance->setMaxFrags(maxFrags);
	instance->setFormat(format);
}

LFB::operator LFBBase*()
{
	if (!instance)
		initCurent();
	return instance;
}

LFBBase* LFB::operator->()
{
	if (!instance)
		initCurent();
	return instance;
}

void LFB::setType(LFBType type)
{
	//if (current == type)
	//	return; //already this type

	current = type;
	
	if (instance)
	{
		release();
		
		initCurent();
		
		LFB_L* lfbL = dynamic_cast<LFB_L*>(instance);
		LFB_LL* lfbLL = dynamic_cast<LFB_LL*>(instance);
	}
}

LFB::LFBType LFB::getType()
{
	return current;
}

void LFB::capture(void (*scene)(Shader*), Shader* depthonly, Shader* shader, std::string lfbName, Profiler* profiler)
{
	LFBBase* lfb = (LFBBase*)*this;

	if (profiler)
		lfb->profile = profiler;

	if (profiler) profiler->start("Construct");
		
	bool fullRender = lfb->begin();
	
	//FIXME: potentially slow if changing every frame
	if (!fullRender)
		lfb->setDefines(*depthonly);
	lfb->setDefines(*shader);
	
	//first pass
	Shader* firstRender = fullRender ? shader : depthonly;
	firstRender->use();
	lfb->setUniforms(*firstRender, lfbName);
	scene(firstRender);
	firstRender->unuse();
	
	//second pass, if needed
	if (lfb->count())
	{
		shader->use();
		lfb->setUniforms(*shader, lfbName);
		scene(shader);
		shader->unuse();
	}
	lfb->end();
	
	if (profiler) profiler->time("Construct");
}
	
void LFB::release()
{
	//back up certain LFB settings
	pack = instance->getPack();
	maxFrags = instance->getMaxFrags();
	format = instance->getFormat();
	//instance->release(); //no longer needed. handled in destructor
	delete instance;
	instance = NULL;
}
