/*
  哈希表实现。
  里面使用了双向链表来维护哈希表里面的elements。有两个链表：
  1、空闲的elements的链表。
  2、有数据的elements的链表
  为了在64位系统中节省空间，不用void*，用int来指向，当数据小于2G的时候
  是没有问题的。
  里面的散列函数都是抄别人的。
  
  1, 刚开始所有的节点都挂载在空闲列表上
  free_list ---->NODE1--->NODE2--->NODE3--->NODE4--->NODE5
  2, 假如往哈希表添加元素在NODE2上
  free_list ---->NODE1------------>NODE3--->NODE4--->NODE5
                          NODE2
  3, 假如再添加一个元素，NODE2冲突，则从链表头摘取一个
  free_list ---------------------->NODE3--->NODE4--->NODE5
                          NODE2--->NODE1
  4, 假如再添加一个元素，哈希值落在NODE1，则NODE1归位，取第一个空余节点
  free_list ------------------------------->NODE4--->NODE5
                NODE1     NODE2--->NODE3
  另外为了避免冲突的时候，需要对节点数据进行比对，里面还引入了
  sub hash的方法，只有hash和sub hash都一样的时候才需要进行数据
  比较，之前尝试用4万多个字符串测试，hash和sub hash都相同的只有
  一对
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bfls_hash.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HASH_DEBUG

/* 根据哈希值计算索引和子哈希值，如果索引用低位，则子哈希用高位，否则反过来 */
#define BFLS_HASH_INDEX(tbl, hash)   (((tbl)->shift==-1)?((hash)%(tbl)->elenum):(((hash)>>((tbl)->shift))%(tbl)->elenum))
#define BFLS_HASH_SUBHS(tbl, hash)   (((tbl)->shift==-1)?((hash)>>16):((hash)&0xffff))

/* table flags */
#define BFLS_HASH_TFLAG_DONOT_FREE      (1<<0)

/* data flags */
#define BFLS_HASH_DFLAG_MASTER      (1<<31) //这个坑的数据是“正统”的（不是被借用的）
#define BFLS_HASH_DFLAG_SLAVE       (1<<30) //这个坑是被借用的

/* element 数据结构 */
typedef struct
{
    int next;               // 不用指针，64位系统浪费4个字节，用int便于用-1
    int prev;    
    unsigned int flags;     // 高16位是flags，低16位是sub hash，也即另一个哈希值
    void *data;             // 数据指针
#ifdef HASH_DEBUG
    unsigned int conflict;  // 这个坑的冲突数
#endif
}hash_data_t;

/* table数据结构 */
typedef struct
{
    bfls_hash_hashf   hash; // 散列函数
    bfls_hash_cmpf    cmp;  // 比较函数
    unsigned int elenum;    // element number
    int          shift;     // 哈希允许取高位或者低位，如果用高位，则通过移位实现，低位通过取余实现。低位时shift=-1
    unsigned int count;     // 当前elements数目
    unsigned int flags;     // flags
    int          next;      // free list
    hash_data_t* data;      // elements
#ifdef HASH_DEBUG
    unsigned int maxcfl;    // 整个表的最大冲突数
    unsigned int maxcflidx; // 最大冲突所在位置
    unsigned int mastercnt; // 散列数目
#endif
}hash_tbl_t;

/*
 从free list里面分配一个 
 i >= 0表示分配指定的
 i == -1表示分配第一个
*/
static int hdlist_alloc(hash_tbl_t *tbl, int i)
{
    if (i < 0) {
        i = tbl->next; 
        #ifdef HASH_DEBUG
        if (i == -1) { // 不应该出现
            printf("ERROR, i==-1\n");
            exit(-1);
        }
        #endif
    }

    if (tbl->data[i].next != -1) {
        tbl->data[tbl->data[i].next].prev = tbl->data[i].prev;
    }
    if (tbl->data[i].prev != -1) {
        tbl->data[tbl->data[i].prev].next = tbl->data[i].next;
    } else {
        tbl->next = tbl->data[i].next;
    }
    
    return i;
}

/*
 把hash_data_t放回free list里面
*/
static void hdlist_free(hash_tbl_t *tbl, int i)
{
    tbl->data[i].next = tbl->next;
    tbl->data[i].prev = -1;
    tbl->next = i;
}

/* 从elements list里面查找 */
static hash_data_t* hdlist_get(hash_tbl_t *tbl, unsigned int index, unsigned int subhs, void *key)
{
    hash_data_t *tbd = &tbl->data[index];
    while (1) {
        if (subhs==(tbd->flags&0xffff) && 0==tbl->cmp(tbd->data, key)) {
            break;
        } else if (tbd->next != -1) {
            tbd = &tbl->data[tbd->next];
        } else {
            tbd = NULL;
            break;
        }
    }
    
    return tbd;
}

/* 用来计算需要移位的数目 */
static unsigned int hash_power2(unsigned int size)
{
    // TODO >0x80000000 的情况
    unsigned int k = 0;
    unsigned int shift = 0x80000000;
    while (size <= shift) {
        shift >>= 1;
        ++k;
    }
    return k;
}

