/*
 KMP字符串查找，这个算法效率比不上BM，建议用BM
*/

#ifndef __BFLS_KMP_H
#define __BFLS_KMP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  
{
	int *next;
    int count;
	unsigned char sd[256]; // sunday算法
}kmp_t;

kmp_t* kmp_build_tbl(const unsigned char *pattern, unsigned int len);
int kmp_search(const unsigned char *dstbuffer, unsigned int dstlen, const unsigned char *pattern, unsigned int patlen, const kmp_t *kmp);


#ifdef __cplusplus
}
#endif


#endif

