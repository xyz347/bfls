#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bfls_kmp.h>

#ifdef __cplusplus
extern "C" {
#endif


kmp_t* kmp_build_tbl(const unsigned char *pattern, unsigned int len)
{
    kmp_t *kmp = (kmp_t*)calloc(1, sizeof(kmp_t)+(len)*sizeof(int));
	int i;
    int j;

	if (NULL == kmp) {
		return NULL;
	} else {
        kmp->next = (int*)(kmp+1);
        kmp->count = (int)len;
    }

    kmp->next[0] = -1;
    kmp->sd[pattern[0]] = 1;

    i = -1;
    j = 0;
    while (j+1 < (int)len) {
        if (i==-1 || pattern[j]==pattern[i]) {
            ++i;
            ++j;
            kmp->sd[pattern[j]] = 1;
            if (pattern[j] == pattern[i]) {
                kmp->next[j] = kmp->next[i];
            } else {
                kmp->next[j] = i;
            }
        } else {
            i = kmp->next[i];
        }
    }
    
    return kmp;
}


int kmp_search(const unsigned char *dstbuffer, unsigned int dstlen, const unsigned char *pattern, unsigned int patlen, const kmp_t *kmp)
{
	int base = 0;
	int i = 0;

	while(base+i < (int)dstlen) {
		if (dstbuffer[base+i] != pattern[i]) {
            if (kmp->sd[dstbuffer[base+patlen]] == 0) {
                base += patlen+1;
                i = 0;
                continue;
            }

			base = base+i-kmp->next[i];
            if (kmp->next[i] == -1) {
                i = 0;
            } else {
                i = kmp->next[i];
            }
		} else {
			++i;
            if (i == (int)patlen) {
                return base;
            }
		}
	}

	return -1;
}

#ifdef __cplusplus
}
#endif

