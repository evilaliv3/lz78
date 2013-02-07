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

#include "wrapper.h"

/* Usage program help */
void help(char *argv[]) {
    fprintf(stderr,
            "Usage: %s [Options]\n\n"
            "Options:\n"
            "-h          show this help\n"
            "-i input    sets input source\n"
            "-o output   sets output destination\n"
            "-d          sets decompress mode\n"
            "-t type     sets compression algorithm\n"
            "\n"
            "Optional flags:\n"
            "-b bsize    sets size of I/O buffers\n"
            "-a param    sets additional parameter\n"
            "",
            argv[0]);
}


int main(int argc, char *argv[]) {
    char *name_in = NULL;
    char *name_out = NULL;
    char *w_argv = NULL;
    wrapper *w;
    int bsize = B_SIZE_DEFAULT;
    int opt, ret;
    uint8_t w_mode = WRAPPER_MODE_COMPRESS;
    uint8_t w_type = LZ78_ALGORITHM;

    while ((opt = getopt(argc, argv, "i:o:dt:b:a:h")) != -1) {
        switch (opt) {
            case 'i': /* Input */
                name_in = optarg;
                break;

            case 'o': /* Output */
                name_out = optarg;
                break;

            case 'd': /* Decompress */
                w_mode = WRAPPER_MODE_DECOMPRESS;
                break;

            case 't': /* Type */
                if (!(w_type = get_algorithm(optarg))) {
                    fprintf(stderr, "Invalid algorithm type: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

            case 'b': /* Buffer size */
                bsize = byte_size(optarg);
                break;

            case 'a': /* Additional parameter */
                w_argv = optarg;
                break;

            case 'h': /* Compressor help */
            default:
                help(argv);
                exit(EXIT_FAILURE);
        }
    }

    if (bsize <= 0) {
        help(argv);
        exit(EXIT_FAILURE);
    }

    /* Creates a wrapper instance */
    w = wrapper_new(w_mode, w_type, w_argv);
    if (w == NULL) {
        fprintf(stderr, "Unable to create wrapper\n");
        exit(EXIT_FAILURE);
    }

    /* Executes the wrapper function */
    ret = wrapper_exec(w, name_in, name_out);
    
    if (ret != WRAPPER_SUCCESS)
        wrapper_perror();

    /* Destroyes the wrapper instance */
    wrapper_destroy(w);

    return ret;
}
