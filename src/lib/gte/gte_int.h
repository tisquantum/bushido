/*
 * Internal lookup tables and helper interfaces for LIBGTE.
 */
#ifndef _GTE_INT_H_
#define _GTE_INT_H_

/*
 * Quarter-wave Q12 sine lookup table (defined in sincos.c):
 * round(sin(i * 2*pi / 4096) * 4096) for i = 0..1024 (0..90 deg).
 */
extern short rsin_tbl[1025];

/* Quadrant-reduced sine helper for non-negative angles in [0, 4095] (geo_00.c). */
extern int sin_1(int a);

#endif /* _GTE_INT_H_ */
