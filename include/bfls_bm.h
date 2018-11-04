/*
 BM×Ö·û´®²éÕÒ
*/

#ifndef __BFLS_BM_H
#define __BFLS_BM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const unsigned char *pattern;
    unsigned int plen;
    int bs[256];
    int *gs;
}bm_t;

bm_t *bm_build_tbl(const unsigned char *pattern, unsigned int len);
int bm_search(const unsigned char *src, unsigned int slen, unsigned int pos, const bm_t *bm);
bm_t *bm_build_tbl_ci(const unsigned char *pattern, unsigned int len);
int bm_search_ci(const unsigned char *src, unsigned int slen, unsigned int pos, const bm_t *bm);


#ifdef __cplusplus
}
#endif


#endif

