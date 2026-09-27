#ifndef SERTOS_PORT_WEAK_H
#define SERTOS_PORT_WEAK_H

#if defined(__GNUC__) || defined(__clang__)
#define SERTOS_PORT_WEAK __attribute__((weak))
#elif defined(__ICCARM__) || defined(__CC_ARM)
#define SERTOS_PORT_WEAK __weak
#else
#error "Define SERTOS_PORT_WEAK for this toolchain"
#endif

#endif /* SERTOS_PORT_WEAK_H */
