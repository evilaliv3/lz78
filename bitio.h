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

#ifndef __BITIO_H
#define __BITIO_H

#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>

#define B_SIZE_DEFAULT 1048576

#define UINTMAX_T uint32_t

/* Access mode for reading and writing */
#define ACCESS_READ (O_RDONLY | O_NONBLOCK)
#define ACCESS_WRITE (O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK)

/* The opaque type used for bitwise streams */
typedef struct __bit_file bit_file;

/* Creates a new bit_file opening a file f with the specificated mode and size */
bit_file* bit_open(int fd, int mode, UINTMAX_T bufsize);

/* Does a memory read (occasionally an i/o read) */
int bit_read(bit_file* bf, char* base, UINTMAX_T n_bits, uint8_t ofs);

/* Does a memory write (occasionally an i/o flush) */
int bit_write(bit_file* bf, const char* base, UINTMAX_T n_bits, uint8_t ofs);

/* Effectively swap out the buffer into memory */
int bit_flush(bit_file* bf);

/* Relases the resources allocated by the bit_file */
int bit_close(bit_file* bf);

#endif /* __BITIO_H */
