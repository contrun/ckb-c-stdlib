//
// If you have only one single C file, you can include std header files in it,
// and then done. It automatically includes impl.c(this file). It is compatible
// to a lot of old CKB code.
//
// If you have more than one C file, and it's a big project. You need to define
// macro CKB_DECLARATION_ONLY for every C files. Compile very C file, together
// with this impl.c (compiled *without* macro CKB_DECLARATION_ONLY) and then
// link them together.
//

#ifndef __CKB_IMPL_INCLUDED__
#define __CKB_IMPL_INCLUDED__

#include <internal/atomic.h>
#include <internal/types.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <locale.h>
#include <setjmp.h>

/*
 * The implementation here is based on musl-libc with modifications for our
 * use case. The original musl-libc follows MIT license, thanks to the authors
 * for the creation.
 */

void *memset(void *dest, int c, size_t n) {
  unsigned char *s = dest;
#ifndef __OPTIMIZE_SIZE__
  size_t k;

  /* Fill head and tail with minimal branching. Each
   * conditional ensures that all the subsequently used
   * offsets are well-defined and in the dest region. */

  if (!n) return dest;
  s[0] = c;
  s[n - 1] = c;
  if (n <= 2) return dest;
  s[1] = c;
  s[2] = c;
  s[n - 2] = c;
  s[n - 3] = c;
  if (n <= 6) return dest;
  s[3] = c;
  s[n - 4] = c;
  if (n <= 8) return dest;

  /* Advance pointer to align it at a 4-byte boundary,
   * and truncate n to a multiple of 4. The previous code
   * already took care of any head/tail that get cut off
   * by the alignment. */

  k = -(uintptr_t)s & 3;
  s += k;
  n -= k;
  n &= -4;

#ifdef __GNUC__
  typedef uint32_t __attribute__((__may_alias__)) u32;
  typedef uint64_t __attribute__((__may_alias__)) u64;

  u32 c32 = ((u32)-1) / 255 * (unsigned char)c;

  /* In preparation to copy 32 bytes at a time, aligned on
   * an 8-byte bounary, fill head/tail up to 28 bytes each.
   * As in the initial byte-based head/tail fill, each
   * conditional below ensures that the subsequent offsets
   * are valid (e.g. !(n<=24) implies n>=28). */

  *(u32 *)(s + 0) = c32;
  *(u32 *)(s + n - 4) = c32;
  if (n <= 8) return dest;
  *(u32 *)(s + 4) = c32;
  *(u32 *)(s + 8) = c32;
  *(u32 *)(s + n - 12) = c32;
  *(u32 *)(s + n - 8) = c32;
  if (n <= 24) return dest;
  *(u32 *)(s + 12) = c32;
  *(u32 *)(s + 16) = c32;
  *(u32 *)(s + 20) = c32;
  *(u32 *)(s + 24) = c32;
  *(u32 *)(s + n - 28) = c32;
  *(u32 *)(s + n - 24) = c32;
  *(u32 *)(s + n - 20) = c32;
  *(u32 *)(s + n - 16) = c32;

  /* Align to a multiple of 8 so we can fill 64 bits at a time,
   * and avoid writing the same bytes twice as much as is
   * practical without introducing additional branching. */

  k = 24 + ((uintptr_t)s & 4);
  s += k;
  n -= k;

  /* If this loop is reached, 28 tail bytes have already been
   * filled, so any remainder when n drops below 32 can be
   * safely ignored. */

  u64 c64 = c32 | ((u64)c32 << 32);
  for (; n >= 32; n -= 32, s += 32) {
    *(u64 *)(s + 0) = c64;
    *(u64 *)(s + 8) = c64;
    *(u64 *)(s + 16) = c64;
    *(u64 *)(s + 24) = c64;
  }
#else
  /* Pure C fallback with no aliasing violations. */
  for (; n; n--, s++) *s = c;
#endif

#else
  for (; n; n--, s++) *s = c;
#endif /* __OPTIMIZE_SIZE__ */

  return dest;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;

#if defined(__GNUC__) && (!defined(__OPTIMIZE_SIZE__))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define LS >>
#define RS <<
#else
#define LS <<
#define RS >>
#endif

  typedef uint32_t __attribute__((__may_alias__)) u32;
  uint32_t w, x;

  for (; (uintptr_t)s % 4 && n; n--) *d++ = *s++;

  if ((uintptr_t)d % 4 == 0) {
    for (; n >= 16; s += 16, d += 16, n -= 16) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      *(u32 *)(d + 4) = *(u32 *)(s + 4);
      *(u32 *)(d + 8) = *(u32 *)(s + 8);
      *(u32 *)(d + 12) = *(u32 *)(s + 12);
    }
    if (n & 8) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);

      *(u32 *)(d + 4) = *(u32 *)(s + 4);
      d += 8;
      s += 8;
    }
    if (n & 4) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      d += 4;
      s += 4;
    }
    if (n & 2) {
      *d++ = *s++;
      *d++ = *s++;
    }
    if (n & 1) {
      *d = *s;
    }
    return dest;
  }

  if (n >= 32) switch ((uintptr_t)d % 4) {
      case 1:
        w = *(u32 *)s;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        n -= 3;
        for (; n >= 17; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 1);
          *(u32 *)(d + 0) = (w LS 24) | (x RS 8);
          w = *(u32 *)(s + 5);
          *(u32 *)(d + 4) = (x LS 24) | (w RS 8);
          x = *(u32 *)(s + 9);
          *(u32 *)(d + 8) = (w LS 24) | (x RS 8);
          w = *(u32 *)(s + 13);
          *(u32 *)(d + 12) = (x LS 24) | (w RS 8);
        }
        break;
      case 2:
        w = *(u32 *)s;
        *d++ = *s++;
        *d++ = *s++;
        n -= 2;
        for (; n >= 18; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 2);
          *(u32 *)(d + 0) = (w LS 16) | (x RS 16);
          w = *(u32 *)(s + 6);
          *(u32 *)(d + 4) = (x LS 16) | (w RS 16);
          x = *(u32 *)(s + 10);
          *(u32 *)(d + 8) = (w LS 16) | (x RS 16);
          w = *(u32 *)(s + 14);
          *(u32 *)(d + 12) = (x LS 16) | (w RS 16);
        }
        break;
      case 3:
        w = *(u32 *)s;
        *d++ = *s++;
        n -= 1;
        for (; n >= 19; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 3);
          *(u32 *)(d + 0) = (w LS 8) | (x RS 24);
          w = *(u32 *)(s + 7);
          *(u32 *)(d + 4) = (x LS 8) | (w RS 24);
          x = *(u32 *)(s + 11);
          *(u32 *)(d + 8) = (w LS 8) | (x RS 24);
          w = *(u32 *)(s + 15);
          *(u32 *)(d + 12) = (x LS 8) | (w RS 24);
        }
        break;
    }
  if (n & 16) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 8) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 4) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 2) {
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 1) {
    *d = *s;
  }
  return dest;
