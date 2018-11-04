#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bfls_bm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不区分大小写的map，小写映射成了大写 */
static const unsigned char case_map[256] = {
0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  
16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  
32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  
48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  
64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  
80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  
96,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  
80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  123, 124, 125, 126, 127, 
128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 
144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 
160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 
176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 
192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 
208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 
224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 
240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};

bm_t *bm_build_tbl(const unsigned char *pattern, unsigned int len)
{
    int  i = (int)len - 1;
    int  j;
    int  k;
    int  last;
    bm_t *bm = (bm_t*)calloc(1, sizeof(bm_t)+len*sizeof(int));
    
    if (bm == NULL) {
        return NULL;
    }
    bm->gs = (int*)(bm+1);
    bm->pattern = pattern;
    bm->plen = len;

    for (i=0; i<256; ++i) {
        bm->bs[i] = len;
    }
    for (i=0; i<(int)len; ++i) {
        bm->bs[pattern[i]] = len-1-i;
    }

    /*
      查找最近的"匹配前缀"，比如xxxxxabcd，在a的时候不匹配了，
      那么就需要查找xxxxx中是否包含子串bcd,且bcd前得有一个字母，
      而且不能是a(如果是a，那么肯定同样不匹配，如果前面没)字符
      了也不会匹配。比如在字符串
      xyzefghijrbcdmnopqabcd中查找
      rbcdmnopqabcd
      从后往前匹配的时候，匹配到a的时候失配了，这个时候就需要找
      到是前面否存在已经匹配成功的子串，没有则跳过整个字符串，有
      则根据最近的进行跳转
      有子串的情况：
      源：|<------------pre--- --------->|<---c1--->|<-post->|  
      子：|<---p--->|<---c2--->|<---b--->|<---c1--->|
      现在匹配到了右边bc交界的地方失配了，那么下一次匹配应该是这么来
      源：|<-------------pre------------>|<---c1--->|<-post->|
      子：                     |<---p--->|<---c2--->|<---b--->|<---c1--->|
                                         old                             new
      c2可以完全等于c1。也可以是c1的一个后缀子集，这时候p必须为0，然后b里面任意
      一个失配移动的距离都是相同的（也即c2是b+c1的一个子串）。
      移动的距离是子串长度-(c2+p)
      zbaab
      55531
    */
    
    /* 一开始假定子串里面没有前后缀匹配，那么偏移量等于字符串长度 */
    for (i=0; i<(int)len-1; ++i) {
        bm->gs[i] = len;
    }
    bm->gs[i] = 1;

    i = (int)len-2;
    last = i; // 最右一个待修改偏移量的
    while (i >= 0) {
        if (pattern[i] == pattern[len-1]) {
            for (j=(int)len-2, k=i-1; k>=0; --k, --j) {
                if (pattern[k] != pattern[j]) {
                    break;
                }
            }
            
            if (k < 0) {/* c1是c2的子串 */
                j = (int)len-1-i;
                for (k=len-i-2; k>i; --k) {
                    if (bm->gs[k] == (int)len) {
                        bm->gs[k] = j;
                    }
                }
            }
            else if (bm->gs[j] == (int)len) {
                bm->gs[j] = (int)len-1-i;
            }
        }
        --i;
    }

    return bm;
}


int bm_search(const unsigned char *src, unsigned int slen, unsigned int pos, const bm_t *bm)
{
    const unsigned char *pattern = bm->pattern;
    unsigned int plen = bm->plen;
    int base = (int)pos;
    int pid = (int)plen;
    int sid = pos+pid;
    
    while (sid <= (int)slen) {
        if (bm->bs[src[sid-1]] > 0) {
            base += bm->bs[src[sid-1]];
            sid += bm->bs[src[sid-1]];
        }

        while (pattern[--pid] == src[--sid]) {
            if (pid == 0) {
                return base;
            }
        }

        base += bm->gs[pid];
        pid = (int)plen;
        sid = base+pid;
    }
    
    return -1;
}

/* 大小写不敏感版本(case insensitive) */
bm_t *bm_build_tbl_ci(const unsigned char *pattern, unsigned int len)
{
    int  i = (int)len - 1;
    int  j;
    int  k;
    int  last;
    unsigned char *tpat;
    bm_t *bm = (bm_t*)calloc(1, sizeof(bm_t)+len*(sizeof(int)+sizeof(unsigned char)));
    
    if (bm == NULL) {
        return NULL;
    }
    bm->gs = (int*)(bm+1);
    tpat = (unsigned char*)(bm->gs+len);
    bm->plen = len;
    bm->pattern = tpat;

    for (i=0; i<(int)len; ++i) {
        tpat[i] = case_map[pattern[i]];
    }

    for (i=0; i<256; ++i) {
        bm->bs[i] = len;
    }
    for (i=0; i<(int)len; ++i) {
        bm->bs[bm->pattern[i]] = len-1-i;
    }

    /* 一开始假定子串里面没有前后缀匹配，那么偏移量等于字符串长度 */
    for (i=0; i<(int)len-1; ++i) {
        bm->gs[i] = len;
    }
    bm->gs[i] = 1;

    i = (int)len-2;
    last = i; // 最右一个待修改偏移量的
    while (i >= 0) {
        if (tpat[i] == tpat[len-1]) {
            for (j=(int)len-2, k=i-1; k>=0; --k, --j) {
                if (tpat[k] != tpat[j]) {
                    break;
                }
            }
            
            if (k < 0) {/* c1是c2的子串 */
                j = (int)len-1-i;
                for (k=len-i-2; k>i; --k) {
                    if (bm->gs[k] == (int)len) {
                        bm->gs[k] = j;
                    }
                }
            }
            else if (bm->gs[j] == (int)len) {
                bm->gs[j] = (int)len-1-i;
            }
        }
        --i;
    }

    return bm;
}


int bm_search_ci(const unsigned char *src, unsigned int slen, unsigned int pos, const bm_t *bm)
{
    const unsigned char *pattern = bm->pattern;
    unsigned int plen = bm->plen;

    int base = (int)pos;
    int pid = (int)plen;
    int sid = pos+pid;

    while (sid <= (int)slen) {
        if (bm->bs[case_map[src[sid-1]]] > 0) {
            base += bm->bs[case_map[src[sid-1]]];
            sid += bm->bs[case_map[src[sid-1]]];
        }

        while (pattern[--pid] == case_map[src[--sid]]) {
            if (pid == 0) {
                return base;
            }
        }

        base += bm->gs[pid];
        pid = (int)plen;
        sid = base+pid;
    }
    
    return -1;
}


#ifdef __cplusplus
}
#endif
