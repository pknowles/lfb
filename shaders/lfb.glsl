/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#ifndef GLOBAL_SORT
#define GLOBAL_SORT 0
#endif

#ifndef LFB_READONLY
#define LFB_READONLY 0
#endif

#define _MAX_FRAGS this_is_defined_by_the_application

#ifndef MAX_FRAGS
#define MAX_FRAGS _MAX_FRAGS
#endif

#define LFB_REQUIRE_COUNTS 0

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

#define LFB_SIZE(suffix) lfbInfo##suffix.size

//helper hash function
#define LFB_COORD_HASH(suffix, coord) \
	(int((coord).y * lfbInfo##suffix.size.y) * lfbInfo##suffix.size.x + int((coord).x * lfbInfo##suffix.size.x))
#define LFB_HASH(suffix, coord) \
	((int((coord).y) * lfbInfo##suffix.size.x) + int((coord).x))
#define LFB_FRAG_HASH(suffix) LFB_HASH(suffix, gl_FragCoord.xy)

//implemented by LFB types
//#define LFB_DEC(name) - declare lfb
//#define lfbHash(suffix, coord)
//#define addFragment(suffix, hash, dataVec, depth)
//#define loadFragments(suffix, hash, fragslist)
//#define writeFragments(suffix, fragslist)
//#define countFragments(suffix, hash)
//#define iterFragments(suffix, hash, fragOut)

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
for ( \
	LFB_ITER_BEGIN(suffix); \
	LFB_ITER_CONDITION(suffix); \
	LFB_ITER_INC(suffix) \
	) \
{ \
	LFB_FRAG_TYPE frag = LFB_GET(suffix);

