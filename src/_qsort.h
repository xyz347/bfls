/* 这个文件要反复引用，不要用#ifndef */


#include <bfls_qsort.h>

#ifndef _QSORT_CMP
  #define _QSORT_CMPARG ,qscmp cmp
  #define _QSORT_CMPNAME ,cmp
  #define _QSORT_CMP(ele1, ele2) (0 <= cmp((ele2), (ele1)))
#else
  #define _QSORT_CMPARG
  #define _QSORT_CMPNAME
#endif

#ifndef _QFNAME
#define _QFNAME(x) __##x
#define __QFNAME(x) _QFNAME(x)
#endif


void __QFNAME(_QSORT_FNAME)(_QSORT_TYPE *eles, unsigned int i, unsigned int j _QSORT_CMPARG)
{
    unsigned int bot=i, top=j; 
    _QSORT_TYPE X = eles[i];
    while (i != j) { 
        while ((j>i) && (_QSORT_CMP(eles[j], X))) {
            --j;
        }
        if (j > i) {
            eles[i++]=eles[j];
        }
        
        while ((j>i) && (!(_QSORT_CMP(eles[i], X)))) {
            ++i;
        }
        if (j > i) {
            eles[j--]=eles[i]; 
        }
    }
    eles[i] = X;
    if (bot+1 < i) {
        __QFNAME(_QSORT_FNAME)(eles, bot, i-1 _QSORT_CMPNAME); 
    }
    if (i+1 < top) {
        __QFNAME(_QSORT_FNAME)(eles, i+1, top _QSORT_CMPNAME); 
    }
}

void _QSORT_FNAME(_QSORT_TYPE *eles, unsigned int num _QSORT_CMPARG)
{
    if (num > 2) {
        __QFNAME(_QSORT_FNAME)(eles, 0, num-1 _QSORT_CMPNAME);
    }
}

#undef _QSORT_FNAME
#undef _QSORT_TYPE
#undef _QSORT_CMPARG
#undef _QSORT_CMPNAME
#undef _QSORT_CMP

