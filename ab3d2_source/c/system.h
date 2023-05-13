#ifndef SYSTEM_H
#define SYSTEM_H

#include <exec/types.h>

extern BOOL Sys_Init(void);
extern void Sys_Done(void);

static inline struct ExecBase *getSysBase(void)
{
    extern struct ExecBase * SysBase;
    return SysBase;
}
#define LOCAL_SYSBASE() struct ExecBase *const SysBase = getSysBase()

static inline struct IntuitionBase *getIntuitionBase(void)
{
    extern struct IntuitionBase * IntuitionBase;
    return IntuitionBase;
}
#define LOCAL_INTUITION() struct IntuitionBase *const IntuitionBase = getIntuitionBase()

static inline struct GfxBase *getGfxBase(void)
{
    extern struct GfxBase * GfxBase;
    return GfxBase;
}
#define LOCAL_GFX() struct GfxBase *const GfxBase = getGfxBase()

static inline void CallAsm(void *func)
{
    __asm __volatile("\t movem.l d2-d7/a2-a6,-(a7)\n"
        "\t jsr (%0)\n"
        "\t movem.l (a7)+,d2-d7/a2-a6\n"
        : /* no result */
        : "a"(func)
        : "d0", "d1", "a0", "a1", "memory");
}

#endif // SYSTEM_H
