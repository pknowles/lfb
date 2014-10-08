/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef PREFIX_SUMS_H
#define PREFIX_SUMS_H

class TextureBuffer;

//turns the unsigned ints within data into their prefix sums
//and returns the total sum. it is expected that data be of size 2^n
unsigned int prefixSumsInPlace(TextureBuffer* data);
unsigned int prefixSumsMip(TextureBuffer* data, int offset, int count);

#endif
