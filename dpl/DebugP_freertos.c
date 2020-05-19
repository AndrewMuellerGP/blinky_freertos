#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

void _DebugP_assert(int expression, const char *file, int line)
{
#if configASSERT_DEFINED
    configASSERT(expression);
#endif
}

void DebugP_log0(const char *format)
{
//    printf(format);
}

void DebugP_log1(const char *format, uintptr_t p1)
{
//    printf(format, p1);
}

void DebugP_log2(const char *format, uintptr_t p1, uintptr_t p2)
{
//    printf(format, p1, p2);
}
/*
 *  ======== DebugP_log3 ========
 */
void DebugP_log3(const char *format, uintptr_t p1, uintptr_t p2, uintptr_t p3)
{
//    printf(format, p1, p2, p3);
}
/*
 *  ======== DebugP_log4 ========
 */
void DebugP_log4(const char *format, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
//    printf(format, p1, p2, p3, p4);
}
