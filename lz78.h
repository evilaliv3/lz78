/*
* Basic implementation of LZ78 compression algorithm 
*
* Copyright (C) 2010 evilaliv3 <giovanni.pellerano@evilaliv3.org>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LZ78_H
#define __LZ78_H

#include "bitio.h"

/* Modes of compression */
#define LZ78_MODE_COMPRESS        0
#define LZ78_MODE_DECOMPRESS      1

/* List of lz78-level return codes */
#define LZ78_SUCCESS              0
#define LZ78_ERROR_DICTIONARY     1
#define LZ78_ERROR_READ           2
#define LZ78_ERROR_WRITE          3
#define LZ78_ERROR_EAGAIN         4
#define LZ78_ERROR_COMPRESS       5
#define LZ78_ERROR_DECOMPRESS     6 
#define LZ78_ERROR_INITIALIZATION 7
#define LZ78_ERROR_MODE           8

/* Size of the dictionary */
#define DICT_SIZE_MIN                260
#define DICT_SIZE_DEFAULT            4096
#define DICT_SIZE_MAX                1048576

/* Opaque type representing the compression instance */
typedef struct __lz78_instance lz78_instance;

/* Allocate and return an instance of lz78 compressor
   cmode:   specify compress/decompress mode
   dsize:   specify the size of the dictionary (byte)
 */
lz78_instance* lz78_new(uint8_t cmode, uint32_t dsize);

/* Compress the input stream by sending the result to the output stream
   arg:     current instance of compressor obtained by invoking lz78_init()
   Return:  one of defined lz78-level return codes
 */
uint8_t lz78_compress(lz78_instance* lz78, int fd_in, int fd_out);

/* Decompress the input stream by sending the result to the output stream
   arg:     current instance of compressor obtained by invoking lz78_init()
   Return:  one of defined lz78-level return codes
 */
uint8_t lz78_decompress(lz78_instance* lz78, int fd_in, int fd_out);


/* Deallocate current instance */
void lz78_destroy(lz78_instance* lz78);

#endif /* __LZ78_H */
