/*
 * Formatted string output (sprintf).
 */
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

struct fmt_spec {
    unsigned int f_minus : 1;
    unsigned int f_plus : 1;
    unsigned int f_hash : 1;
    unsigned int f_zero : 1;
    unsigned int f_prec : 1;
    unsigned int f_short : 1;
    unsigned int f_long : 1;
    unsigned int f_ldbl : 1;
    unsigned char sign;
    int width;
    int prec;
};

static struct fmt_spec default_spec = { 0 };

/* the original keeps ap in memory and steps it there for each argument */
#define next_arg(ap, type) (*(*(type **)&(ap))++)

int sprintf(char *buffer, char *fmt, ...)
{
    va_list ap;
    char conv_buffer[512];
    struct fmt_spec spec;
    int count;
    int ch;
    int len;
    char *p;
    long val;
    char *digits;
    char *nul;

    va_start(ap, fmt);

    for (count = 0; (ch = *(unsigned char *)fmt) != '\0'; fmt++) {
        if (ch != '%') {
            buffer[count++] = ch;
            continue;
        }
        spec = default_spec;
        for (;;) {
            ch = *(unsigned char *)++fmt;
            if (ch == '-') {
                spec.f_minus = 1;
            } else if (ch == '+') {
                spec.f_plus = 1;
            } else if (ch == ' ') {
                spec.sign = ' ';
            } else if (ch == '#') {
                spec.f_hash = 1;
            } else if (ch == '0') {
                spec.f_zero = 1;
            } else {
                break;
            }
        }
        if (ch == '*') {
            spec.width = next_arg(ap, int);
            if (spec.width < 0) {
                spec.width = -spec.width;
                spec.f_minus = 1;
            }
            ch = *(unsigned char *)++fmt;
        } else {
            while (ch >= '0' && ch <= '9') {
                spec.width = spec.width * 10 + (ch - '0');
                ch = *(unsigned char *)++fmt;
            }
        }
        if (ch == '.') {
            ch = *(unsigned char *)++fmt;
            if (ch == '*') {
                spec.prec = next_arg(ap, int);
                ch = *(unsigned char *)++fmt;
            } else {
                while (ch >= '0' && ch <= '9') {
                    spec.prec = spec.prec * 10 + (ch - '0');
                    ch = *(unsigned char *)++fmt;
                }
            }
            if (spec.prec >= 0) {
                spec.f_prec = 1;
            }
        }
        if (spec.f_minus) {
            spec.f_zero = 0;
        }
        p = conv_buffer + sizeof(conv_buffer);
    reswitch:
        switch (ch) {
        case 'h':
            spec.f_short = 1;
            goto next_mod;
        case 'l':
            spec.f_long = 1;
            goto next_mod;
        case 'L':
            spec.f_ldbl = 1;
        next_mod:
            ch = *(unsigned char *)++fmt;
            goto reswitch;
        case 'd':
        case 'i':
            if (spec.f_long) {
                val = next_arg(ap, long);
            } else {
                val = next_arg(ap, int);
            }
            if (spec.f_short) {
                val = (short)val;
            }
            if (val < 0) {
                val = -val;
                spec.sign = '-';
            } else if (spec.f_plus) {
                spec.sign = '+';
            }
            goto do_dec;
        case 'u':
            if (spec.f_long) {
                val = next_arg(ap, unsigned long);
            } else {
                val = next_arg(ap, unsigned int);
            }
            if (spec.f_short) {
                val = (unsigned short)val;
            }
            spec.sign = 0;
        do_dec:
            if (!spec.f_prec) {
                if (spec.f_zero) {
                    spec.prec = spec.width;
                    if (spec.sign) {
                        spec.prec--;
                    }
                }
                if (spec.prec <= 0) {
                    spec.prec = 1;
                }
            }
            len = 0;
            while (val != 0) {
                *--p = ((unsigned long)val % 10) + '0';
                val = (unsigned long)val / 10;
                len++;
            }
            while (len < spec.prec) {
                *--p = '0';
                len++;
            }
            if (spec.sign) {
                *--p = spec.sign;
                len++;
            }
            break;
        case 'o':
            if (spec.f_long) {
                val = next_arg(ap, unsigned long);
            } else {
                val = next_arg(ap, unsigned int);
            }
            if (spec.f_short) {
                val = (unsigned short)val;
            }
            if (!spec.f_prec) {
                if (spec.f_zero) {
                    spec.prec = spec.width;
                }
                if (spec.prec <= 0) {
                    spec.prec = 1;
                }
            }
            len = 0;
            while (val != 0) {
                *--p = (val & 7) + '0';
                val = (unsigned long)val >> 3;
                len++;
            }
            if (spec.f_hash && len != 0 && *p != '0') {
                *--p = '0';
                len++;
            }
            while (len < spec.prec) {
                *--p = '0';
                len++;
            }
            break;
        case 'p':
            spec.prec = 8;
            spec.f_prec = 1;
            spec.f_long = 1;
            /* fall through */
        case 'X':
            digits = "0123456789ABCDEF";
            goto do_hex;
        case 'x':
            digits = "0123456789abcdef";
        do_hex:
            if (spec.f_long) {
                val = next_arg(ap, unsigned long);
            } else {
                val = next_arg(ap, unsigned int);
            }
            if (spec.f_short) {
                val = (unsigned short)val;
            }
            if (!spec.f_prec) {
                if (spec.f_zero) {
                    spec.prec = spec.width;
                    if (spec.f_hash) {
                        spec.prec -= 2;
                    }
                }
                if (spec.prec <= 0) {
                    spec.prec = 1;
                }
            }
            len = 0;
            while (val != 0) {
                *--p = digits[val & 0xf];
                val = (unsigned long)val >> 4;
                len++;
            }
            while (len < spec.prec) {
                *--p = '0';
                len++;
            }
            if (spec.f_hash) {
                *--p = ch;
                *--p = '0';
                len += 2;
            }
            break;
        case 'c':
            *--p = next_arg(ap, int);
            len = 1;
            break;
        case 's':
            p = next_arg(ap, char *);
            if (spec.f_hash) {
                /* Pascal-style length-prefixed string extension (%#s). */
                len = *(unsigned char *)p++;
                if (spec.f_prec && spec.prec < len) {
                    len = spec.prec;
                }
            } else if (!spec.f_prec) {
                len = strlen(p);
            } else {
                nul = memchr((unsigned char *)p, '\0', spec.prec);
                if (nul != NULL) {
                    len = nul - p;
                } else {
                    len = spec.prec;
                }
            }
            break;
        case 'n':
            p = next_arg(ap, char *);
            if (spec.f_short) {
                *(short *)p = count;
            } else if (spec.f_long) {
                *(long *)p = count;
            } else {
                *(int *)p = count;
            }
            continue;
        default:
            if (ch == '%') {
                buffer[count++] = ch;
                continue;
            }
            goto done;
        }
        if (len < spec.width && !spec.f_minus) {
            do {
                buffer[count++] = ' ';
                spec.width--;
            } while (len < spec.width);
        }
        memmove((unsigned char *)(buffer + count), (unsigned char *)p, len);
        count += len;
        while (len < spec.width) {
            buffer[count++] = ' ';
            len++;
        }
    }
done:
    buffer[count] = '\0';
    return count;
}
