#include "common.h"

extern void putchar(char ch);

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = c;
    return buf;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static void print_binary(unsigned int num)
{
    putchar('0');
    putchar('b');

    // 如果是 0，直接打印并返回
    if (num == 0) {
        putchar('0');
        return;
    }

    // 寻找最高位的 1，跳过前面的前导 0（可选，让输出更紧凑）
    int start_bit = 31;
    while ((start_bit >= 0) && !((num >> start_bit) & 1)) {
        start_bit--;
    }

    // 从最高有效位开始，逐位打印 '0' 或 '1'
    for (int i = start_bit; i >= 0; i--) {
        char bit = ((num >> i) & 1) ? '1' : '0';
        putchar(bit);
        if ((i % 4 == 0) && (i != 0)) {
            putchar('_');
        }
    }
}

void printf(const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case '\0':
                    putchar('%');
                    goto end;
                case '%':
                    putchar('%');
                    break;
                case 's': {
                    const char *s = va_arg(vargs, const char *);
                    while (*s) {
                        putchar(*s);
                        s++;
                    }
                    break;
                }
                case 'd': {
					int value = va_arg(vargs, int);
					unsigned magnitude = value;
                    if (value < 0) {
                        putchar('-');
                        magnitude = -magnitude;
                    }

                    unsigned divisor = 1;
                    while (magnitude / divisor > 9)
                        divisor *= 10;

                    while (divisor > 0) {
                        putchar('0' + magnitude / divisor);
                        magnitude %= divisor;
                        divisor /= 10;
                    }

                    break;
                }
                case 'b': {
                    unsigned value = va_arg(vargs, unsigned);
                    print_binary(value);
                    break;
                }
                case 'x': {
                    unsigned value = va_arg(vargs, unsigned);
                    for (int i = 7; i >= 0; i--) {
                        unsigned nibble = (value >> (i * 4)) & 0xf;
                        putchar("0123456789abcdef"[nibble]);
                    }
                }
            }
        } else {
            putchar(*fmt);
        }

        fmt++;
    }

end:
    va_end(vargs);
}
