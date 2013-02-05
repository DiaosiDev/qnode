/*
 * See Copyright Notice in qnode.h
 */

#ifndef __QMINHEAP_H__
#define __QMINHEAP_H__

#include "qidmap.h"

/* 
 * set data min heap index
 * */
typedef void (*set_func_t)(void *data, int index);

/* 
 * get data min heap index
 * */
typedef int  (*get_func_t)(void *data);

/* 
 * compare data key
 * */
typedef int  (*cmp_func_t)(void *data1, void *data2);

typedef struct qminheap_t {
  void**        data;
  unsigned int  size;
  unsigned int  num;
  cmp_func_t    cmp;
  set_func_t    set;
  get_func_t    get;
} qminheap_t;

#define qminheap_empty(heap)  ((heap)->num == 0)

void    qminheap_init(qminheap_t *heap, cmp_func_t cmp, set_func_t set, get_func_t get);
int     qminheap_push(qminheap_t *heap, void *data);
void*   qminheap_pop(qminheap_t *heap);
void*   qminheap_top(qminheap_t *heap);
int     qminheap_erase(qminheap_t *heap, int index);

#endif  /* __QMINHEAP_H__ */
