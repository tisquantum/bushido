/*
 * Internal kernel stub and patch interfaces for LIBAPI.
 */
#ifndef _API_INT_H_
#define _API_INT_H_

/* An entry of the kernel's interrupt chains, as SysEnqIntRP and SysDeqIntRP take it. */
struct IntrChainEntry {
    struct IntrChainEntry *next;
    long (*handler)(void);
    long (*check)(void);
    long unused; /* zeroed by setup_patch_rp; nothing in the program reads it */
};

#define PAD_INTR_PRIORITY 1

extern void _send_pad(void);
extern long _SendPAD(char *buf1, long len1, char *buf2, long len2);

extern void _patch_pad(void);
extern long EnablePAD(void);
extern void DesablePAD(void);
extern long PAD_init2(long mode, long *tbl, long arg3, long arg4);
extern long InitPAD2(char *bufA, long lenA, char *bufB, long lenB);
extern long StartPAD2(void);
extern void StopPAD2(void);
extern void SysEnqIntRP(int priority, void *chain);
extern void SysDeqIntRP(int priority, void *chain);
extern long AddDrv(void *dcb);
extern long DelDrv(char *name);

extern long SendPAD(char *buf1, long len1, char *buf2, long len2);

#endif /* _API_INT_H_ */
