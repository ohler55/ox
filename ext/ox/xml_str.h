/* xml_str.h
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 */

#ifndef OX_XML_STR_H
#define OX_XML_STR_H

#include <stdint.h>
#include <string.h>

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_memcpy)
#define HAVE_FAST_MEMCPY 1

inline static void fast_memcpy16(void *dest, const void *src, size_t n) {
    char       *d = (char *)dest;
    const char *s = (const char *)src;

    if (n >= 8) {
        __builtin_memcpy(d, s, 8);
        __builtin_memcpy(d + n - 8, s + n - 8, 8);
    } else if (n >= 4) {
        __builtin_memcpy(d, s, 4);
        __builtin_memcpy(d + n - 4, s + n - 4, 4);
    } else if (n >= 2) {
        __builtin_memcpy(d, s, 2);
        __builtin_memcpy(d + n - 2, s + n - 2, 2);
    } else if (n >= 1) {
        *d = *s;
    }
}
#endif

/* Shared helpers for the serializer escape path used by both dump.c and
 * builder.c. The size scan and the escape loop both walk the source string a
 * word at a time and only fall to a byte at a time when a word contains a byte
 * that might need escaping.
 *
 * Each escape table maps a byte to the length of its serialized form held as an
 * ASCII digit, '1' meaning the byte is copied through unchanged. The tables in
 * use (xml_element_chars, xml_quote_chars / xml_attr_chars) escape only a
 * subset of
 *
 *     a byte below 0x20, '"', '\'', '&', '<', '>'
 *
 * so a word none of whose bytes are in that set is entirely pass-through and
 * every one of its bytes is '1' in every table. The predicate below is that
 * union. It is deliberately a superset: '"' and '\'' are pass-through in
 * xml_element_chars, and 0x09, 0x0a and 0x0d are pass-through in every table,
 * but flagging them only sends those bytes to the byte loop, which copies them
 * through unchanged, so the output is identical. It must never miss a byte a
 * table would escape, which is why the whole escaped union is covered even
 * though no current table escapes '\''.
 */

#define XSTR_ONES 0x0101010101010101ULL
#define XSTR_HIGH 0x8080808080808080ULL

/* Sets the high bit of every byte of a word that is not guaranteed to be
 * pass-through, that is a byte below 0x20, or a '"', '\'', '&', '<' or '>'.
 *
 * The byte below 0x20 test is the classic (v - ones*0x20) & ~v hasless, and the
 * five character tests are haszero on v xored with the character broadcast to
 * every byte. Bytes with the high bit already set, the UTF-8 lead and
 * continuation bytes, are cleared by the & ~... term and so are never flagged.
 *
 * A borrow out of one byte can also flag the byte above it, but a borrow is
 * only produced by a byte that already matched, so the lowest flagged byte is
 * always a real match and there are never false negatives. That is what lets a
 * zero result mean the whole word is pass-through, and the count of trailing
 * zeros point at the first byte that has to go through the table.
 */
inline static uint64_t xml_bytes_of_interest(uint64_t v) {
    uint64_t q = v ^ (XSTR_ONES * (uint64_t)'"');
    uint64_t s = v ^ (XSTR_ONES * (uint64_t)'\'');
    uint64_t a = v ^ (XSTR_ONES * (uint64_t)'&');
    uint64_t l = v ^ (XSTR_ONES * (uint64_t)'<');
    uint64_t g = v ^ (XSTR_ONES * (uint64_t)'>');

    return (((v - XSTR_ONES * 0x20) & ~v) | ((q - XSTR_ONES) & ~q) | ((s - XSTR_ONES) & ~s) | ((a - XSTR_ONES) & ~a) |
            ((l - XSTR_ONES) & ~l) | ((g - XSTR_ONES) & ~g)) &
           XSTR_HIGH;
}

/* Offset of the lowest flagged byte, which is the first byte of the word that
 * has to go through the table. Only ever called with a non-zero mask.
 */
inline static int xml_first_of_interest(const unsigned char *s, uint64_t mask) {
#if defined(__GNUC__) || defined(__clang__)
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    (void)s;
    return (int)(__builtin_clzll(mask) >> 3);
#else
    (void)s;
    return (int)(__builtin_ctzll(mask) >> 3);
#endif
#else
    int i;

    (void)mask;
    for (i = 0; i < 8; i++) {
        unsigned char u = s[i];

        if (u < 0x20 || '"' == u || '\'' == u || '&' == u || '<' == u || '>' == u) {
            break;
        }
    }
    return i;
#endif
}

/* Length of str once serialized through table, the sum of the table entries
 * less len * '0' so that a pass-through byte counts as one. A word none of
 * whose bytes need escaping contributes 8 * '1' with no table lookup at all;
 * only words with a byte of interest are summed one byte at a time. The word
 * loop is bounded by str + len so it never reads past the string.
 */
inline static size_t xml_str_len(const unsigned char *str, size_t len, const char *table) {
    const unsigned char *end  = str + len;
    size_t               size = 0;

    while (str + 8 <= end) {
        uint64_t v;

        memcpy(&v, str, 8);
        if (0 == xml_bytes_of_interest(v)) {
            size += 8 * (size_t)'1';
        } else {
            int i;

            for (i = 0; i < 8; i++) {
                size += table[str[i]];
            }
        }
        str += 8;
    }
    for (; str < end; str++) {
        size += table[*str];
    }
    return size - len * (size_t)'0';
}

#endif /* OX_XML_STR_H */
