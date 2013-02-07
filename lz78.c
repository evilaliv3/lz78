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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "lz78.h"

/* Code used to represent an EOF */
#define DICT_CODE_EOF    256
/* Code used before to send the size of the dictionary */
#define DICT_CODE_SIZE   257
/* Code used by the compressor to start the operations */
#define DICT_CODE_START  258
/* Code used by the compressor to stop the operations */
#define DICT_CODE_STOP   259

/* Limits dict_size inside [DICT_SIZE_MIN, DICT_SIZE_MAX] */
#ifndef DICT_LIMIT
#define DICT_LIMIT(x) (((x) < (DICT_SIZE_MIN + 1)) ? (DICT_SIZE_MIN + 1) : (((x) > (DICT_SIZE_MAX)) ? (DICT_SIZE_MAX) : (x)))
#endif

/* Compute the threshold for the start of secondary dictionary */
#define DICT_SIZE_THRESHOLD(x) ((x) * 8 / 10)

/* Entry of the hash table used by the compressor to encode data */
struct __ht_entry {
    uint8_t used;             /* Flag indicating if the node is used or not */
    uint32_t parent;          /* Parent node */
    uint16_t label;           /* Node's label */
    uint32_t child;           /* Child node */
};

/* The opaque type of hash table entry used by the compressor */
typedef struct __ht_entry ht_entry;

/* Dictionary of the compressor implemented as an hash table */
struct __ht_dictionary {
    ht_entry *root;           /* Root node of the dictionary */
    uint32_t cur_node;        /* Current position inside the dictionary */
    uint32_t prev_node;       /* Pointer to the father of cur_node */
    uint32_t d_size;          /* Size of the dictionary */
    uint32_t d_thr;           /* Threshold for activation of secondary dictionary */
    uint32_t d_next;          /* Next code to put in the dictionary */
};

/* The opaque type representing the dictionary used by the compressor */
typedef struct __ht_dictionary ht_dictionary;

/* State of a compressor */
struct __lz78_c {
    uint8_t completed;        /* Termination flag */
    uint32_t d_size;          /* Size of the dictionaries */
    ht_dictionary *main;      /* Main dictionary */
    ht_dictionary *secondary; /* Secondary dictionary */
    uint32_t bitbuf;          /* Buffer containing bits not yet written */
    uint32_t n_bits;          /* Number of valid bits in the buffer */
};

/* The opaque type representing the state of the compressor */
typedef struct __lz78_c lz78_c;

/* Entry of the dictionary used by the decompressor */
struct __entry {
    uint32_t parent;          /* Parent node */
    uint16_t label;           /* Node's label */
};

/* The opaque type of a dictionary entry used by the decompressor */
typedef struct __entry entry;

/* Dictionary of the decompressor */
struct __dictionary {
    entry *root;              /* Root node of the dictionary */
    uint32_t d_size;          /* Size of the dictionray */
    uint32_t d_thr;           /* Threshold for activation of secondary dictionary */
    uint32_t d_min;           /* Minimum size of the dictionary */
    uint32_t d_next;          /* Next code to put in the dictionary */
    uint32_t n_bytes;         /* Number of bytes contained in bytebuf */
    uint32_t offset;          /* Offset of the first valid byte inside bytebuf */
    char bytebuf[0];          /* Buffer used to output strings */
};

/* The opaque type representing the dictionary used by the decompressor */
typedef struct __dictionary dictionary;

/* State of the decompressor */
struct __lz78_d {
    uint8_t completed;        /* Termination flag */
    dictionary *main;         /* Main dictionary */
    ht_dictionary *secondary; /* Secondary dictionary */
    uint32_t bitbuf;          /* Buffer containing bits not yet written */
    uint32_t n_bits;          /* Number of valid bits contained in the buffer */
};

/* The opaque type representing the status of the decompressor */
typedef struct __lz78_d lz78_d;

