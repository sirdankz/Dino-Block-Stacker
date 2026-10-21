#ifndef TA_DIAGNOSTICS_H
#define TA_DIAGNOSTICS_H
/* Keep older test/configuration behavior unless explicitly disabled. */
#ifndef TA_R32_DIAGNOSTICS
#define TA_R32_DIAGNOSTICS 1
#endif
#if TA_R32_DIAGNOSTICS
#define TA_DIAG(statement) do { statement; } while(0)
#else
#define TA_DIAG(statement) ((void)0)
#endif
#endif