#endif

  for (; n; n--) *d++ = *s++;
  return dest;
}

int memcmp(const void *vl, const void *vr, size_t n) {
  const unsigned char *l = vl, *r = vr;
  for (; n && *l == *r; n--, l++, r++)
    ;
  return n ? *l - *r : 0;
}

#ifdef __GNUC__
typedef __attribute__((__may_alias__)) size_t WT;
#define WS (sizeof(WT))
#endif

void *memmove(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;

  if (d == s) return d;
  if ((uintptr_t)s - (uintptr_t)d - n <= -2 * n) return memcpy(d, s, n);

  if (d < s) {
#ifdef __GNUC__
    if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
      while ((uintptr_t)d % WS) {
        if (!n--) return dest;
        *d++ = *s++;
      }
      for (; n >= WS; n -= WS, d += WS, s += WS) *(WT *)d = *(WT *)s;
    }
#endif
    for (; n; n--) *d++ = *s++;
  } else {
#ifdef __GNUC__
    if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
      while ((uintptr_t)(d + n) % WS) {
        if (!n--) return dest;
        d[n] = s[n];
      }
      while (n >= WS) n -= WS, *(WT *)(d + n) = *(WT *)(s + n);
    }
#endif
    while (n) n--, d[n] = s[n];
  }

  return dest;
}

#define SS (sizeof(size_t))
#define ALIGN (sizeof(size_t)-1)
#define ONES ((size_t)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) (((x)-ONES) & ~(x) & HIGHS)

void *memchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	c = (unsigned char)c;
#ifdef __GNUC__
	for (; ((uintptr_t)s & ALIGN) && n && *s != c; s++, n--);
	if (n && *s != c) {
		typedef size_t __attribute__((__may_alias__)) word;
		const word *w;
		size_t k = ONES * c;
		for (w = (const void *)s; n>=SS && !HASZERO(*w^k); w++, n-=SS);
		s = (const void *)w;
	}
#endif
	for (; n && *s != c; s++, n--);
	return n ? (void *)s : 0;
}

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

char *strcpy(char *restrict d, const char *restrict s) {
  char *dest = d;
  for (; (*d = *s); s++, d++)
    ;
  return dest;
}

size_t strlen(const char *s) {
  const char *a = s;
  for (; *s; s++)
    ;
  return s - a;
}

int strcmp(const char *l, const char *r) {
  for (; *l == *r && *l; l++, r++)
    ;
  return *(unsigned char *)l - *(unsigned char *)r;
}

char *__strchrnul(const char *s, int c) {
	c = (unsigned char)c;
	if (!c) return (char *)s + strlen(s);

	for (; *s && *(unsigned char *)s != c; s++);
	return (char *)s;
}

char *strchr(const char *s, int c) {
	char *r = __strchrnul(s, c);
	return *(unsigned char *)r == (unsigned char)c ? r : 0;
}

int strncmp(const char *_l, const char *_r, size_t n) {
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	if (!n--) return 0;
	for (; *l && *r && n && *l == *r ; l++, r++, n--);
	return *l - *r;
}

size_t strspn(const char *s, const char *c)
{
	const char *a = s;
	size_t byteset[32/sizeof(size_t)] = { 0 };

	if (!c[0]) return 0;
	if (!c[1]) {
		for (; *s == *c; s++);
		return s-a;
	}

	for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++);
	for (; *s && BITOP(byteset, *(unsigned char *)s, &); s++);
	return s-a;
}

size_t strcspn(const char *s, const char *c)
{
	const char *a = s;
	size_t byteset[32/sizeof(size_t)];

	if (!c[0] || !c[1]) return __strchrnul(s, *c)-a;

	memset(byteset, 0, sizeof byteset);
	for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++);
	for (; *s && !BITOP(byteset, *(unsigned char *)s, &); s++);
	return s-a;
}

char *strpbrk(const char *s, const char *b)
{
	s += strcspn(s, b);
	return *s ? (char *)s : 0;
}