/* lz78 instance descriptor */
struct __lz78_instance {
    uint8_t mode;             /* Discriminate compression operations */
    char state[0];            /* Compression/Decompression state struct */    
};

/* Return the number of bits needed to represent the given number */
uint8_t bitlen(uint32_t i);

/* Create a new ht_dictionary to be used for the compression */
ht_dictionary* ht_dictionary_new(uint32_t d_size);

/* Update the dictionary depending with input byte
   Return:
     0   a new entry have been put in the dictionary
    -1   switch the current node
 */
int ht_dictionary_update(ht_dictionary *d, uint16_t label);

/* Reset the dictionary associated to the given compressor */
void ht_dictionary_reset(ht_dictionary *d);

/* Destroy the given ht_dictionary object */
void ht_dictionary_destroy(ht_dictionary *d);

/* Create a new dictionary to be used for the decompression */
dictionary *dictionary_new(uint32_t d_size);

/* Update the internal state of the dictionary */
void dictionary_update(dictionary *d, uint32_t code);

/* Reset the dictionary associated to the given decompressor */
void dictionary_reset(dictionary *d);

/* Destroy the given dictionary object */
void dictionary_destroy(dictionary *d);

/* Compress the input byte and modifiy the state of the dictionary */
void compress_byte(lz78_c *o, int c_in);

/* Decompress the input code and modify the state of the dictionary */
int decompress_code(lz78_d *o, uint32_t code);

uint8_t bitlen(uint32_t i) {
    uint8_t n = 0;
    while (i) {
        n++;
        i >>= 1;
    }
    return n;
}

ht_dictionary* ht_dictionary_new(uint32_t d_size) {
    ht_dictionary *dict = malloc(sizeof (ht_dictionary));
    if (dict == NULL)
        return NULL;

    d_size = DICT_LIMIT(d_size);
    dict->root = calloc(1, sizeof (ht_entry) * d_size);
    if (dict->root == NULL) {
        free(dict);
        return NULL;
    }

    dict->d_size = d_size;
    dict->d_thr = DICT_SIZE_THRESHOLD(d_size);
    dict->d_next = DICT_SIZE_MIN;
    dict->cur_node = -1;
    return dict;
}

int ht_dictionary_update(ht_dictionary *d, uint16_t label) {
    uint8_t i;
    uint32_t key, hash;
    d->prev_node = d->cur_node;

    if (d->cur_node == -1) {
        d->cur_node = label;
        return -1;
    }

    /* Bernstein hash function */
    key = (label << bitlen(d->d_size)) + d->cur_node;
    hash = 0;
    for (i = 0; i < 4; i++) {
        hash = ((hash << 5) + hash) + (key & 0xFF);
        key >>= 8;
    }
    hash %= d->d_size;

    /* Search if current sequence is present, else return an empty hash entry
       where insert it */
    while (d->root[hash].used) {
        if (d->root[hash].parent == d->cur_node &&
                d->root[hash].label == label) {
            d->cur_node = d->root[hash].child;
            return -1;
        } else {
            /* Collision (linear search) */
            hash = (hash + 1) % d->d_size;
        }
    }

    /* At this point, in d->prev_node there is the symbol we will send */

    /* Fill out hash entry */
    d->root[hash].used = 1;
    d->root[hash].parent = d->prev_node;
    d->root[hash].label = label;
    d->root[hash].child = d->d_next;
    /* Update current node */
    d->cur_node = label;
    /* Update next symbol */
    d->d_next++;

    return 0;
}

void ht_dictionary_reset(ht_dictionary *d) {
    memset(d->root, 0, sizeof (ht_entry) * d->d_size);
    d->d_next = DICT_SIZE_MIN;
    d->cur_node = -1;
}

void ht_dictionary_destroy(ht_dictionary *d) {
    if (d != NULL) {
        free(d);
    }
}

