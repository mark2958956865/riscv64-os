#ifndef TOS_ASSERT_H
#define TOS_ASSERT_H

void assertion_failure(const char *exp,
                       const char *file,
                       int line);

#define assert(exp)                                      \
    do {                                                 \
        if (!(exp)) {                                    \
            assertion_failure(#exp, __FILE__, __LINE__); \
        }                                                \
    } while (0)

#endif