#ifndef __BFLS_HASH_H
#define __BFLS_HASH_H


#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int (*bfls_hash_hashf)(void *data);
typedef int (*bfls_hash_cmpf)(void*data, void *key);
typedef int (*bfls_hash_iterf)(void*data, void *arg);



unsigned int bfls_hash_char(void *data);
unsigned int bfls_hash_32(unsigned int val);
unsigned int bfls_hash_size(unsigned int elenum);
void* bfls_hash_create(void *addr, unsigned int elenum, int uselow, bfls_hash_hashf func, bfls_hash_cmpf cmp);
void bfls_hash_destroy(void *tbl);
void *bfls_hash_get(void *table, void *key);
int bfls_hash_add(void *table, void *data);
void *bfls_hash_del(void *table, void *key);
unsigned int bfls_hash_iter(void *table, bfls_hash_iterf iter, void *arg);
void bfls_hash_show_index(void *table, unsigned int index);
void bfls_hash_show_key(void *table, void *data);
void bfls_hash_show(void *table);

#ifdef __cplusplus
}
#endif


#endif