dictionary* dictionary_new(uint32_t d_size) {
    uint16_t i;
    dictionary *dict = malloc(sizeof (dictionary) + d_size);
    if (dict == NULL)
        return NULL;

    d_size = DICT_LIMIT(d_size);
    dict->root = malloc(sizeof (entry) * d_size);
    if (dict->root == NULL) {
        free(dict);
        return NULL;
    }

    dict->d_size = d_size;
    dict->d_thr = DICT_SIZE_THRESHOLD(d_size);
    dict->d_min = DICT_SIZE_MIN;
    dict->d_next = DICT_SIZE_MIN;
    for (i = 0; i < DICT_SIZE_MIN; i++) {
        dict->root[i].parent = 0;
        dict->root[i].label = i;
    }
    return dict;
}

void dictionary_update(dictionary *d, uint32_t code) {
    uint32_t d_size = d->d_size - 1;
    uint32_t d_next = d->d_next;
    uint32_t d_min = d->d_min;
    uint32_t i = d_size;
    uint32_t p = code;

    /* Recover original sequence */
    while (1) {
        d->bytebuf[i--] = d->root[p].label;
        if (p < DICT_SIZE_MIN || i == 0)
            break;
        p = d->root[p].parent;
    }

    /* Fill last char with the first char of the sequence */
    if (code >= d_min && code == d_next - 1)
        d->bytebuf[d_size] = d->bytebuf[i + 1];

    /* Update last incomplete entry of the dictionary */
    if (d_next > d_min)
        d->root[d_next - 1].label = d->bytebuf[i + 1];

    /* Update */
    d->n_bytes = d_size - i;
    d->offset = d_size + 1 - d->n_bytes;
    d->root[d_next].parent = code;
    d->d_next++;
}

void dictionary_reset(dictionary *d) {
    d->d_min = DICT_SIZE_MIN;
    d->d_next = DICT_SIZE_MIN;
}

void dictionary_destroy(dictionary *d) {
    if (d != NULL) {
        free(d->root);
        free(d);
    }
}

void compress_byte(lz78_c *o, int c_in) {
    /* Optimization pointers */
    ht_dictionary *d_main = o->main;
    ht_dictionary *d_sec = o->secondary;

    switch(d_main->cur_node) {
	case DICT_CODE_START:
            o->bitbuf = d_main->d_size;
            o->n_bits = bitlen(DICT_SIZE_MAX);
            d_main->cur_node = -1;
            break;
	case DICT_CODE_EOF:
            o->bitbuf = d_main->cur_node;
            o->n_bits = bitlen(d_main->d_next);
            d_main->cur_node = DICT_CODE_STOP;
            return;
        case DICT_CODE_STOP:
            o->completed = 1;
            return;
	default:
            break;
    }

    c_in = c_in == EOF ? DICT_CODE_EOF : c_in;
    /* Dictonaries update */
    if (ht_dictionary_update(d_main, c_in) != 0) {
        if (d_main->d_next >= d_main->d_thr)
            ht_dictionary_update(d_sec, c_in);
        return;
    }

    o->bitbuf = d_main->prev_node;
    o->n_bits = bitlen(d_main->d_next - 1);

    /* Dictonaries swap */
    if (d_main->d_next == d_main->d_size) {
        o->main = o->secondary;
        o->secondary = d_main;
        d_main = d_sec;
        d_sec = o->secondary;
        d_main->cur_node = c_in;
        ht_dictionary_reset(d_sec);
    }
    /* Update of secondary if threshold is reached */
    if (d_main->d_next >= d_main->d_thr)
        ht_dictionary_update(d_sec, c_in);
}

