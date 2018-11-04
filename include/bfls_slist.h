/* 单向无序链表，如果需要用有序链表，建议使用bfls_solist.h */

#ifndef __BFLS_SLIST_H
#define __BFLS_SLIST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snode_s
{
    struct snode_s *next;
}snode_t;


typedef struct
{
    snode_t *head;
    snode_t *tail;
    int     count;
}slist_t;

/* 定义一个链表 */
#define SLIST_DEF(name)     slist_t name = {NULL, NULL, 0}
#define SLIST_FIRST(list)   ((list)->head)
#define SLIST_LAST(list)    ((list)->tail)
#define SLIST_NEXT(node)    (((snode_t*)(node))->next)

/* 插入一个节点 */
#define SLIST_INSERT(list, prev, node) \
do { \
    if (NULL != prev) {\
        ((snode_t*)(node))->next = ((snode_t*)(prev))->next;\
        ((snode_t*)(prev))->next = (snode_t*)(node);\
    } else if (NULL != (list)->head) { \
        ((snode_t*)(node))->next = (list)->head;\
        (list)->head = (snode_t*)(node);\
    } else {\
        (list)->head = (snode_t*)(node);\
    }\
    if (((snode_t*)(prev)) == (list)->tail) (list)->tail = (snode_t*)(node);\
    ++(list)->count;\
} while(0)

/* 根据cmp函数和key获取一个节点 */
#define SLIST_GET(list, cmp, key) \
({\
    snode_t *__tmp = SLIST_FIRST(list);\
    while (NULL != __tmp) {\
        if (0 == cmp(__tmp, (snode_t*)key)) {\
            break;\
        }\
        __tmp = SLIST_NEXT(__tmp);\
    }\
    __tmp;\
})

/* 获取前一个节点 */
#define SLIST_GET_PREV(list, cmp, key) \
({\
    snode_t *__tmp = SLIST_FIRST(list);\
    snode_t *__pre = SLIST_FIRST(list);\
    while (NULL != __tmp) {\
        if (0 == cmp(__tmp, (snode_t*)key)) {\
            break;\
        }\
        __pre = __tmp;\
        __tmp = SLIST_NEXT(__tmp);\
    }\
    __pre;\
})

/* 删除一个节点 */
#define SLIST_REMOVE(list, prev, node) \
do {\
    if (NULL != prev) {\
        ((snode_t*)(prev))->next = ((snode_t*)(node))->next;\
    } else {\
        (list)->head = ((snode_t*)(node))->next;\
    }\
    if (((snode_t*)(node)) == (list)->tail) (list)->tail = (snode_t*)(prev);\
    --(list)->count;\
} while(0)

/* 指定比较函数和key删除一个节点 */
#define SLIST_DEL(list, cmp, key) \
({\
    snode_t *__tmp = SLIST_FIRST(list);\
    snode_t *__pre = NULL;\
    while (NULL != __tmp) {\
        if (0 == cmp(__tmp, (snode_t*)key)) {\
            SLIST_REMOVE(list, __pre, __tmp);\
            break;\
        }\
        __pre = __tmp;\
        __tmp = SLIST_NEXT(__tmp);\
    }\
    __tmp;\
})

/* 增加一个节点 */
#define SLIST_ADD(list, node)       SLIST_INSERT(list, NULL, node)

/* 增加一个节点到队尾 */
#define SLIST_APPEND(list, node)    SLIST_INSERT(list, (list)->tail, node)

#ifdef __cplusplus
}
#endif

#endif

