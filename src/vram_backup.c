/* Saving and restoring a 256x5 strip of VRAM at (320, 480); nothing calls either routine. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "vram_backup.h"

RECT gVramStripRect = { 320, 480, 256, 5 };

static u_long *gVramStripBackupBuffer;

u_long *BackupVramStrip(u_long *buffer)
{
    StoreImage(&gVramStripRect, buffer);
    DrawSync(0);
    gVramStripBackupBuffer = buffer;
    return buffer + (256 * 5) / 2;
}

int RestoreVramStrip(void)
{
    LoadImage(&gVramStripRect, gVramStripBackupBuffer);
    return DrawSync(0);
}