int decompress_code(lz78_d *o, uint32_t code) {
    uint32_t i;
    int c_in;
    /* Optimization pointers */
    dictionary *d_main = o->main;
    ht_dictionary *d_sec = o->secondary;

    switch(code) {
	case DICT_CODE_EOF:
            o->completed = 1;
            return 0;
	case DICT_CODE_START:
        case DICT_CODE_SIZE:
            d_main->d_next = DICT_SIZE_MAX;
            o->n_bits = 0;
            return 0;
	default:
            /* Initial operations */
            if (d_main->d_next == DICT_SIZE_MAX) {
                dictionary_destroy(d_main);
                d_main = dictionary_new(code);
                o->main = d_main;
                if (d_main == NULL)
                    return -1;
                ht_dictionary_destroy(d_sec);
                d_sec = ht_dictionary_new(code);
                o->secondary = d_sec;
                if (d_sec == NULL) {
                    dictionary_destroy(d_main);
                    o->main = NULL;
                    return -1;
                }
                o->bitbuf = 0;
                o->n_bits = 0;
                return 0;
            }
            break;
    }

    /* Bad compressed file */
    if (d_sec == NULL || d_main == NULL)
        return -2;

    dictionary_update(d_main, code);

    /* Update of secondary if threshold is reached */
    if (d_main->d_next > d_main->d_thr) {
        for (i = 0; i < d_main->n_bytes; i++) {
            c_in = (uint8_t) d_main->bytebuf[d_main->offset + i];
            ht_dictionary_update(d_sec, c_in);
        }
    }

    /* Dictonaries swap */
    if (d_main->d_next == d_main->d_size) {
        dictionary_reset(d_main);
        d_main->d_min = d_sec->d_next;
        d_main->d_next = d_sec->d_next;
        for (i = 0; i < d_sec->d_size && d_sec->d_next; i++) {
            if (d_sec->root[i].used) {
                d_main->root[d_sec->root[i].child].parent =
                        d_sec->root[i].parent;
                d_main->root[d_sec->root[i].child].label =
                        d_sec->root[i].label;
                d_sec->d_next--;
            }
        }
        ht_dictionary_reset(d_sec);
    }
    return 0;
}

lz78_instance* lz78_new(uint8_t cmode, uint32_t dsize) {
    lz78_instance *i;
    lz78_c *c;
    lz78_d *d;

    int max_dim = (sizeof(lz78_c) > sizeof(lz78_d)) ? sizeof(lz78_c) : sizeof(lz78_d);
    i = malloc(sizeof (lz78_instance) + max_dim);
    if (i == NULL)
        return NULL;
    i->mode = cmode;

    switch (cmode) {
        case LZ78_MODE_COMPRESS:
            c = (lz78_c*)&i->state; 
            dsize = (dsize == 0) ? DICT_SIZE_DEFAULT : dsize;
            c->d_size = DICT_LIMIT(dsize);
            c->completed = 0;
            c->main = ht_dictionary_new(c->d_size);
            if (c->main == NULL) {
                free(i);
                return NULL;
            }
            c->secondary = ht_dictionary_new(c->d_size);
            if (c->secondary == NULL) {
                ht_dictionary_destroy(c->main);
                free(i);
                return NULL;
            }
            c->bitbuf = DICT_CODE_START;
            c->n_bits = bitlen(DICT_SIZE_MIN);
            c->main->cur_node = DICT_CODE_START;
            return i;

        case LZ78_MODE_DECOMPRESS:
            d = (lz78_d*)&i->state; 
            d->completed = 0;
            d->main = dictionary_new(DICT_SIZE_MIN);
            if (d->main == NULL) {
                free(i);
                return NULL;
            }
            return i;

        default:
            return NULL;
    }
}

