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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wrapper.h"

/* Structure representing the type of algorithm */
struct __algorithm {
    char* name;    /* String representing the name */
    uint8_t type;  /* Constant representing the type */
};

/* Opaque type representing the type of algorithm */
typedef struct __algorithm algorithm;

/* Struct of available algorithms */
const algorithm algo_list[] = {
    {"lz78", LZ78_ALGORITHM}, 
    {NULL,   UNKNOWN_ALGORITHM}
};

/* Struct representing the wrapper used for compression or decompression */
struct __wrapper {
    uint8_t type;      /* Algorithm used to compress or decompress data */
    uint8_t mode;      /* Flag indicating compress/decompress mode */
    void *data;        /* Opaque structure representing the algorithm */
};

/* Global variable representing the current error stored */
uint8_t wrapper_cur_err = WRAPPER_SUCCESS;

/* Associate an algorithm-dependent error to a wrapper-generic error */
uint8_t wrapper_return(uint8_t code) {
    wrapper_cur_err = code;
    switch (code) {
        case LZ78_SUCCESS:
            return WRAPPER_SUCCESS;
        case LZ78_ERROR_READ:
            return WRAPPER_ERROR_READ;
        case LZ78_ERROR_WRITE:
            return WRAPPER_ERROR_WRITE;
        case LZ78_ERROR_EAGAIN:
            return WRAPPER_ERROR_EAGAIN;
        case LZ78_ERROR_COMPRESS:
            return WRAPPER_ERROR_COMPRESS;
        case LZ78_ERROR_DECOMPRESS:
            return WRAPPER_ERROR_DECOMPRESS;
        case LZ78_ERROR_DICTIONARY:
        case LZ78_ERROR_INITIALIZATION:
        case LZ78_ERROR_MODE:
            return WRAPPER_ERROR_GENERIC;
    }
    return code;
}

uint8_t get_algorithm(char *type) {
    uint8_t i = 0;
    while (algo_list[i].name != NULL) {
        if (strcmp(type, algo_list[i].name) == 0)
            return algo_list[i].type;
        i++;
    }
    return UNKNOWN_ALGORITHM;
}

int byte_size(char *size) {
    int n;
    if (size == NULL) return 0;
    n = atoi(size);

    switch (size[strlen(size) - 1]) {
        case 'K':
            n <<= 10;
            break;
	
        case 'M':
            n <<= 20;
            break;
    }

    return (n < 0) ? 0 : n;
}

void wrapper_perror() {
    switch (wrapper_cur_err) {
        case WRAPPER_SUCCESS:
            break;

        case WRAPPER_ERROR_ALGORITHM:
            fprintf(stderr, "Unrecognized compression algorithm\n");
            break;

        case WRAPPER_ERROR_FILE_IN:
            fprintf(stderr, "Unable to read input file\n");
            break;

        case WRAPPER_ERROR_FILE_OUT:
            fprintf(stderr, "Unable to write output file\n");
            break;

        case LZ78_SUCCESS:
            break;

        case LZ78_ERROR_DICTIONARY:
            fprintf(stderr, "LZ78: unable to allocate dictionaries\n");
            break;

        case LZ78_ERROR_INITIALIZATION:
            fprintf(stderr, "LZ78: bad initialization\n");
            break;

        case LZ78_ERROR_MODE:
            fprintf(stderr, "LZ78: wrong compression/decompression mode\n");
            break;

        case LZ78_ERROR_READ:
            fprintf(stderr, "LZ78: unable to read input data\n");
            break;

        case LZ78_ERROR_WRITE:
            fprintf(stderr, "LZ78: unable to write output data\n");
            break;

        case LZ78_ERROR_EAGAIN:
            fprintf(stderr, "LZ78: I/O operation would block: retry...\n");
            break;

        case LZ78_ERROR_COMPRESS:
            fprintf(stderr, "LZ78: unable to compress input data\n");
            break;

        case LZ78_ERROR_DECOMPRESS:      
            fprintf(stderr, "LZ78: unable to decompress input data\n");
            break;

        default:
            fprintf(stderr, "Unhandled error code %d\n", wrapper_cur_err);
    }
}

wrapper* wrapper_new(uint8_t w_mode, uint8_t w_type, char *argv) {
    wrapper *w;
    w = malloc(sizeof(wrapper));
    if (w == NULL)
        return NULL;

    w->type = w_type;
    w->mode = w_mode;

    switch (w->type) {
        case LZ78_ALGORITHM:
            w->data = lz78_new(w_mode, byte_size(argv));
            break;

        default:
            free(w);
            return NULL;
    }

    if (w->data)
        return w;
    else {
        free(w);
        return NULL;
    }
}

void wrapper_destroy(wrapper *w) {
    if (w == NULL)
        return;

    switch (w->type) {
        case LZ78_ALGORITHM:
            lz78_destroy(w->data);
            break;

        default:
            return;
    }
    free(w);
}

uint8_t wrapper_compress(wrapper *w, char *input, char *output) {
    int fd_in, fd_out;
    uint8_t ret;

    switch (w->type) {
        case LZ78_ALGORITHM:
            if (input == NULL) {
                fd_in = STDIN_FILENO;
            } else {
                fd_in = open(input, ACCESS_READ);
                if (fd_in == -1)
                    return wrapper_return(WRAPPER_ERROR_FILE_IN);
            }

            if (output == NULL) {
                fd_out = STDOUT_FILENO;
            } else {
                fd_out = open(output, ACCESS_WRITE, 0644);
                if (fd_out == -1) {
                    close(fd_in);
                    return wrapper_return(WRAPPER_ERROR_FILE_OUT);
                }
            }

            ret = lz78_compress(w->data, fd_in, fd_out);

            close(fd_in);
            close(fd_out);
            return wrapper_return(ret);

        default:
            return wrapper_return(WRAPPER_ERROR_ALGORITHM);
    }
}

uint8_t wrapper_decompress(wrapper *w, char *input, char *output) {
    int fd_in, fd_out;
    uint8_t ret;

    switch (w->type) {
        case LZ78_ALGORITHM:
            if (input == NULL) {
                fd_in = STDIN_FILENO;
            } else {
                fd_in = open(input, ACCESS_READ);
                if (fd_in == -1)
                    return wrapper_return(WRAPPER_ERROR_FILE_IN);
            }

            if (output == NULL) {
                fd_out = STDOUT_FILENO;
            } else {
                fd_out = open(output, ACCESS_WRITE, 0644);
                if (fd_out == -1) {
                    close(fd_in);
                    return wrapper_return(WRAPPER_ERROR_FILE_OUT);
                }
             }
            
            ret = lz78_decompress(w->data, fd_in, fd_out);

            close(fd_in);
            close(fd_out);
            return wrapper_return(ret);

        default:
            return wrapper_return(WRAPPER_ERROR_ALGORITHM);
    }
}

uint8_t wrapper_exec(wrapper *w, char *input, char *output) {
    int ret;
    if (w->mode == WRAPPER_MODE_COMPRESS) {
        for (;;) {
            ret = wrapper_compress(w, input, output);
            if (ret != WRAPPER_ERROR_EAGAIN)
                return ret;
        }
    } else {
        for (;;) {
            ret = wrapper_decompress(w, input, output);
            if (ret != WRAPPER_ERROR_EAGAIN)
                return ret;
        }
    }
}
