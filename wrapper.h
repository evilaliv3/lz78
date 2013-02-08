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

#ifndef __WRAPPER_H
#define __WRAPPER_H

#include "lz78.h"

/* List of included compression algorithms */
#define UNKNOWN_ALGORITHM         0
#define LZ78_ALGORITHM            1

/* Modes of compression */
#define WRAPPER_MODE_COMPRESS     0
#define WRAPPER_MODE_DECOMPRESS   1

/* List of managed wrapper-level errors */
#define WRAPPER_SUCCESS           20
#define WRAPPER_ERROR_ALGORITHM   21
#define WRAPPER_ERROR_FILE_IN     22
#define WRAPPER_ERROR_FILE_OUT    23
#define WRAPPER_ERROR_READ        24
#define WRAPPER_ERROR_WRITE       25
#define WRAPPER_ERROR_EAGAIN      26
#define WRAPPER_ERROR_COMPRESS    27
#define WRAPPER_ERROR_DECOMPRESS  28
#define WRAPPER_ERROR_GENERIC     29

/* Opaque type representing the wrapper */
typedef struct __wrapper wrapper;

/* Creates a new wrapper:
   w_mode   mode of compression
   w_type   type of algorithm
   w_argv   additional parameter
 */
wrapper* wrapper_new(uint8_t w_mode, uint8_t w_type, char* w_argv);

/* Deallocates a wrapper */
void wrapper_destroy(wrapper* w);

/* Execute the function associated with the wrapper (compress/decompress)
   Return:
     WRAPPER_SUCCESS          on success
     WRAPPER_ERROR_FILE_IN    unable to open input file
     WRAPPER_ERROR_FILE_OUT   unable to open output file
     WRAPPER_ERROR_READ       unable to read input data
     WRAPPER_ERROR_WRITE      unable to write output data
     WRAPPER_ERROR_EAGAIN     unable to accomplish current operation
     WRAPPER_ERROR_ALGORITHM  type of wrapper unknown
     WRAPPER_ERROR_COMPRESS   unable to compress input data
     WRAPPER_ERROR_DECOMPRESS unable to decompress input data
     WRAPPER_ERROR_GENERIC    algorithm-dependent error
 */
uint8_t wrapper_exec(wrapper* w, char* in, char* out);

/* Return a positive constant associated to a particular algorithm
   (UNKNOWN_ALGORITHM if doesn't exist)
 */
uint8_t get_algorithm(char* type);

/* Return an integer representing the given size
   (K = KBytes, M = MBytes)
 */
int byte_size(char* size);

/* Print last wrapper error occurred into standard error stream */
void wrapper_perror();

#endif /* __WRAPPER_H */
