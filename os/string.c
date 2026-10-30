#include "os.h"


size_t strlen(const char *str)
{
    const char *ptr = str;

    while (*ptr != '\0') {
        ptr++;
    }

    return ptr - str;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (count--) {
        *d++ = *s++;
    }

    return dest;
}