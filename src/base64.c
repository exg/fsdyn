#include "base64.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "fsalloc.h"
#include "fsdyn_version.h"

size_t base64_encoding_size(size_t binary_size)
{
    size_t encoding_size = (binary_size + 2) / 3 * 4;
    if (encoding_size < binary_size)
        return -1;
    return encoding_size;
}

const char base64_bitfield_encoding[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const int8_t base64_bitfield_decoding[256] = {
    -4, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -1, -2, -2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, -1, -1, -1, -3, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1,
    -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static char base64_char(uint8_t bitfield, char pos62, char pos63)
{
    bitfield &= 0x3f;
    switch (bitfield) {
        case 62:
            return pos62;
        case 63:
            return pos63;
        default:
            return base64_bitfield_encoding[bitfield];
    }
}

static size_t put4(char *p, size_t i, size_t size, char c0, char c1, char c2,
                   char c3)
{
    if (i++ < size)
        p[i - 1] = c0;
    if (i++ < size)
        p[i - 1] = c1;
    if (i++ < size)
        p[i - 1] = c2;
    if (i++ < size)
        p[i - 1] = c3;
    return i;
}

size_t base64_encode_buffer(const void *source, size_t source_size, char *dest,
                            size_t dest_size, char pos62, char pos63)
{
    size_t i = 0;
    const uint8_t *q = source;
    const uint8_t *tail = q + source_size - source_size % 3;
    uint32_t bits;
    if (pos62 == BASE64_DEFAULT_CHAR)
        pos62 = '+';
    if (pos63 == BASE64_DEFAULT_CHAR)
        pos63 = '/';
    size_t dest_body_counter = dest_size / 4;
    while (dest_body_counter-- && q < tail) {
        bits = q[0] << 16 | q[1] << 8 | q[2];
        dest[i++] = base64_char(bits >> 18, pos62, pos63);
        dest[i++] = base64_char(bits >> 12, pos62, pos63);
        dest[i++] = base64_char(bits >> 6, pos62, pos63);
        dest[i++] = base64_char(bits, pos62, pos63);
        q += 3;
    }
    if (q < tail) {
        bits = q[0] << 16 | q[1] << 8 | q[2];
        i = put4(dest, i, dest_size, base64_char(bits >> 18, pos62, pos63),
                 base64_char(bits >> 12, pos62, pos63),
                 base64_char(bits >> 6, pos62, pos63),
                 base64_char(bits, pos62, pos63));
        q += 3;
    }
    i += (tail - q) / 3 * 4;
    q = tail;
    switch (source_size % 3) {
        case 0:
            break;
        case 1:
            bits = q[0] << 16;
            i = put4(dest, i, dest_size, base64_char(bits >> 18, pos62, pos63),
                     base64_char(bits >> 12, pos62, pos63), '=', '=');
            break;
        default:
            bits = q[0] << 16 | q[1] << 8;
            i = put4(dest, i, dest_size, base64_char(bits >> 18, pos62, pos63),
                     base64_char(bits >> 12, pos62, pos63),
                     base64_char(bits >> 6, pos62, pos63), '=');
    }
    if (i < dest_size)
        dest[i] = '\0';
    else if (dest_size)
        dest[dest_size - 1] = '\0';
    return i;
}

char *base64_encode_simple(const void *buffer, size_t size)
{
    size_t encoding_size = base64_encoding_size(size);
    if (encoding_size == -1)
        return NULL;
    char *result = fsalloc(encoding_size + 1);
    (void) base64_encode_buffer(buffer, size, result, encoding_size + 1,
                                BASE64_DEFAULT_CHAR, BASE64_DEFAULT_CHAR);
    return result;
}

static bool good_termination(const char *remainder, size_t remainder_size,
                             bool ignore_wsp)
{
    if (!ignore_wsp)
        return remainder_size == 0 || !*remainder;
    for (; remainder_size && *remainder; remainder++, remainder_size--)
        if (base64_bitfield_decoding[*remainder & 0xff] !=
            BASE64_ILLEGAL_WHITESPACE)
            return false;
    return true;
}

static bool good_base64_tail(const char *tail, size_t tail_size,
                             unsigned bit_count, bool ignore_wsp)
{
    const char *padding;
    switch (bit_count) {
        case 0:
            padding = "";
            break;
        case 6:
            return false;
        case 4:
            padding = "==";
            break;
        case 2:
            padding = "=";
            break;
        default:
            assert(false);
    }
    size_t padding_len = strlen(padding);
    return tail_size >= padding_len && !strncmp(tail, padding, padding_len) &&
        good_termination(tail + padding_len, tail_size - padding_len,
                         ignore_wsp);
}

static int8_t get_bits(char c, char pos62, char pos63, bool ignore_wsp)
{
    if (c == pos62)
        return 62;
    if (c == pos63)
        return 63;
    int8_t new_bits = base64_bitfield_decoding[c & 0xff];
    switch (new_bits) {
        case 62:
        case 63:
            return BASE64_ILLEGAL_OTHER;
        case BASE64_ILLEGAL_WHITESPACE:
            if (!ignore_wsp)
                return BASE64_ILLEGAL_OTHER;
            return new_bits;
        default:
            return new_bits;
    }
}

ssize_t base64_decode_buffer(const char *source, size_t source_size, void *dest,
                             size_t dest_size, char pos62, char pos63,
                             bool ignore_wsp)
{
    size_t si = 0;
    uint8_t *buffer = dest;
    size_t pi = 0;
    unsigned bits = 0;
    unsigned bit_count = 0;
    if (pos62 == BASE64_DEFAULT_CHAR)
        pos62 = '+';
    if (pos63 == BASE64_DEFAULT_CHAR)
        pos63 = '/';
    while (pi < dest_size && si < source_size && source[si] &&
           source[si] != '=') {
        int8_t new_bits = get_bits(source[si++], pos62, pos63, ignore_wsp);
        if (new_bits < 0) {
            if (new_bits == BASE64_ILLEGAL_WHITESPACE)
                continue;
            errno = EILSEQ;
            return -1;
        }
        bits = bits << 6 | new_bits;
        bit_count += 6;
        if (bit_count >= 8) {
            bit_count -= 8;
            buffer[pi++] = bits >> bit_count & 0xff;
        }
    }
    while (si < source_size && source[si] && source[si] != '=') {
        int8_t new_bits = get_bits(source[si++], pos62, pos63, ignore_wsp);
        if (new_bits < 0) {
            if (new_bits == BASE64_ILLEGAL_WHITESPACE)
                continue;
            errno = EILSEQ;
            return -1;
        }
        bits = bits << 6 | new_bits;
        bit_count += 6;
        if (bit_count >= 8) {
            bit_count -= 8;
            pi++;
        }
    }
    if (bits & ~(~0U << bit_count) ||
        !good_base64_tail(&source[si], source_size - si, bit_count,
                          ignore_wsp)) {
        errno = EILSEQ;
        return -1;
    }
    if ((ssize_t) pi < 0) {
        errno = EOVERFLOW;
        return -1;
    }
    return pi;
}

void *base64_decode_simple(const char *encoding, size_t *binary_size)
{
    ssize_t count = base64_decode_buffer(encoding, -1, NULL, 0,
                                         BASE64_DEFAULT_CHAR, BASE64_DEFAULT_CHAR, true);
    if (count < 0)
        return NULL;
    uint8_t *result = fsalloc(count + 1);
    (void) base64_decode_buffer(encoding, -1, result, count,
                                BASE64_DEFAULT_CHAR, BASE64_DEFAULT_CHAR, true);
    result[count] = '\0';
    *binary_size = count;
    return result;
}
