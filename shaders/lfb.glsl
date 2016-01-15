/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#ifndef GLOBAL_SORT
#define GLOBAL_SORT 0
#endif

#ifndef LFB_READONLY
#define LFB_READONLY 0
#endif

#ifndef LFB_L_REVERSE
#define LFB_L_REVERSE 0
#endif

#define _MAX_FRAGS this_is_defined_by_the_application

#ifndef MAX_FRAGS
#define MAX_FRAGS _MAX_FRAGS
#endif

#define LFB_REQUIRE_COUNTS 0
#define LFB_BINDLESS 0

#define LFB_FRAG_SIZE set_by_app //in floats

#if LFB_FRAG_SIZE == 1
#define LFB_IMAGE_TYPE r32f
#define LFB_FRAG_TYPE float
#define LFB_FRAG_PAD ,0,0,0
#define LFB_FRAG_DEPTH(v) (v)
#elif LFB_FRAG_SIZE == 2
#define LFB_IMAGE_TYPE rg32f
#define LFB_FRAG_TYPE vec2
#define LFB_FRAG_PAD ,0,0
#define LFB_FRAG_DEPTH(v) (v.y)
#elif LFB_FRAG_SIZE == 3
#define LFB_IMAGE_TYPE rgb32f
#define LFB_FRAG_TYPE vec3
#define LFB_FRAG_PAD ,0
#define LFB_FRAG_DEPTH(v) (v.z)
#elif LFB_FRAG_SIZE == 4
#define LFB_IMAGE_TYPE rgba32f
#define LFB_FRAG_TYPE vec4
#define LFB_FRAG_PAD
#define LFB_FRAG_DEPTH(v) (v.w)
#else
#error Invalid LFB_FRAG_SIZE
#endif

//FIXME: should really clear up this signed/unsigned int nonsense at some point
#if LFB_BINDLESS
	#extension GL_NV_gpu_shader5 : enable
	#extension GL_NV_shader_buffer_load : enable

	//FIXME: not sure if bindless graphics have a const for LFB_READONLY
	#define LFB_EXPOSE_TABLE uint*
	#define LFB_EXPOSE_TABLE_COHERENT coherent LFB_EXPOSE_TABLE
	#define LFB_EXPOSE_DATA LFB_FRAG_TYPE*
	#define LFB_EXPOSE_TABLE_GET(buffer, index) int(buffer[index])
	#define LFB_EXPOSE_DATA_GET(buffer, index) buffer[index]
	#define LFB_EXPOSE_TABLE_SET(buffer, index, val) buffer[index] = uint(val)
	#define LFB_EXPOSE_DATA_SET(buffer, index, val) buffer[index] = val
	#define LFB_EXPOSE_TABLE_ADD(buffer, index, val) int(atomicAdd(buffer + index, val))
	#define LFB_EXPOSE_TABLE_EXCHANGE(buffer, index, val) int(atomicExchange(buffer + index, val))
#else
	#if LFB_READONLY
		#define LFB_EXPOSE_TABLE layout(r32ui) readonly uimageBuffer
		#define LFB_EXPOSE_TABLE_COHERENT LFB_EXPOSE_TABLE
		#define LFB_EXPOSE_DATA layout(LFB_IMAGE_TYPE) readonly imageBuffer
	#else
		#define LFB_EXPOSE_TABLE layout(r32ui) uimageBuffer
		#define LFB_EXPOSE_TABLE_COHERENT coherent LFB_EXPOSE_TABLE
		#define LFB_EXPOSE_DATA layout(LFB_IMAGE_TYPE) imageBuffer
	#endif
	#define LFB_EXPOSE_TABLE_GET(buffer, index) int(imageLoad(buffer, index).r)
	#define LFB_EXPOSE_DATA_GET(buffer, index) LFB_FRAG_TYPE(imageLoad(buffer, index))
	#define LFB_EXPOSE_TABLE_SET(buffer, index, val) imageStore(nextPtrs, index, uvec4(val, 0U, 0U, 0U));
	#define LFB_EXPOSE_DATA_SET(buffer, index, val) imageStore(buffer, index, vec4(val LFB_FRAG_PAD));
	#define LFB_EXPOSE_TABLE_ADD(buffer, index, val) int(imageAtomicAdd(buffer, index, val))
	#define LFB_EXPOSE_TABLE_EXCHANGE(buffer, index, val) int(imageAtomicExchange(buffer, index, val).r)
#endif

#define LFB_SIZE(suffix) lfbInfo##suffix.size

//helper hash function
#define LFB_COORD_HASH(suffix, coord) \
	(int((coord).y * lfbInfo##suffix.size.y) * lfbInfo##suffix.size.x + int((coord).x * lfbInfo##suffix.size.x))
//FIXME: should rename to LFB_NCOORD_HASH and LFB_ICOORD_HASH
#define LFB_HASH(suffix, coord) \
	((int((coord).y) * lfbInfo##suffix.size.x) + int((coord).x))
#define LFB_FRAG_HASH(suffix) LFB_HASH(suffix, gl_FragCoord.xy)

//this encode/decode was used to store an alpha channel in addition to rgb + depth
//with proper support for structs in global video memory, this whole process will become much cleaner
//nvidia's NV_shader_buffer_load comes close but in my experience using structs significantly slows performance
vec2 sillyEncode(vec4 v)
{
	return vec2(floor(clamp(v.r, 0.0, 1.0)*4096.0) + clamp(v.g*0.9, 0.0, 0.9), floor(clamp(v.b, 0.0, 1.0)*4096.0) + clamp(v.a*0.9, 0.0, 0.9));
}
vec4 sillyDecode(vec2 v)
{
	return vec4(floor(v.x)/4096.0, fract(v.x)/0.9, floor(v.y)/4096.0, fract(v.y)/0.9);
}

LFB_FRAG_TYPE make_lfb_data(vec2 d)
{
#if LFB_FRAG_SIZE == 1
	return LFB_FRAG_TYPE(d.y);
#elif LFB_FRAG_SIZE == 2
	return d;
#elif LFB_FRAG_SIZE == 3
	return LFB_FRAG_TYPE(d.x, 0.0, d.y);
#elif LFB_FRAG_SIZE == 4
	return LFB_FRAG_TYPE(d.x, 0.0, 0.0, d.y);
#endif
}

#include "lfbTiles.glsl"

#include "LFB_METHOD_H" //Shader::define() will substitute the include string here. thanks, freetype2 for the idea

#define LFB_FOREACH(suffix, frag) \
	LFB_ITER_BEGIN(suffix); \
	for (; LFB_ITER_CONDITION(suffix); LFB_ITER_INC(suffix) ) \
	{ \
		LFB_FRAG_TYPE frag = LFB_GET(suffix);


