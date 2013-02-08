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
#include <errno.h>
#include <string.h>

#include "bitio.h"

/* Struct of bitfile */
struct __bit_file {
    int fd;              /* File descriptor */
    int mode;            /* Mode: read 0, write 1 */
    UINTMAX_T buff_size; /* Buffer size (bits) */
    UINTMAX_T w_start;   /* Window start (bits) */
    UINTMAX_T w_len;     /* Window length (bits) */
    char buff[0];        /* Buffer (contiguous memory area) */
};

/* Return max value that can be represented using UINTMAX_T */
UINTMAX_T max_index() {
    UINTMAX_T max = -1;
    max /= 8;
    return max;
}

bit_file* bit_open(int fd, int mode, UINTMAX_T buff_size) {

    bit_file* bfp;
    int ret;

    if (mode != ACCESS_READ && mode != ACCESS_WRITE)
        return NULL;
    
    if (mode == ACCESS_READ)
        ret = read(fd, NULL, 0);
    else /* mode == ACCESS_WRITE */
        ret = write(fd, NULL, 0);

    if(ret != 0)
        return NULL;

    if (buff_size % 8 != 0)
        return NULL;

    buff_size = (buff_size > max_index()) ? max_index() : buff_size;

    /* Buffer allocation */
    bfp = (bit_file*) calloc(1, sizeof(bit_file) + buff_size / 8);
    if (bfp == NULL) {
        close(fd);
    } else {
        bfp->fd = fd;
        bfp->mode = mode;
        bfp->buff_size = buff_size;
        /* bfp->w_start and bfp->w_len are initialized by calloc */
    }

    return bfp;
}

int bit_read(bit_file* bfp, char* buff_out, UINTMAX_T n_bits, uint8_t ofs) {
    uint8_t* base;
    uint8_t mask;
    uint8_t r_mask;
    uint8_t writebit;
    const uint8_t* readptr;
    UINTMAX_T buff_ready_bytes;
    UINTMAX_T bits_read = 0;
    UINTMAX_T bits_read_total = 0;
    UINTMAX_T buff_size;
    UINTMAX_T w_start;
    UINTMAX_T w_len;
    UINTMAX_T c;
    uint8_t aligned;

    if (bfp == NULL || buff_out == NULL || n_bits < 0 || ofs < 0 || ofs > 7)
        return -1;

    if (bfp->mode != ACCESS_READ)
        return -1;

    buff_size = bfp->buff_size;
    w_start = bfp->w_start;
    w_len = bfp->w_len;

    mask = 1 << ofs;
    base = (uint8_t*) buff_out;

    /* Check if input ad output are aligned to byte */
    aligned = (mask == 1 && (w_start % 8 == 0)) ? 1 : 0;

    while (n_bits > 0) {
        /* Buffer refill if needed */
        if (w_len == 0) {
            c = read(bfp->fd, bfp->buff, buff_size / 8);
            if (c == -1) {
                if (errno == EAGAIN) {
                    errno = 0;
                    break;
                } else {
                    return -1;
                }
            } else if (c == 0) {
                break;
            }

            w_start = 0;
            w_len = c * 8;
        }

        readptr = (uint8_t*)&(bfp->buff) + w_start / 8;

        if (aligned && w_len > 7 && n_bits >= w_len) {
            /* Optimization: due to alignment we can use memcpy */
            buff_ready_bytes = w_len / 8;
            memcpy(base, readptr, buff_ready_bytes);
            base += buff_ready_bytes;

            bits_read = buff_ready_bytes * 8;
            w_start = (w_start + bits_read) % buff_size;
            w_len -= bits_read;
            n_bits -= bits_read;
            bits_read_total += bits_read;
        } else {
            /* Single bit read */
            r_mask = 1 << w_start % 8;

            writebit = (*readptr & r_mask) ? 1 : 0;
            if (writebit == 0) {
                *base &= ~mask;
            } else {
                *base |= mask;
            }

            w_start = ((w_start + 1) % buff_size);
            --w_len;
            --n_bits;
            ++bits_read_total;

            if (mask == 0x80) {
                mask = 1;
                ++base;
                aligned = (mask == 1 && (w_start % 8 == 0)) ? 1 : 0;
            } else {
                mask <<= 1;
            }
        }
    }

    /* Update */
    bfp->buff_size = buff_size;
    bfp->w_start = w_start;
    bfp->w_len = w_len;
    
    return bits_read_total;
}