static char *twobyte_strstr(const unsigned char *h, const unsigned char *n)
{
	uint16_t nw = n[0]<<8 | n[1], hw = h[0]<<8 | h[1];
	for (h++; *h && hw != nw; hw = hw<<8 | *++h);
	return *h ? (char *)h-1 : 0;
}

static char *threebyte_strstr(const unsigned char *h, const unsigned char *n)
{
	uint32_t nw = (uint32_t)n[0]<<24 | n[1]<<16 | n[2]<<8;
	uint32_t hw = (uint32_t)h[0]<<24 | h[1]<<16 | h[2]<<8;
	for (h+=2; *h && hw != nw; hw = (hw|*++h)<<8);
	return *h ? (char *)h-2 : 0;
}

static char *fourbyte_strstr(const unsigned char *h, const unsigned char *n)
{
	uint32_t nw = (uint32_t)n[0]<<24 | n[1]<<16 | n[2]<<8 | n[3];
	uint32_t hw = (uint32_t)h[0]<<24 | h[1]<<16 | h[2]<<8 | h[3];
	for (h+=3; *h && hw != nw; hw = hw<<8 | *++h);
	return *h ? (char *)h-3 : 0;
}

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

static char *twoway_strstr(const unsigned char *h, const unsigned char *n)
{
	const unsigned char *z;
	size_t l, ip, jp, k, p, ms, p0, mem, mem0;
	size_t byteset[32 / sizeof(size_t)] = { 0 };
	size_t shift[256];

	/* Computing length of needle and fill shift table */
	for (l=0; n[l] && h[l]; l++)
		BITOP(byteset, n[l], |=), shift[n[l]] = l+1;
	if (n[l]) return 0; /* hit the end of h */

	/* Compute maximal suffix */
	ip = -1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] > n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	ms = ip;
	p0 = p;

	/* And with the opposite comparison */
	ip = -1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] < n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	if (ip+1 > ms+1) ms = ip;
	else p = p0;

	/* Periodic needle? */
	if (memcmp(n, n+p, ms+1)) {
		mem0 = 0;
		p = MAX(ms, l-ms-1) + 1;
	} else mem0 = l-p;
	mem = 0;

	/* Initialize incremental end-of-haystack pointer */
	z = h;

	/* Search loop */
	for (;;) {
		/* Update incremental end-of-haystack pointer */
		if (z-h < l) {
			/* Fast estimate for MAX(l,63) */
			size_t grow = l | 63;
			const unsigned char *z2 = memchr(z, 0, grow);
			if (z2) {
				z = z2;
				if (z-h < l) return 0;
			} else z += grow;
		}

		/* Check last byte first; advance by shift on mismatch */
		if (BITOP(byteset, h[l-1], &)) {
			k = l-shift[h[l-1]];
			if (k) {
				if (k < mem) k = mem;
				h += k;
				mem = 0;
				continue;
			}
		} else {
			h += l;
			mem = 0;
			continue;
		}

		/* Compare right half */
		for (k=MAX(ms+1,mem); n[k] && n[k] == h[k]; k++);
		if (n[k]) {
			h += k-ms;
			mem = 0;
			continue;
		}
		/* Compare left half */
		for (k=ms+1; k>mem && n[k-1] == h[k-1]; k--);
		if (k <= mem) return (char *)h;
		h += p;
		mem = mem0;
	}
}

char *strstr(const char *h, const char *n)
{
	/* Return immediately on empty needle */
	if (!n[0]) return (char *)h;

	/* Use faster algorithms for short needles */
	h = strchr(h, *n);
	if (!h || !n[1]) return (char *)h;
	if (!h[1]) return 0;
	if (!n[2]) return twobyte_strstr((void *)h, (void *)n);
	if (!h[2]) return 0;
	if (!n[3]) return threebyte_strstr((void *)h, (void *)n);
	if (!h[3]) return 0;
	if (!n[4]) return fourbyte_strstr((void *)h, (void *)n);

	return twoway_strstr((void *)h, (void *)n);
}

static const struct lconv posix_lconv = {
        .decimal_point = ".",
        .thousands_sep = "",
        .grouping = "",
        .int_curr_symbol = "",
        .currency_symbol = "",
        .mon_decimal_point = "",
        .mon_thousands_sep = "",
        .mon_grouping = "",
        .positive_sign = "",
        .negative_sign = "",
        .int_frac_digits = CHAR_MAX,
        .frac_digits = CHAR_MAX,
        .p_cs_precedes = CHAR_MAX,
        .p_sep_by_space = CHAR_MAX,
        .n_cs_precedes = CHAR_MAX,
        .n_sep_by_space = CHAR_MAX,
        .p_sign_posn = CHAR_MAX,
        .n_sign_posn = CHAR_MAX,
        .int_p_cs_precedes = CHAR_MAX,
        .int_p_sep_by_space = CHAR_MAX,
        .int_n_cs_precedes = CHAR_MAX,
        .int_n_sep_by_space = CHAR_MAX,
        .int_p_sign_posn = CHAR_MAX,
        .int_n_sign_posn = CHAR_MAX,
};

struct lconv *localeconv(void)
{
        return (void *)&posix_lconv;
}

#ifdef CKB_C_STDLIB_MALLOC
#include "malloc_impl.h"
#else
void *malloc(size_t size) { return NULL; }
void free(void *ptr) {}
void *calloc(size_t nmemb, size_t size) { return NULL; }
void *realloc(void *ptr, size_t size) { return NULL; }
#endif

/*
 * qsort implementation below is modified from
 * https://git.musl-libc.org/cgit/musl/tree/src/stdlib/qsort.c?id=33338ebc853d37c80f0f236cc7a92cb0acc6aace
 */
