#ifndef OPTIONS_UI_H
#define OPTIONS_UI_H

void LoadOptionsMenuAssets(void);
void DrawAutoSaveErrorOverlay(void);
void DrawOptionsMenu(int selectedRow, short **optionValues, int msgIndex, int confirmSelection);

#endif
