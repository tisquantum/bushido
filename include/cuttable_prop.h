#ifndef CUTTABLE_PROP_H
#define CUTTABLE_PROP_H

void InitCuttableProps(unsigned int *resource, int slot, unsigned int *propMarkers);
void TriggerCuttablePropFall(int propIndex, int heading);
void UpdateCuttableProps(void);
void RelocateCuttableProps(int delta);

/* cuttable_fade.s: dispatch-table drawers, inputs in registers. */
void DrawFadingPolyFt3(void);
void DrawFadingPolyFt4(void);

#endif