/* Copyright (C) 2011 by Valentin Ochs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Minor changes by Rich Felker for integration in musl, 2011-04-27. */

/* Smoothsort, an adaptive variant of Heapsort.  Memory usage: O(1).
   Run time: Worst case O(n log n), close to O(n) in the mostly-sorted case. */

#define ntz(x) a_ctz_l((x))

typedef int (*cmpfun)(const void *, const void *);

static inline int pntz(size_t p[2]) {
  int r = ntz(p[0] - 1);
  if (r != 0 || (r = 8 * sizeof(size_t) + ntz(p[1])) != 8 * sizeof(size_t)) {
    return r;
  }
  return 0;
}

static void cycle(size_t width, unsigned char *ar[], int n) {
  unsigned char tmp[256];
  size_t l;
  int i;

  if (n < 2) {
    return;
  }

  ar[n] = tmp;
  while (width) {
    l = sizeof(tmp) < width ? sizeof(tmp) : width;
    memcpy(ar[n], ar[0], l);
    for (i = 0; i < n; i++) {
      memcpy(ar[i], ar[i + 1], l);
      ar[i] += l;
    }
    width -= l;
  }
}

/* shl() and shr() need n > 0 */
static inline void shl(size_t p[2], int n) {
  if (n >= (int)(8 * sizeof(size_t))) {
    n -= 8 * sizeof(size_t);
    p[1] = p[0];
    p[0] = 0;
  }
  p[1] <<= n;
  p[1] |= p[0] >> (sizeof(size_t) * 8 - n);
  p[0] <<= n;
}

static inline void shr(size_t p[2], int n) {
  if (n >= (int)(8 * sizeof(size_t))) {
    n -= 8 * sizeof(size_t);
    p[0] = p[1];
    p[1] = 0;
  }
  p[0] >>= n;
  p[0] |= p[1] << (sizeof(size_t) * 8 - n);
  p[1] >>= n;
}

static void sift(unsigned char *head, size_t width, cmpfun cmp, int pshift,
                 size_t lp[]) {
  unsigned char *rt, *lf;
  unsigned char *ar[14 * sizeof(size_t) + 1];
  int i = 1;

  ar[0] = head;
  while (pshift > 1) {
    rt = head - width;
    lf = head - width - lp[pshift - 2];

    if ((*cmp)(ar[0], lf) >= 0 && (*cmp)(ar[0], rt) >= 0) {
      break;
    }
    if ((*cmp)(lf, rt) >= 0) {
      ar[i++] = lf;
      head = lf;
      pshift -= 1;
    } else {
      ar[i++] = rt;
      head = rt;
      pshift -= 2;
    }
  }
  cycle(width, ar, i);
}

static void trinkle(unsigned char *head, size_t width, cmpfun cmp, size_t pp[2],
                    int pshift, int trusty, size_t lp[]) {
  unsigned char *stepson, *rt, *lf;
  size_t p[2];
  unsigned char *ar[14 * sizeof(size_t) + 1];
  int i = 1;
  int trail;

  p[0] = pp[0];
  p[1] = pp[1];

  ar[0] = head;
  while (p[0] != 1 || p[1] != 0) {
    stepson = head - lp[pshift];
    if ((*cmp)(stepson, ar[0]) <= 0) {
      break;
    }
    if (!trusty && pshift > 1) {
      rt = head - width;
      lf = head - width - lp[pshift - 2];
      if ((*cmp)(rt, stepson) >= 0 || (*cmp)(lf, stepson) >= 0) {
        break;
      }
    }

    ar[i++] = stepson;
    head = stepson;
    trail = pntz(p);
    shr(p, trail);
    pshift += trail;
    trusty = 0;
  }
  if (!trusty) {
    cycle(width, ar, i);
    sift(head, width, cmp, pshift, lp);
  }
}

void qsort(void *base, size_t nel, size_t width, cmpfun cmp) {
  size_t lp[12 * sizeof(size_t)];
  size_t i, size = width * nel;
  unsigned char *head, *high;
  size_t p[2] = {1, 0};
  int pshift = 1;
  int trail;

  if (!size) return;

  head = base;
  high = head + size - width;

  /* Precompute Leonardo numbers, scaled by element width */
  for (lp[0] = lp[1] = width, i = 2;
       (lp[i] = lp[i - 2] + lp[i - 1] + width) < size; i++)
    ;

  while (head < high) {
    if ((p[0] & 3) == 3) {
      sift(head, width, cmp, pshift, lp);
      shr(p, 2);
      pshift += 2;
    } else {
      if (lp[pshift - 1] >= (size_t)(high - head)) {
        trinkle(head, width, cmp, p, pshift, 0, lp);
      } else {
        sift(head, width, cmp, pshift, lp);
      }

      if (pshift == 1) {
        shl(p, 1);
        pshift = 0;
      } else {
        shl(p, pshift - 1);
        pshift = 1;
      }
    }

    p[0] |= 1;
    head += width;
  }

  trinkle(head, width, cmp, p, pshift, 0, lp);

  while (pshift != 1 || p[0] != 1 || p[1] != 0) {
    if (pshift <= 1) {
      trail = pntz(p);
      shr(p, trail);
      pshift += trail;
    } else {
      shl(p, 2);
      pshift -= 2;
      p[0] ^= 7;
      shr(p, 1);
      trinkle(head - lp[pshift] - width, width, cmp, p, pshift + 1, 1, lp);
      shl(p, 1);
      p[0] |= 1;
      trinkle(head - width, width, cmp, p, pshift, 1, lp);
    }
    head -= width;
  }
}

