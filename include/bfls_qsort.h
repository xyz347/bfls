#ifndef __BFLS_QSORT_H
#define __BFLS_QSORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*qscmp)(void*, void *);  // put this in bfls_qsort.h

void bfls_qsort_int(int *eles, unsigned int num);
void bfls_qsort_int_de(int *eles, unsigned int num);
void bfls_qsort(void **eles, unsigned int num, qscmp cmp);

#ifdef __cplusplus
}
#endif


#endif


