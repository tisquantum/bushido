#ifndef VRAM_BACKUP_H
#define VRAM_BACKUP_H

#include <sys/types.h>

u_long *BackupVramStrip(u_long *buffer);
int RestoreVramStrip(void);

#endif