//  bsearch implementation below is modified from
// http://git.musl-libc.org/cgit/musl/tree/src/stdlib/bsearch.c
void *bsearch(const void *key, const void *base, size_t nel, size_t width,
              int (*cmp)(const void *, const void *)) {
  void *try_v;
  int sign;
  while (nel > 0) {
    try_v = (char *)base + width * (nel / 2);
    sign = cmp(key, try_v);
    if (sign < 0) {
      nel /= 2;
    } else if (sign > 0) {
      base = (char *)try_v + width;
      nel -= nel / 2 + 1;
    } else {
      return try_v;
    }
  }
  return NULL;
}

// printf, pass -D CKB_C_STDLIB_PRINTF to enable printf
// default: disabled
#ifdef CKB_C_STDLIB_PRINTF

// Default PRINTF_BUFFER_SIZE
#ifndef CKB_C_STDLIB_PRINTF_BUFFER_SIZE
#define CKB_C_STDLIB_PRINTF_BUFFER_SIZE 256
#endif

// define this globally (e.g. gcc -DPRINTF_INCLUDE_CONFIG_H ...) to include the
// printf_config.h header file
// default: undefined
#ifdef PRINTF_INCLUDE_CONFIG_H
#include "printf_config.h"
#endif

// 'ntoa' conversion buffer size, this must be big enough to hold one converted
// numeric number including padded zeros (dynamically created on stack)
// default: 32 byte
#ifndef PRINTF_NTOA_BUFFER_SIZE
#define PRINTF_NTOA_BUFFER_SIZE 32U
#endif

// 'ftoa' conversion buffer size, this must be big enough to hold one converted
// float number including padded zeros (dynamically created on stack)
// default: 32 byte
#ifndef PRINTF_FTOA_BUFFER_SIZE
#define PRINTF_FTOA_BUFFER_SIZE 32U
#endif

// support for the floating point type (%f)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_FLOAT
#define PRINTF_SUPPORT_FLOAT
#endif

// support for exponential floating point notation (%e/%g)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_EXPONENTIAL
#define PRINTF_SUPPORT_EXPONENTIAL
#endif

// define the default floating point precision
// default: 6 digits
#ifndef PRINTF_DEFAULT_FLOAT_PRECISION
#define PRINTF_DEFAULT_FLOAT_PRECISION 6U
#endif

// define the largest float suitable to print with %f
// default: 1e9
#ifndef PRINTF_MAX_FLOAT
#define PRINTF_MAX_FLOAT 1e9
#endif

// support for the long long types (%llu or %p)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_LONG_LONG
#define PRINTF_SUPPORT_LONG_LONG
#endif

// support for the ptrdiff_t type (%t)
// ptrdiff_t is normally defined in <stddef.h> as long or long long type
// default: disabled
#ifndef PRINTF_DISABLE_SUPPORT_PTRDIFF_T
#endif

///////////////////////////////////////////////////////////////////////////////

// internal flag definitions
#define FLAGS_ZEROPAD (1U << 0U)
#define FLAGS_LEFT (1U << 1U)
#define FLAGS_PLUS (1U << 2U)
#define FLAGS_SPACE (1U << 3U)
#define FLAGS_HASH (1U << 4U)
#define FLAGS_UPPERCASE (1U << 5U)
#define FLAGS_CHAR (1U << 6U)
#define FLAGS_SHORT (1U << 7U)
#define FLAGS_LONG (1U << 8U)
#define FLAGS_LONG_LONG (1U << 9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)

// output function type
typedef void (*out_fct_type)(char character, void *buffer, size_t idx,
                             size_t maxlen);

// wrapper (used as buffer) for output function type
typedef struct {
  void (*fct)(char character, void *arg);
  void *arg;
} out_fct_wrap_type;

// internal buffer output
static inline void _out_buffer(char character, void *buffer, size_t idx,
                               size_t maxlen) {
  if (idx < maxlen) {
    ((char *)buffer)[idx] = character;
  }
}

// internal null output
static inline void _out_null(char character, void *buffer, size_t idx,
                             size_t maxlen) {
  (void)character;
  (void)buffer;
  (void)idx;
  (void)maxlen;
}

// internal output function wrapper
static inline void _out_fct(char character, void *buffer, size_t idx,
                            size_t maxlen) {
  (void)idx;
  (void)maxlen;
  if (character) {
    // buffer is the output fct pointer
    ((out_fct_wrap_type *)buffer)
        ->fct(character, ((out_fct_wrap_type *)buffer)->arg);
  }
}

// internal secure strlen
// \return The length of the string (excluding the terminating 0) limited by
// 'maxsize'
static inline unsigned int _strnlen_s(const char *str, size_t maxsize) {
  const char *s;
  for (s = str; *s && maxsize--; ++s)
    ;
  return (unsigned int)(s - str);
}

// internal test if char is a digit (0-9)
// \return true if char is a digit
static inline bool _is_digit(char ch) { return (ch >= '0') && (ch <= '9'); }

// internal ASCII string to unsigned int conversion
static unsigned int _atoi(const char **str) {
  unsigned int i = 0U;
  while (_is_digit(**str)) {
    i = i * 10U + (unsigned int)(*((*str)++) - '0');
  }
  return i;
}

