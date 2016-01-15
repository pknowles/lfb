/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#version 420

layout(r32ui) uniform uimageBuffer offsets;
layout(r32ui) uniform uimageBuffer tozero;

uniform int offsetRead;
uniform int offsetWrite;

uniform ivec2 vpSize;

void main()
{
	int read = offsetRead + gl_VertexID * 2;
	int write = offsetWrite + gl_VertexID;
	
	uint val;
	/*
	if (offsetRead == 0)
	{
		ivec2 pos = ivec2((gl_VertexID*2) % vpSize.x, (gl_VertexID*2) / vpSize.x);
		val = 0;
		const int r = 5;
		for (int y = max(0, pos.y-r); y < min(vpSize.y-1, pos.y+r); ++y)
			for (int x = max(0, pos.x-r); x < min(vpSize.x-1, pos.x+r+1); ++x)
				val = max(val, imageLoad(offsets, y*vpSize.x+x).r);
		val += val / 2;
	}
	else*/
		val = max(imageLoad(offsets, read).r, imageLoad(offsets, read+1).r);
	imageStore(offsets, write, uvec4(val));
	
	if (offsetRead == 0)
	{
		imageStore(tozero, gl_VertexID*2+0, uvec4(0U));
		imageStore(tozero, gl_VertexID*2+1, uvec4(0U));
	}
	
	gl_Position = vec4(0, 0, 0, 0); //<-- ok, are you happy now, mr windows driver???
}
