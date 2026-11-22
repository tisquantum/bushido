#ifndef WORLD_VECTOR_H
#define WORLD_VECTOR_H

/* VECTOR without its pad: a point, offset or velocity in world units. */
typedef struct WorldVector {
    int x, y, z;
} WorldVector;

#endif