int bit_write(bit_file* bfp, const char* buff_in, UINTMAX_T n_bits, uint8_t ofs) {
    UINTMAX_T ret = 0;
    const uint8_t* base;
    uint8_t mask;
    uint8_t readbit;
    uint8_t* writeptr;
    UINTMAX_T pos;
    UINTMAX_T buff_free_bits;
    UINTMAX_T buff_free_bytes;
    UINTMAX_T bits_written = 0;
    uint8_t aligned;

    if (bfp == NULL || buff_in == NULL || n_bits < 0 || ofs < 0 || ofs > 7)
        return -1;

    if (bfp->mode != ACCESS_WRITE)
        return -1;

    mask = 1 << ofs;
    base = (uint8_t*)buff_in;

    pos = bfp->w_start + bfp->w_len;
    buff_free_bits = bfp->buff_size - bfp->w_len;

    /* Check if input ad output are aligned to byte */
    aligned = (mask == 1 && (pos % 8 == 0)) ? 1 : 0;

    while (n_bits > 0) {
        writeptr = (uint8_t*)&(bfp->buff) + pos / 8;

        if (aligned && buff_free_bits > 7 && n_bits >= buff_free_bits) {
            /* Optimization: due to alignment we can use memcpy */
            buff_free_bytes = buff_free_bits / 8;
            memcpy(writeptr, base, buff_free_bytes);
            base += buff_free_bytes;
            bits_written = buff_free_bytes * 8;

            pos += bits_written;
            bfp->w_len += bits_written;
            n_bits -= bits_written;
            ret += bits_written;
            buff_free_bits -= bits_written;
        } else {
            /* Single bit write */
            readbit = (*base & mask) ? 1 : 0;
            if (readbit == 0) {
                *writeptr &= ~(1 << pos % 8);
            } else {
                *writeptr |= (1 << pos % 8);
            }

            if (mask == 0x80) {
                mask = 1;
                ++base;
                aligned = (mask == 1 && (pos % 8 == 0)) ? 1 : 0;
            } else {
                mask <<= 1;
            }

            ++pos;
            ++(bfp->w_len);
            --(n_bits);
            --buff_free_bits;
            ++ret;
        }

        /* Flush if needed */
        if (bfp->w_len == bfp->buff_size) {
            if (bit_flush(bfp) == -1)
                return -1;
            if (bfp->w_len != 0)
                return ret;
            pos = bfp->w_start + bfp->w_len;
            buff_free_bits = bfp->buff_size;
        }

    }

    return ret;
}

int bit_flush(bit_file* bfp) {
    UINTMAX_T count;
    UINTMAX_T written;
    UINTMAX_T n;
    uint8_t* base;

    if (bfp == NULL)
        return -1;

    count = bfp->w_len / 8;
    written = 0;
    base = (uint8_t*) bfp->buff + bfp->w_start / 8;

    while (count > 0) {
        n = write(bfp->fd, base, count);
        if (n == -1) {
            if (errno == EAGAIN) {
                errno = 0;
                break;
            } else {
                return -1;
            }
        }
        base += n;
        written += n;
        count -= n;
    }

    bfp->w_start = (bfp->w_start + written * 8) % bfp->buff_size;
    bfp->w_len -= written * 8;
    return 0;
}

int bit_close(bit_file* bfp) {
    int fd;

    if (bfp == NULL)
        return -1;

    fd = bfp->fd;

    if (bfp->w_len % 8)
        bfp->w_len += 8 - (bfp->w_len % 8);

    bit_flush(bfp);
    free(bfp);
    close(fd);
    return 0;
}