/*
 @Author Brian Kernighan & Dennis Ritchie 
 */
unsigned int bfls_hash_char(void *data)
{ 
    const char *str = (char*)data;
    register unsigned int hash = 0; 
    register unsigned int ch;
    while (ch = (unsigned int)*str++)  
    {         
        hash = hash * 131 + ch;
    }  
    return hash;  
}

/* From linux */
unsigned int bfls_hash_32(unsigned int val)
{
	return val * 0x9e370001UL;
}

/* 计算表头+elements的大小，bytes */
unsigned int bfls_hash_size(unsigned int elenum)
{
    return sizeof(hash_tbl_t)+sizeof(hash_data_t)*elenum;
}

/*
说明: 创建哈希表
入参: addr        内存地址，如果非空表示调用者负责内存的分配，如果为空则内部分配
      elenum      元素的个数
      uselow      1表示使用哈希值的低位做索引，0表示使用高位
      func        散列函数
      cmmp        比较函数
*/
void* bfls_hash_create(void *addr, unsigned int elenum, int uselow, bfls_hash_hashf func, bfls_hash_cmpf cmp)
{
    hash_tbl_t *tbl;
    int shift;
    unsigned int size;
    int i;
    
    shift = (1==uselow)?-1:hash_power2(elenum);
    size = bfls_hash_size(elenum);

    if (elenum < 2) {
        return NULL;
    } else if (NULL == addr) {
        tbl = (hash_tbl_t*)calloc(1, size);
        if (NULL == tbl)
            return NULL;
    } else {
        memset(addr, 0, size);
        tbl = (hash_tbl_t*)addr;
        tbl->flags = BFLS_HASH_TFLAG_DONOT_FREE;
    }

    tbl->data = (hash_data_t*)(tbl+1);
    tbl->hash = func;
    tbl->cmp = cmp;
    tbl->elenum = elenum;
    tbl->shift = shift;
    //tbl->subhs = 0x10000;
    /*TODO 暂时散列到0-0xffff之间，测试了一个60K的表项，增加了subhs之后
      只有12组字符串发生冲突，每组都只有两个字符串*/
    
    for (i=(int)elenum-1; i>=0; --i) {
        tbl->data[i].next = i+1;
        tbl->data[i].prev = i-1;
    }
    tbl->data[elenum-1].next = -1;
    
    return (void*)tbl;
}

/* 销毁哈希表 */
void bfls_hash_destroy(void *tbl)
{
    if (0 == (BFLS_HASH_TFLAG_DONOT_FREE & ((hash_tbl_t*)tbl)->flags))
        free(tbl);
}

/* 查找哈希表 */
void *bfls_hash_get(void *table, void *key)
{
    hash_tbl_t  *tbl = (hash_tbl_t*)table;
    hash_data_t *tbd;
    unsigned int hash = tbl->hash(key);
    unsigned int index = BFLS_HASH_INDEX(tbl, hash);//hash%tbl->size;
    unsigned int subhs = BFLS_HASH_SUBHS(tbl, hash);//hash%tbl->subhs;

    tbd = &tbl->data[index];
    if ((0 != (BFLS_HASH_DFLAG_MASTER&tbd->flags)) 
     && (NULL != (tbd=hdlist_get(tbl, index, subhs, key)))) {
         return tbd->data;
    } else {
        return NULL;
    }
}

// TODO BOOL?
int bfls_hash_add(void *table, void *data)
{
    hash_tbl_t  *tbl = (hash_tbl_t*)table;
    hash_data_t *tbd;
    unsigned int hash = tbl->hash(data);
    unsigned int index = BFLS_HASH_INDEX(tbl, hash);
    unsigned int subhs = BFLS_HASH_SUBHS(tbl, hash);
    int i;

    tbd = &tbl->data[index];

    if (tbl->count >= tbl->elenum) {
        goto ERRLABEL; 
    } else if (0 != (BFLS_HASH_DFLAG_MASTER&tbd->flags)) { //冲突，从空闲链表找一个
        if (NULL == hdlist_get(tbl, index, subhs, data)) {
            i = hdlist_alloc(tbl, -1);
            tbl->data[i].next = tbd->next;
            tbl->data[i].prev = index;
            tbl->data[i].flags = BFLS_HASH_DFLAG_SLAVE+subhs;
            tbl->data[i].data = data;

            if (tbd->next != -1) {
                tbl->data[tbd->next].prev = i;
            }
            tbd->next = i;

            #ifdef HASH_DEBUG
            ++tbd->conflict;
            if (tbd->conflict > tbl->maxcfl) {
                tbl->maxcfl = tbd->conflict;
                tbl->maxcflidx = index;
            }
            #endif
        } else {
            goto ERRLABEL;
        }
    } else if (0 != (BFLS_HASH_DFLAG_SLAVE&tbd->flags)) { //被别人占了
        i = hdlist_alloc(tbl, -1);
        memcpy(&tbl->data[i], tbd, sizeof(hash_data_t));
        tbl->data[tbd->prev].next = i;
        if (-1 != tbd->next) {
            tbl->data[tbd->next].prev = i;
        }
        
        tbd->next = -1;
        tbd->prev = -1;
        tbd->data = data;
        tbd->flags = BFLS_HASH_DFLAG_MASTER+subhs;
        #ifdef HASH_DEBUG
        tbd->conflict = 0;
        ++tbl->mastercnt;
        #endif
    } else {
        i = hdlist_alloc(tbl, index);

        tbd->next = -1;
        tbd->prev = -1;
        tbd->data = data;
        tbd->flags = BFLS_HASH_DFLAG_MASTER+subhs;
        #ifdef HASH_DEBUG
        tbd->conflict = 0;
        ++tbl->mastercnt;
        #endif
    }
    
    ++tbl->count;

    return 0;
ERRLABEL:
    return -1;// TODO
}