// output the specified string in reverse, taking care of any zero-padding
static size_t _out_rev(out_fct_type out, char *buffer, size_t idx,
                       size_t maxlen, const char *buf, size_t len,
                       unsigned int width, unsigned int flags) {
  const size_t start_idx = idx;

  // pad spaces up to given width
  if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
    for (size_t i = len; i < width; i++) {
      out(' ', buffer, idx++, maxlen);
    }
  }

  // reverse string
  while (len) {
    out(buf[--len], buffer, idx++, maxlen);
  }

  // append pad spaces up to given width
  if (flags & FLAGS_LEFT) {
    while (idx - start_idx < width) {
      out(' ', buffer, idx++, maxlen);
    }
  }

  return idx;
}

// internal itoa format
static size_t _ntoa_format(out_fct_type out, char *buffer, size_t idx,
                           size_t maxlen, char *buf, size_t len, bool negative,
                           unsigned int base, unsigned int prec,
                           unsigned int width, unsigned int flags) {
  // pad leading zeros
  if (!(flags & FLAGS_LEFT)) {
    if (width && (flags & FLAGS_ZEROPAD) &&
        (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
      width--;
    }
    while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }
    while ((flags & FLAGS_ZEROPAD) && (len < width) &&
           (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }
  }

  // handle hash
  if (flags & FLAGS_HASH) {
    if (!(flags & FLAGS_PRECISION) && len &&
        ((len == prec) || (len == width))) {
      len--;
      if (len && (base == 16U)) {
        len--;
      }
    }
    if ((base == 16U) && !(flags & FLAGS_UPPERCASE) &&
        (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'x';
    } else if ((base == 16U) && (flags & FLAGS_UPPERCASE) &&
               (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'X';
    } else if ((base == 2U) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'b';
    }
    if (len < PRINTF_NTOA_BUFFER_SIZE) {
      buf[len++] = '0';
    }
  }

  if (len < PRINTF_NTOA_BUFFER_SIZE) {
    if (negative) {
      buf[len++] = '-';
    } else if (flags & FLAGS_PLUS) {
      buf[len++] = '+';  // ignore the space if the '+' exists
    } else if (flags & FLAGS_SPACE) {
      buf[len++] = ' ';
    }
  }

  return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}

// internal itoa for 'long' type
static size_t _ntoa_long(out_fct_type out, char *buffer, size_t idx,
                         size_t maxlen, unsigned long value, bool negative,
                         unsigned long base, unsigned int prec,
                         unsigned int width, unsigned int flags) {
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;

  // no hash for 0 values
  if (!value) {
    flags &= ~FLAGS_HASH;
  }

  // write if precision != 0 and value is != 0
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10
                       ? '0' + digit
                       : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }

  return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative,
                      (unsigned int)base, prec, width, flags);
}

// internal itoa for 'long long' type
#if defined(PRINTF_SUPPORT_LONG_LONG)
static size_t _ntoa_long_long(out_fct_type out, char *buffer, size_t idx,
                              size_t maxlen, unsigned long long value,
                              bool negative, unsigned long long base,
                              unsigned int prec, unsigned int width,
                              unsigned int flags) {
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;

  // no hash for 0 values
  if (!value) {
    flags &= ~FLAGS_HASH;
  }

  // write if precision != 0 and value is != 0
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10
                       ? '0' + digit
                       : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }

  return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative,
                      (unsigned int)base, prec, width, flags);
}
#endif  // PRINTF_SUPPORT_LONG_LONG

