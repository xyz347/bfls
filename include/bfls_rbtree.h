#ifndef __BFLS_RBTREE_H
#define __BFLS_RBTREE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 遍历顺序 */
typedef enum
{
    RB_TREE_ORDER_PRE,
    RB_TREE_ORDER_IN,
    RB_TREE_ORDER_POST
}RBORDER_E;

typedef struct rbnode_s
{
    struct rbnode_s *child[2];	/* 0:left, 1:right */
	unsigned char color;
	unsigned char lr;			/* 表征这个节点是父节点的左还是右子节点 */
    unsigned short flags;       /* FLAGS */
}rbnode_t;

typedef int  (*rbcmp)(rbnode_t *node, void* key);
typedef int  (*rbwalk)(rbnode_t *node, void* arg);
typedef void (*rbdestroy)(rbnode_t *node, void* arg);

typedef struct
{
    rbnode_t *tree;
    rbnode_t nil;
    rbcmp    cmp;
}rbtree_t;

void rbtree_init(rbtree_t *tree, rbcmp cmp);
void rbtree_add(rbtree_t *tree, rbnode_t *node, void *key);
void rbtree_destroy(rbtree_t *tree, rbdestroy pcb, void *pcbarg, RBORDER_E order);
int  rbtree_walk(rbtree_t *tree, rbwalk pcb, void *pcbarg, RBORDER_E order);
rbnode_t *rbtree_getmini(rbnode_t *node);
rbnode_t *rbtree_del(rbtree_t *tree, void* key);
rbnode_t *rbtree_get(rbtree_t *tree, void* key);

#ifdef BFLS_RBTREE_TEST
typedef char* (*rbtreeprintcb)(rbnode_t *node, void *arg);
void rbtree_print(rbtree_t *tree, FILE *fp, rbtreeprintcb pcb, void *pcbarg);
#endif


#ifdef __cplusplus
}
#endif


#endif
