#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdlib.h>

// 一个 unsigned int 多少位
#define BITS_PER_INT 32

// 每个变量用 2 bit 编码，一个 int 能存 BITS_PER_INT/2 个变量
#define VARS_PER_INT (BITS_PER_INT / 2)

// 变量取值编码
// 00 不用，01=0(反相), 10=1(原相), 11=-(任意)
#define ZERO 0x1
#define ONE 0x2
#define DC 0x3

// 一个乘积项就是一个 unsigned int 数组
typedef unsigned int *pset;

// 一堆乘积项（即一个布尔函数）
typedef struct set_family
{
    int wsize;    // 每个 pset 占几个 int
    int count;    // 当前有多少个 pset
    int capacity; // 最多能装多少个
    pset data;    // 所有 pset 的连续内存
} set_family;

// 取第 i 个 pset
#define GETSET(F, i) ((F)->data + (i) * (F)->wsize)

#endif
