
#ifdef __cplusplus
extern "C" {
#endif

#define _QSORT_TYPE int
#define _QSORT_FNAME bfls_qsort_int
#define _QSORT_CMP(ele1, ele2) ((ele1) >= (ele2))
#include "_qsort.h"

#define _QSORT_TYPE int
#define _QSORT_FNAME bfls_qsort_int_de
#define _QSORT_CMP(ele1, ele2) ((ele1) <= (ele2))
#include "_qsort.h"


#define _QSORT_TYPE void*
#define _QSORT_FNAME bfls_qsort
#include "_qsort.h"


static int intcmp(void *e1, void *e2)
{
    return (int)e2 - (int)e1;
}

int main(int argc, char **argv)
{
    int ar[] = {10, 20, 1, 2, 4, 7, 10, 12, 5, 6, 9, 0, 20};
    unsigned int num=sizeof(ar)/sizeof(ar[0]);
    unsigned int i;
    //bfls_qsort_int(ar, num);
    bfls_qsort((void**)ar, num, intcmp);
    for (i=0; i<num; ++i)printf("%-3d", ar[i]);
    bfls_qsort_int_de(ar, num);
    printf("\n");
    for (i=0; i<num; ++i)printf("%-3d", ar[i]);
    
    return 0;
}

#ifdef __cplusplus
}
#endif