uint8_t lz78_compress(lz78_instance *lz78, int fd_in, int fd_out) {
    FILE *in;
    bit_file *out;
    lz78_c *o;
    int bits;
    int c_in;

    if (lz78 == NULL)
        return LZ78_ERROR_INITIALIZATION;

    if (lz78->mode != LZ78_MODE_COMPRESS)
        return LZ78_ERROR_MODE;

    in = fdopen(fd_in, "r");
    if (in == NULL)
        return LZ78_ERROR_READ;

    out = bit_open(fd_out, ACCESS_WRITE, B_SIZE_DEFAULT);
    if (out == NULL)
        return LZ78_ERROR_WRITE;

    o = (lz78_c*)&lz78->state;

    for (;;) {

        if (o->n_bits > 0) {
            bits = bit_write(out, (char*) &o->bitbuf, o->n_bits, 0);
            if (bits == -1)
                return LZ78_ERROR_WRITE;

            o->bitbuf >>= bits;
            o->n_bits -= bits;

            if (o->n_bits > 0)
                return LZ78_ERROR_EAGAIN;
        }

        c_in = fgetc(in);
        if (c_in == EOF) {
            if (errno == EAGAIN) {
                errno = 0;
                return LZ78_ERROR_EAGAIN;
            } else if (errno != 0) {
                return LZ78_ERROR_READ;
            }
        }

        compress_byte(o, c_in);
        if (o->completed == 1) {
            bit_close(out);
            return LZ78_SUCCESS;
        }
    }
}

uint8_t lz78_decompress(lz78_instance *lz78, int fd_in, int fd_out) {
    bit_file *in;
    FILE *out;
    lz78_d *o;
    dictionary *d_main;
    uint32_t bits, written;
    int ret;

    if (lz78 == NULL)
        return LZ78_ERROR_INITIALIZATION;

    if (lz78->mode != LZ78_MODE_DECOMPRESS)
        return LZ78_ERROR_MODE;

    in = bit_open(fd_in, ACCESS_READ, B_SIZE_DEFAULT);
    if (in == NULL)
        return LZ78_ERROR_READ;

    out = fdopen(fd_out, "w");
    if (out == NULL)
        return LZ78_ERROR_WRITE;

    o = (lz78_d*)&lz78->state;

    for (;;) {

        /* Optimization pointer (MUST be init every cycle) */
        d_main = o->main;
        if (d_main->n_bytes) {
            written = 0;
            while (written != d_main->n_bytes) {
                ret = fwrite(d_main->bytebuf + d_main->offset + written, 1,
                        d_main->n_bytes - written, out);
                if (ret == -1) {
                    d_main->offset += written;
                    d_main->n_bytes -= written;
                    if (errno == EAGAIN) {
                        errno = 0;
                        return LZ78_ERROR_EAGAIN;
                    } else
                        return LZ78_ERROR_WRITE;
                }
                written += ret;
            }
        }

        o->bitbuf = 0;
        o->n_bits = 0;
        bits = bitlen(d_main->d_next);

        if (bits > 0) {
            ret = bit_read(in, (char*) &o->bitbuf, bits, 0);
            if (ret == -1)
                return LZ78_ERROR_READ;

            o->n_bits = ret;
            if (bits != o->n_bits)
                return LZ78_ERROR_EAGAIN;
        }


        ret = decompress_code(o, o->bitbuf);
        if (ret < 0) {
            switch(ret) {
                case -1:
                    return LZ78_ERROR_DICTIONARY;
                case -2:
                    return LZ78_ERROR_DECOMPRESS;
            }
        }

        if (o->completed == 1) {
            fflush(out);
            return LZ78_SUCCESS;
        }

    }
}

void lz78_destroy(lz78_instance *lz78) {
    lz78_c *c;
    lz78_d *d;
    if (lz78 != NULL) {
        switch (lz78->mode) {
            case LZ78_MODE_COMPRESS:
                c = (lz78_c*)&lz78->state;
                if (c != NULL) {
                    ht_dictionary_destroy(c->main);
                    ht_dictionary_destroy(c->secondary);
                }
                break;

            case LZ78_MODE_DECOMPRESS:
                d = (lz78_d*)&lz78->state;
                if (d != NULL) {
                    dictionary_destroy(d->main);
                    ht_dictionary_destroy(d->secondary);
                }
                break;
        }

        free(lz78);
    }
}