// internal vsnprintf
static int _vsnprintf(out_fct_type out, char *buffer, const size_t maxlen,
                      const char *format, va_list va) {
  unsigned int flags, width, precision, n;
  size_t idx = 0U;

  if (!buffer) {
    // use null output function
    out = _out_null;
  }

  while (*format) {
    // format specifier?  %[flags][width][.precision][length]
    if (*format != '%') {
      // no
      out(*format, buffer, idx++, maxlen);
      format++;
      continue;
    } else {
      // yes, evaluate it
      format++;
    }

    // evaluate flags
    flags = 0U;
    do {
      switch (*format) {
        case '0':
          flags |= FLAGS_ZEROPAD;
          format++;
          n = 1U;
          break;
        case '-':
          flags |= FLAGS_LEFT;
          format++;
          n = 1U;
          break;
        case '+':
          flags |= FLAGS_PLUS;
          format++;
          n = 1U;
          break;
        case ' ':
          flags |= FLAGS_SPACE;
          format++;
          n = 1U;
          break;
        case '#':
          flags |= FLAGS_HASH;
          format++;
          n = 1U;
          break;
        default:
          n = 0U;
          break;
      }
    } while (n);

    // evaluate width field
    width = 0U;
    if (_is_digit(*format)) {
      width = _atoi(&format);
    } else if (*format == '*') {
      const int w = va_arg(va, int);
      if (w < 0) {
        flags |= FLAGS_LEFT;  // reverse padding
        width = (unsigned int)-w;
      } else {
        width = (unsigned int)w;
      }
      format++;
    }

    // evaluate precision field
    precision = 0U;
    if (*format == '.') {
      flags |= FLAGS_PRECISION;
      format++;
      if (_is_digit(*format)) {
        precision = _atoi(&format);
      } else if (*format == '*') {
        const int prec = (int)va_arg(va, int);
        precision = prec > 0 ? (unsigned int)prec : 0U;
        format++;
      }
    }

    // evaluate length field
    switch (*format) {
      case 'l':
        flags |= FLAGS_LONG;
        format++;
        if (*format == 'l') {
          flags |= FLAGS_LONG_LONG;
          format++;
        }
        break;
      case 'h':
        flags |= FLAGS_SHORT;
        format++;
        if (*format == 'h') {
          flags |= FLAGS_CHAR;
          format++;
        }
        break;
#if defined(PRINTF_SUPPORT_PTRDIFF_T)
      case 't':
        flags |=
            (sizeof(ptrdiff_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
#endif
      case 'j':
        flags |=
            (sizeof(uint64_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
      case 'z':
        flags |=
            (sizeof(size_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
      default:
        break;
    }

    // evaluate specifier
    switch (*format) {
      case 'd':
      case 'i':
      case 'u':
      case 'x':
      case 'X':
      case 'o':
      case 'b': {
        // set the base
        unsigned int base;
        if (*format == 'x' || *format == 'X') {
          base = 16U;
        } else if (*format == 'o') {
          base = 8U;
        } else if (*format == 'b') {
          base = 2U;
        } else {
          base = 10U;
          flags &= ~FLAGS_HASH;  // no hash for dec format
        }
        // uppercase
        if (*format == 'X') {
          flags |= FLAGS_UPPERCASE;
        }

        // no plus or space flag for u, x, X, o, b
        if ((*format != 'i') && (*format != 'd')) {
          flags &= ~(FLAGS_PLUS | FLAGS_SPACE);
        }

        // ignore '0' flag when precision is given
        if (flags & FLAGS_PRECISION) {
          flags &= ~FLAGS_ZEROPAD;
        }

        // convert the integer
        if ((*format == 'i') || (*format == 'd')) {
          // signed
          if (flags & FLAGS_LONG_LONG) {
#if defined(PRINTF_SUPPORT_LONG_LONG)
            const long long value = va_arg(va, long long);
            idx = _ntoa_long_long(
                out, buffer, idx, maxlen,
                (unsigned long long)(value > 0 ? value : 0 - value), value < 0,
                base, precision, width, flags);
#endif
          } else if (flags & FLAGS_LONG) {
            const long value = va_arg(va, long);
            idx = _ntoa_long(out, buffer, idx, maxlen,
                             (unsigned long)(value > 0 ? value : 0 - value),
                             value < 0, base, precision, width, flags);
          } else {
            const int value = (flags & FLAGS_CHAR)
                                  ? (char)va_arg(va, int)
                                  : (flags & FLAGS_SHORT)
                                        ? (short int)va_arg(va, int)
                                        : va_arg(va, int);
            idx = _ntoa_long(out, buffer, idx, maxlen,
                             (unsigned int)(value > 0 ? value : 0 - value),
                             value < 0, base, precision, width, flags);
          }
        } else {
          // unsigned
          if (flags & FLAGS_LONG_LONG) {
#if defined(PRINTF_SUPPORT_LONG_LONG)
            idx = _ntoa_long_long(out, buffer, idx, maxlen,
                                  va_arg(va, unsigned long long), false, base,
                                  precision, width, flags);
#endif
          } else if (flags & FLAGS_LONG) {
            idx =
                _ntoa_long(out, buffer, idx, maxlen, va_arg(va, unsigned long),
                           false, base, precision, width, flags);
          } else {
            const unsigned int value =
                (flags & FLAGS_CHAR)
                    ? (unsigned char)va_arg(va, unsigned int)
                    : (flags & FLAGS_SHORT)
                          ? (unsigned short int)va_arg(va, unsigned int)
                          : va_arg(va, unsigned int);
            idx = _ntoa_long(out, buffer, idx, maxlen, value, false, base,
                             precision, width, flags);
          }
        }
        format++;
        break;
      }
      case 'c': {
        unsigned int l = 1U;
        // pre padding
        if (!(flags & FLAGS_LEFT)) {
          while (l++ < width) {
            out(' ', buffer, idx++, maxlen);
          }
        }
        // char output
        out((char)va_arg(va, int), buffer, idx++, maxlen);
        // post padding
        if (flags & FLAGS_LEFT) {
          while (l++ < width) {
            out(' ', buffer, idx++, maxlen);
          }
        }
        format++;
        break;
      }

      case 's': {
        const char *p = va_arg(va, char *);
        unsigned int l = _strnlen_s(p, precision ? precision : (size_t)-1);
        // pre padding
        if (flags & FLAGS_PRECISION) {
          l = (l < precision ? l : precision);
        }
        if (!(flags & FLAGS_LEFT)) {
          while (l++ < width) {
            out(' ', buffer, idx++, maxlen);
          }
        }
        // string output
        while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
          out(*(p++), buffer, idx++, maxlen);
        }
        // post padding
        if (flags & FLAGS_LEFT) {
          while (l++ < width) {
            out(' ', buffer, idx++, maxlen);
          }
        }
        format++;
        break;
      }

      case 'p': {
        width = sizeof(void *) * 2U;
        flags |= FLAGS_ZEROPAD | FLAGS_UPPERCASE;
#if defined(PRINTF_SUPPORT_LONG_LONG)
        const bool is_ll = sizeof(uintptr_t) == sizeof(long long);
        if (is_ll) {
          idx = _ntoa_long_long(out, buffer, idx, maxlen,
                                (uintptr_t)va_arg(va, void *), false, 16U,
                                precision, width, flags);
        } else {
#endif
          idx = _ntoa_long(out, buffer, idx, maxlen,
                           (unsigned long)((uintptr_t)va_arg(va, void *)),
                           false, 16U, precision, width, flags);
#if defined(PRINTF_SUPPORT_LONG_LONG)
        }
#endif
        format++;
        break;
      }

      case '%':
        out('%', buffer, idx++, maxlen);
        format++;
        break;

      default:
        out(*format, buffer, idx++, maxlen);
        format++;
        break;
    }
  }

  // termination
  out((char)0, buffer, idx < maxlen ? idx : maxlen - 1U, maxlen);

  // return written chars without terminating \0
  return (int)idx;
}

///////////////////////////////////////////////////////////////////////////////

int sprintf_(char *buffer, const char *format, ...) {
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(_out_buffer, buffer, (size_t)-1, format, va);
  va_end(va);
  return ret;
}

int snprintf_(char *buffer, size_t count, const char *format, ...) {
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(_out_buffer, buffer, count, format, va);
  va_end(va);
  return ret;
}

int vsnprintf_(char *buffer, size_t count, const char *format, va_list va) {
  return _vsnprintf(_out_buffer, buffer, count, format, va);
}

int fctprintf(void (*out)(char character, void *arg), void *arg,
              const char *format, ...) {
  va_list va;
  va_start(va, format);
  const out_fct_wrap_type out_fct_wrap = {out, arg};
  const int ret = _vsnprintf(_out_fct, (char *)(uintptr_t)&out_fct_wrap,
                             (size_t)-1, format, va);
  va_end(va);
  return ret;
}

// syscall
int ckb_debug(const char *s);
int printf(const char *format, ...) {
  static char buf[CKB_C_STDLIB_PRINTF_BUFFER_SIZE];
  va_list va;
  va_start(va, format);
  int ret = vsnprintf_(buf, CKB_C_STDLIB_PRINTF_BUFFER_SIZE, format, va);
  va_end(va);
  ckb_debug(buf);
  return ret;
}
int ckb_printf(const char *format, ...) {
  static char buf[CKB_C_STDLIB_PRINTF_BUFFER_SIZE];
  va_list va;
  va_start(va, format);
  int ret = vsnprintf_(buf, CKB_C_STDLIB_PRINTF_BUFFER_SIZE, format, va);
  va_end(va);
  ckb_debug(buf);
  return ret;
}

#else

int printf(const char *format, ...) { return 0; }
int ckb_printf(const char *format, ...) { return 0; }

#endif /* CKB_C_STDLIB_PRINTF */

/* Copied from https://github.com/bminor/musl/blob/46d1c7801bb509e1097e8fadbaf359367fa4ef0b/src/setjmp/riscv64/setjmp.S */
/* We need to use inline asm for easier compilation, https://stackoverflow.com/a/42358235. */
/* We need __attribute__((naked)) to remove prolog and epilog, https://stackoverflow.com/a/42637729 */
int setjmp(jmp_buf b) {
  asm volatile("sd s0,    0(a0)\n"
               "sd s1,    8(a0)\n"
               "sd s2,    16(a0)\n"
               "sd s3,    24(a0)\n"
               "sd s4,    32(a0)\n"
               "sd s5,    40(a0)\n"
               "sd s6,    48(a0)\n"
               "sd s7,    56(a0)\n"
               "sd s8,    64(a0)\n"
               "sd s9,    72(a0)\n"
               "sd s10,   80(a0)\n"
               "sd s11,   88(a0)\n"
               "sd sp,    96(a0)\n"
               "sd ra,    104(a0)\n"
               "li a0, 0\n"
               "ret\n"
               );
}

int _setjmp(jmp_buf b) {
  asm volatile("sd s0,    0(a0)\n"
               "sd s1,    8(a0)\n"
               "sd s2,    16(a0)\n"
               "sd s3,    24(a0)\n"
               "sd s4,    32(a0)\n"
               "sd s5,    40(a0)\n"
               "sd s6,    48(a0)\n"
               "sd s7,    56(a0)\n"
               "sd s8,    64(a0)\n"
               "sd s9,    72(a0)\n"
               "sd s10,   80(a0)\n"
               "sd s11,   88(a0)\n"
               "sd sp,    96(a0)\n"
               "sd ra,    104(a0)\n"
               "li a0, 0\n"
               "ret\n"
               );
}

void longjmp(jmp_buf b, int n) {
  asm volatile("ld s0,    0(a0)\n"
               "ld s1,    8(a0)\n"
               "ld s2,    16(a0)\n"
               "ld s3,    24(a0)\n"
               "ld s4,    32(a0)\n"
               "ld s5,    40(a0)\n"
               "ld s6,    48(a0)\n"
               "ld s7,    56(a0)\n"
               "ld s8,    64(a0)\n"
               "ld s9,    72(a0)\n"
               "ld s10,   80(a0)\n"
               "ld s11,   88(a0)\n"
               "ld sp,    96(a0)\n"
               "ld ra,    104(a0)\n"
               "seqz a0, a1\n"
               "add a0, a0, a1\n"
               "ret\n"
               );
}

void _longjmp(jmp_buf b, int n) {
  asm volatile("ld s0,    0(a0)\n"
               "ld s1,    8(a0)\n"
               "ld s2,    16(a0)\n"
               "ld s3,    24(a0)\n"
               "ld s4,    32(a0)\n"
               "ld s5,    40(a0)\n"
               "ld s6,    48(a0)\n"
               "ld s7,    56(a0)\n"
               "ld s8,    64(a0)\n"
               "ld s9,    72(a0)\n"
               "ld s10,   80(a0)\n"
               "ld s11,   88(a0)\n"
               "ld sp,    96(a0)\n"
               "ld ra,    104(a0)\n"
               "seqz a0, a1\n"
               "add a0, a0, a1\n"
               "ret\n"
               );
}

int abs(int a)
{
  return a>0 ? a : -a;
}

#endif  // __CKB_IMPL_INCLUDED__
