/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#version 420

layout(r32ui) uniform uimageBuffer sums;

uniform int offsetRead;
uniform int offsetWrite;
uniform int pass;

void main()
{
	int read = offsetRead + gl_VertexID * 2;
	int write = offsetWrite + gl_VertexID;
	if (pass == 0)
	{
		uint a = imageLoad(sums, read).r;
		uint b = imageLoad(sums, read+1).r;
		imageStore(sums, write, uvec4(a + b));
		//if (offsetWrite - offsetRead == 2)
		//	imageStore(sums, write + 1, uvec4(a + b));
	}
	else
	{
		uint a = imageLoad(sums, read).r;
		uint b = imageLoad(sums, write).r;
		imageStore(sums, read+1, uvec4(a+b));
		imageStore(sums, read, uvec4(b));
	}
/*
	int r1, r2, w1, w2;
	r1 = offsetRead + gl_VertexID * 2;
	r2 = offsetRead + gl_VertexID * 2 + 1;
	w1 = offsetWrite + gl_VertexID;
	if (pass == 1)
	{
		r2 = offsetWrite + gl_VertexID;
		w1 = offsetRead + gl_VertexID * 2 + 1;
		w2 = offsetRead + gl_VertexID * 2;
	}

	uint a = imageLoad(sums, r1).r;
	uint b = imageLoad(sums, r2).r;
	imageStore(sums, w1, uvec4(a + b));
	if (pass == 1)
		imageStore(sums, w2, uvec4(a));
*/
}
