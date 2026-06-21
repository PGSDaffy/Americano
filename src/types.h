#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdlib.h>

// variable encoding: 00=unused, 01=0, 10=1, 11=DC
#define ZERO 0x1
#define ONE 0x2
#define DC 0x3

// a cube is just an array of unsigned ints
typedef unsigned int *pset;

// a set of cubes (a boolean function)
typedef struct set_family
{
    int wsize;    // ints per cube
    int count;    // current number of cubes
    int capacity; // allocated capacity
    pset data;    // contiguous storage for all cubes
} set_family;

// get the i-th cube in a set_family
#define GETSET(F, i) ((F)->data + (i) * (F)->wsize)

#endif