void *bfls_hash_del(void *table, void *key)
{
    hash_tbl_t  *tbl = (hash_tbl_t*)table;
    hash_data_t *tbd;
    unsigned int hash = tbl->hash(key);
    unsigned int index = BFLS_HASH_INDEX(tbl, hash);//hash%tbl->size;
    unsigned int subhs = BFLS_HASH_SUBHS(tbl, hash);//hash%tbl->subhs;
    unsigned int i;
    
    tbd = &tbl->data[index]; 
    if (0 != (BFLS_HASH_DFLAG_MASTER&tbd->flags)) {
        tbd = hdlist_get(tbl, index, subhs, key);
        if (NULL == tbd) {
            return NULL;
        } else if (0 != (BFLS_HASH_DFLAG_MASTER&tbd->flags)) { //首节点，那么我们删第二个节点
            --tbl->count;
            i = tbd->next;
            if (-1 != i) {
                void *deldata = tbd->data;
                tbd->data = tbl->data[i].data;
                tbd->flags = BFLS_HASH_DFLAG_MASTER+(tbl->data[i].flags & 0xffff);
                tbd->next = tbl->data[i].next;
                if (tbl->data[i].next != -1) {
                    tbl->data[tbl->data[i].next].prev = index;
                }
                tbl->data[i].flags = 0;
                hdlist_free(tbl, i);
                return deldata;
            } else {
                tbd->flags = 0;
                hdlist_free(tbl, index);
                return tbd->data;
            }
        } else { // slave
            --tbl->count;
            i = tbl->data[tbd->prev].next; // my index
            if (-1 != tbd->next) {
                tbl->data[tbd->next].prev = tbd->prev;
            }
            tbl->data[tbd->prev].next = tbd->next;
            tbd->flags = 0;
            hdlist_free(tbl, i);
            return tbd->data;
        }
    } else {
        return NULL;
    }
}

// return count
unsigned int bfls_hash_iter(void *table, bfls_hash_iterf iter, void *arg)
{
    hash_tbl_t  *tbl = (hash_tbl_t*)table;
    hash_data_t *tbd;
    unsigned int i;
    unsigned int cnt = 0;
    
    for (i=0; i<tbl->elenum; ++i) {
        tbd = &tbl->data[i];
        if (0 != ((BFLS_HASH_DFLAG_MASTER|BFLS_HASH_DFLAG_SLAVE)&tbd->flags)) {
            if (0 == iter(tbd->data, arg))
                ++cnt;
            else
                break;
        }
    }
    
    return cnt;
}

void bfls_hash_show_index(void *table, unsigned int index)
{
    hash_tbl_t  *tbl = (hash_tbl_t*)table;
    hash_data_t *tbd;
    int i = (int)index;

    tbd = &tbl->data[i];
    if (0 != (BFLS_HASH_DFLAG_MASTER&tbd->flags)) {
        while (-1 != i) {
            tbd = &tbl->data[i];
            printf("Index:%-8d subhs:%04x data:%s\n", i, tbd->flags&0xffff, tbd->data);
            i = tbd->next;
        }
    } else {
        printf("Not Master\n");
    }
}

void bfls_hash_show_key(void *table, void *data)
{
    hash_tbl_t  *tbl = (hash_tbl_t*)table;
    unsigned int hash = tbl->hash(data);
    unsigned int index = BFLS_HASH_INDEX(tbl, hash);//hash%tbl->size;
    
    bfls_hash_show_index(table, index);
}

void bfls_hash_show(void *table)
{
    hash_tbl_t *tbl = (hash_tbl_t*)table;
    printf("Hash address:%#x\nSize:%d\nCount:%d\nShift:%d\n", (int)table, tbl->elenum, tbl->count, tbl->shift);
#ifdef HASH_DEBUG
    printf("Max Conflict:%d\nMax conflict index:%d\nMaster count:%d\n", tbl->maxcfl, tbl->maxcflidx, tbl->mastercnt);
#endif
}

#ifdef __cplusplus
}
#endif
