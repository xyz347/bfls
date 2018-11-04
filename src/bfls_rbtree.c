#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <bfls_rbtree.h>



#ifdef __cplusplus
extern "C" {
#endif

#define RB_TREE_MAX_DEPTH   64

#define RB_TREE_COLOR_BLACK		(1)
#define RB_TREE_COLOR_RED		(0)

#define RB_TREE_LR_LEFT			(0)
#define RB_TREE_LR_RIGHT		(1)

#define RB_TREE_FLAGS_NIL       (1<<0)

#define RB_TREE_IS_NIL(node)            (0 != (RB_TREE_FLAGS_NIL&((node)->flags)))
#define RB_TREE_SET_NIL(node)           ((node)->flags |= RB_TREE_FLAGS_NIL)

#define RB_TREE_IS_BLACK(node)			(RB_TREE_COLOR_BLACK == (node)->color)
#define RB_TREE_IS_RED(node)			(RB_TREE_COLOR_RED == (node)->color)
#define RB_TREE_SET_BLACK(node)			((node)->color = RB_TREE_COLOR_BLACK)
#define RB_TREE_SET_RED(node)			((node)->color = RB_TREE_COLOR_RED)
#define RB_TREE_SET_COLOR(node, cl)	    ((node)->color = cl)
#define RB_TREE_COLOR_REVERSE(node)		((node)->color = 1-(node)->color)

#define RB_TREE_SET_LEFT(node)			((node)->lr = RB_TREE_LR_LEFT)
#define RB_TREE_SET_RIGHT(node)         ((node)->lr = RB_TREE_LR_RIGHT)
#define RB_TREE_SET_CHILD(P, L, C)      (P)->child[L]=(C); (C)->lr=(L)
#define RB_TREE_LR_INDEX(node)			((node)->lr)
#define RB_TREE_LR_OTHER(node)			(1-RB_TREE_LR_INDEX(node))
#define RB_TREE_LR_REVERSE(lr)			(1-(lr))

#define RB_TREE_BROTHER(parent, node)	((parent)->child[RB_TREE_LR_OTHER(node)])

#define RB_TREE_INIT_NODE(tree, node)       (node)->child[0]=(node)->child[1]=&(tree)->nil;(node)->flags=0;RB_TREE_SET_RED(node)

static int _rb_walk_pre(rbnode_t *node, rbwalk pcb, void *pcbarg)
{
    int ret = 0;
    if (RB_TREE_IS_NIL(node)
     || 0 != (ret=pcb(node, pcbarg))
     || 0 != (ret=_rb_walk_pre(node->child[RB_TREE_LR_LEFT], pcb, pcbarg))
     || 0 != (ret=_rb_walk_pre(node->child[RB_TREE_LR_RIGHT], pcb, pcbarg))) {
        return ret;
    } else {
        return 0;
    }
}

static int _rb_walk_in(rbnode_t *node, rbwalk pcb, void *pcbarg)
{
    int ret = 0;
    if (RB_TREE_IS_NIL(node)
     || 0 != (ret=_rb_walk_in(node->child[RB_TREE_LR_LEFT], pcb, pcbarg))
     || 0 != (ret=pcb(node, pcbarg))
     || 0 != (ret=_rb_walk_in(node->child[RB_TREE_LR_RIGHT], pcb, pcbarg))) {
        return ret;
    } else {
        return 0;
    }
}

static int _rb_walk_post(rbnode_t *node, rbwalk pcb, void *pcbarg)
{
    int ret = 0;
    if (RB_TREE_IS_NIL(node)
     || 0 != (ret=_rb_walk_post(node->child[RB_TREE_LR_LEFT], pcb, pcbarg))
     || 0 != (ret=_rb_walk_post(node->child[RB_TREE_LR_RIGHT], pcb, pcbarg))
     || 0 != (ret=pcb(node, pcbarg))) {
        return ret;
    } else {
        return 0;
    }
}

static void _rb_destroy_pre(rbnode_t *node, rbdestroy pcb, void *pcbarg)
{
    rbnode_t *left;
    rbnode_t *right;
    
    if (RB_TREE_IS_NIL(node)) {
        return;
    } else {
        left = node->child[RB_TREE_LR_LEFT];
        right = node->child[RB_TREE_LR_RIGHT];
        pcb(node, pcbarg);
        _rb_destroy_pre(left, pcb, pcbarg);
        _rb_destroy_pre(right, pcb, pcbarg);
    }
}

static void _rb_destroy_in(rbnode_t *node, rbdestroy pcb, void *pcbarg)
{
    rbnode_t *right;
    
    if (RB_TREE_IS_NIL(node)) {
        return;
    } else {
        _rb_destroy_in(node->child[RB_TREE_LR_LEFT], pcb, pcbarg);
        right = node->child[RB_TREE_LR_RIGHT];
        pcb(node, pcbarg);
        _rb_destroy_in(right, pcb, pcbarg);
    }
}

static void _rb_destroy_post(rbnode_t *node, rbdestroy pcb, void *pcbarg)
{
    if (RB_TREE_IS_NIL(node)) {
        return;
    } else {
        _rb_destroy_post(node->child[RB_TREE_LR_LEFT], pcb, pcbarg);
        _rb_destroy_post(node->child[RB_TREE_LR_RIGHT], pcb, pcbarg);
        pcb(node, pcbarg);
    }
}

void rbtree_init(rbtree_t *tree, rbcmp cmp)
{
    tree->tree = &tree->nil;
    tree->cmp = cmp;
    tree->nil.child[0] = tree->nil.child[1] = NULL;
    tree->nil.color = RB_TREE_COLOR_BLACK;
    tree->nil.lr = RB_TREE_LR_LEFT;
    tree->nil.flags = 0;
    RB_TREE_SET_NIL(&tree->nil);    
}

rbnode_t *rbtree_getmini(rbnode_t *node)
{
    rbnode_t *prev = NULL;
    
    while (!RB_TREE_SET_NIL(node)) {
        prev = node;
        node = node->child[RB_TREE_LR_LEFT];
    }
    
    return prev;
}

/*
增加一个节点
N表示新增节点， .B表示黑色， .R表示红色， .X表示黑或者红。

1. 父节点是黑色的，直接平衡   

          P.B
          / \
         /   \
       N.R   C.X
       
2. 父节点是红色的，那么:
   主要操作是为了消除“双红”，且不能破坏左右子树的黑平衡
   P 应该有父节点(根节点必须是黑色)
   P.right 应该是空(红节点孩子必须为黑 & 到叶子路径上黑节点数目必须相同) 
     假设 PP.right 为红色(空的情况请看 3.) 
     
   修改P,U,PP的颜色，如果 PP.father 为黑，完成。
   否则继续按4.处理PP

            PP.B                       PP.R                                  
             / \                        / \
            /   \                      /   \
          P.R   U.R     -------->    P.B   U.B
          / \     \                  / \     \
         /   \     \                /   \     \
       N.R   NULL  NULL           N.R   NULL  NULL
 
 
3.  
            PP.B                        P.B                                  
             / \                        / \
            /   \                      /   \
          P.R   NULL     -------->   N.R  PP.R
          / \                        / \     \
         /   \                      /   \     \
       N.R   NULL                NULL   NULL  NULL

4. 如果X是红，重复步骤2，否则步骤3（下图）         

              PPPP.B                    PPP.B
               / \                       / \
              /   \                     /   \
           PPP.R   X                PP.R   PPPP.R                               
             / \                    / \      /  \
           /     \                 /   \    /    \
        PP.R     UU.B     --->    A    B   UU.B   X
         / \      / \                      / \
        /   \    /   \                    /   \     
       A    B   C     D                  C    D
 */

void rbtree_add(rbtree_t *tree, rbnode_t *node, void *key)
{
    rbnode_t **path[RB_TREE_MAX_DEPTH];
    rbnode_t *uncle;
    rbnode_t *A;
    rbnode_t *B;
    rbnode_t *C;
    int      depth = 0;
    int      ret = 0;
	unsigned char lr; //参考lr

    RB_TREE_INIT_NODE(tree, node);

    /* 查找树插入 */
    path[depth] = &tree->tree;
    while (!RB_TREE_IS_NIL(*path[depth])) {
        A=*path[depth];
        ret = tree->cmp(A, key);
        if (ret > 0) {
            path[++depth] = &A->child[RB_TREE_LR_RIGHT];
        } else if (ret < 0) {
            path[++depth] = &A->child[RB_TREE_LR_LEFT];
        } else {
            return;
        }
    }
    *path[depth] = node;
    if (ret > 0) {
        RB_TREE_SET_RIGHT(node);
    } else {
        RB_TREE_SET_LEFT(node);
    }

    /* re-banlance*/
    while (depth>1 && RB_TREE_IS_RED(*path[depth-1]))
    {
        A = *path[depth-2];
        B = *path[depth-1];
        C = *path[depth];
        uncle = RB_TREE_BROTHER(A, B);

        /* 3. */
        if (RB_TREE_IS_BLACK(uncle)) {
            if (RB_TREE_LR_INDEX(C) == RB_TREE_LR_INDEX(B)) {/*A-B-C中B、C的左右属性一致，那么B在中间*/
                /*
                           A.B                B.B                A.B                B.B
                           / \                / \                / \                / \
                         Y.B B.R   ---->    A.R  C.R           B.R Y.B   ---->    C.R  A.R
                             / \            / \                / \                     / \
                           X.B C.R        Y.B X.B            C.R X.B                 X.B Y.B
                */
                rbnode_t *X = B->child[RB_TREE_LR_OTHER(C)];
                rbnode_t *Y = A->child[RB_TREE_LR_OTHER(C)];
				lr = RB_TREE_LR_INDEX(C);   // C的左右属性没变，以这个为参考
                
				*path[depth-2] = B; B->lr = A->lr; // 不能用RB_TREE_SET_CHILD，因为path[depth-3]可能指向tree->tree
                RB_TREE_SET_CHILD(B, RB_TREE_LR_REVERSE(lr), A); //B->A
                RB_TREE_SET_CHILD(A, lr, X); //A->X

                RB_TREE_SET_BLACK(B);
                RB_TREE_SET_RED(A);
            }
            else
            {
                /*
                           A.B                C.B               A.B                 C.B
                           / \              /     \             / \               /     \
                         B.R Y.B  ---->   B.R     A.R         Y.B B.R   ---->   A.R     B.R
                         / \             /   \   /   \            / \          /   \   /   \
                       X.B C.R         X.B  M.B N.B  Y.B        C.R X.B      Y.B  N.B M.B  X.B
                           / \                                  / \
                          M   N                                N   M
                */
                rbnode_t *X = B->child[RB_TREE_LR_INDEX(B)];
                rbnode_t *Y = A->child[RB_TREE_LR_OTHER(B)];
                rbnode_t *M = C->child[RB_TREE_LR_INDEX(B)];
                rbnode_t *N = C->child[RB_TREE_LR_OTHER(B)];
				lr = RB_TREE_LR_INDEX(B);  // B的左右属性没变，以这个为参考

                *path[depth-2] = C; C->lr=A->lr;
                RB_TREE_SET_CHILD(A, lr, N);   //A->N
                RB_TREE_SET_CHILD(C, lr, B);   //C->B
                RB_TREE_SET_CHILD(C, RB_TREE_LR_REVERSE(lr), A);   //C->A
                RB_TREE_SET_CHILD(B, RB_TREE_LR_REVERSE(lr), M);   //B->M

                RB_TREE_SET_BLACK(C);
                RB_TREE_SET_RED(A);
            }
            break;
        }
        /* 2. */
        else
        {
            RB_TREE_SET_BLACK(B);
            RB_TREE_SET_BLACK(uncle);
            RB_TREE_SET_RED(A);
            depth -= 2;
        }
    }
    RB_TREE_SET_BLACK(*path[0]);
}

/*
Del a node from a rbtree
先按照普通查找树来删除。0/1孩子直接删，2孩子用右子树最小节点替换。
A. 待删除节点是红色的，直接删除，依然平衡
B. 删除的是黑色的节点，那么黑平衡就被打破了，处理措施：
    1、如果子节点为红色，那红改黑就好了（增加本树的黑点）
    2、否则需要想办法减少兄弟树的黑点，然后继续往上处理

具体分析一下情况B。搜索树删除实际上删除的都是叶端节点（只有0/1个子树）。
另外根据红黑树的要求，该删除点不可能有黑色的子节点（黑平衡），如果是红
子节点，那么改颜色即可（B.1）。下面分析一下没有子节点的情况：
P是被删除节点的父亲节点（左边是被破坏的子树）：
            P.X
            / \
          L.B  R.X
L刚开始的时候是NIL，认为是黑色的，后续调整过程也应该是黑色的，否则直接
染红即可（B.1），无需继续调整。那么我们现在需要减少R.X子树的黑色，然后
继续处理P.X。
1. 兄弟(A)是黑色的，且孩子也都是黑色的
            P.X                   P.X
            / \                   / \
          C.B  A.B      --->    C.B  A.R
               / \                   / \
             X.B Y.B               X.B Y.B
   则把A染成红色，这样C子树和A子树的黑色点数就相同了，然后继续处理P子树：
   如果P是红色，则P变黑，平衡完成。否则继续按这几个步骤判断处理。

2. 兄弟是黑色的，至少有一个孩子是红色的
            P.X                   A.X
            / \                   / \
          C.B  A.B      --->    P.B Y.B
               / \              / \
             X.X Y.R          C.B X.X
   则把A染成父节点的颜色，把父节点染成黑色，把A的红色孩子节点也染成黑色。
   然后进行一次左/右旋。由于C子树比A子树少一层黑色，因此C和X子树的黑色是
   相等的（A是黑色），因此C和X已经平衡了。另外Y的子树（不包含Y）和C/X的黑
   色是一样多的，因此P和Y的黑色也是相等的。这样整棵树就平衡了（P下的黑色
   应该是C+1，现在达到了）

2. 兄弟是红色
            P.B                   A.B
            / \                   / \
          C.B  A.R   --->       P.R  Y.B
               / \              / \
             X.B Y.B          C.B X.B
             
   由于A子树的黑色比C子树多一层，因此X, Y肯定不是哨兵节点。这时候需要染
   色旋转。可以判断旋转之后X, Y的黑平衡依然符合要求，只需要解决C的黑平衡
   即可。这种情况就转换成了情况1/2
*/
rbnode_t *rbtree_del(rbtree_t *tree, void* key)
{
    rbnode_t **path[RB_TREE_MAX_DEPTH];
    rbnode_t *delnode = NULL;
    rbnode_t *subnode = NULL;
    rbnode_t *rmnode;
    rbnode_t *A;
    rbnode_t *P;
    rbnode_t *C;
    int      depth = 0;
    int      fdepth;
    int      ret = -1;
	unsigned char lr;

    /* 查找树删除 */
    path[depth] = &tree->tree;
    while (!RB_TREE_IS_NIL(*path[depth])){
        delnode=*path[depth];
        ret = tree->cmp(delnode, key);
        if (ret > 0) {
            path[++depth] = &delnode->child[RB_TREE_LR_RIGHT];
        } else if (ret < 0) {
            path[++depth] = &delnode->child[RB_TREE_LR_LEFT];
        } else {
            break;
        }
    }
    fdepth = depth;
    rmnode = delnode;

    if (0 != ret) {
        return NULL;
    } else if (RB_TREE_IS_NIL(delnode->child[RB_TREE_LR_LEFT])) {
        subnode = delnode->child[RB_TREE_LR_RIGHT];
		subnode->lr = rmnode->lr;
    } else if (RB_TREE_IS_NIL(delnode->child[RB_TREE_LR_RIGHT])) {
        subnode = delnode->child[RB_TREE_LR_LEFT];
		subnode->lr = rmnode->lr;
    } else {
        path[++depth] = &delnode->child[RB_TREE_LR_RIGHT];
        rmnode = delnode->child[RB_TREE_LR_RIGHT];
        while (!(RB_TREE_IS_NIL(rmnode->child[RB_TREE_LR_LEFT]))) {
            path[++depth] = &rmnode->child[RB_TREE_LR_LEFT];
            rmnode = rmnode->child[RB_TREE_LR_LEFT];
        }
        subnode = rmnode->child[RB_TREE_LR_RIGHT];
        
        /*replace the delnode*/
        rmnode->child[RB_TREE_LR_LEFT] = delnode->child[RB_TREE_LR_LEFT];
        rmnode->child[RB_TREE_LR_RIGHT] = delnode->child[RB_TREE_LR_RIGHT];
		subnode->lr = rmnode->lr;
        rmnode->lr = delnode->lr;
        rmnode->color = delnode->color;
        *path[fdepth] = rmnode;
        path[fdepth+1] = &rmnode->child[RB_TREE_LR_RIGHT]; //delnode will delete
    }
    *path[depth] = subnode;
    
    if (RB_TREE_IS_RED(rmnode)) {
        return delnode;
    }

    while (depth > 0) {
        if (RB_TREE_IS_RED(*path[depth])) {
            RB_TREE_SET_BLACK(*path[depth]);
            break;
        }
        C = *path[depth];
        P = *path[depth-1];
        A = P->child[RB_TREE_LR_OTHER(C)];
        
        if (RB_TREE_IS_RED(A)) {
            /*
              2. 兄弟是红色
                  P.B               A.B        P.B               A.B
                  / \               / \        / \               / \
                C.B  A.R   --->  P.R  Y.B    A.R  C.B   --->   Y.B  P.R
                     / \         / \         / \                    / \
                   X.B Y.B     C.B X.B     Y.B X.B                X.B C.B 
            */
            rbnode_t *X = A->child[RB_TREE_LR_INDEX(C)];
            rbnode_t *Y = A->child[RB_TREE_LR_OTHER(C)];
			lr = RB_TREE_LR_INDEX(C);

            path[depth+1] = path[depth];
            path[depth] = &A->child[RB_TREE_LR_INDEX(C)];

            *path[depth-1] = A; A->lr=P->lr;
            RB_TREE_SET_CHILD(A, lr, P);
            RB_TREE_SET_CHILD(P, RB_TREE_LR_REVERSE(lr), X);
            
            RB_TREE_SET_RED(P);
            RB_TREE_SET_BLACK(A);

            ++depth;
			A = X;
        }
        
        if (RB_TREE_IS_BLACK(A->child[RB_TREE_LR_LEFT]) && RB_TREE_IS_BLACK(A->child[RB_TREE_LR_RIGHT])) {
            /*
                P.X                   P.X
                / \                   / \
              C.B  A.B      --->    C.B  A.R
                   / \                   / \
                 X.B Y.B               X.B Y.B
            */
            RB_TREE_SET_RED(A);
            --depth;
        } else if (RB_TREE_IS_RED(A->child[RB_TREE_LR_INDEX(C)])) {// 近侄优先于远侄
            /*
                P.X              X.X                 P.X              X.X
                / \            /     \               / \            /     \
              C.B  A.B  ---> P.B     A.B          A.B  C.B  --->  A.B     P.B
                   / \       / \     / \          / \             / \     / \
                 X.R Y.X   C.B M.B N.B Y.X     Y.X X.R          Y.X N.B M.B C.B
                 / \                               / \
               M.B N.B                           N.B M.B
            */
            rbnode_t *X = A->child[RB_TREE_LR_INDEX(C)];
            rbnode_t *Y = A->child[RB_TREE_LR_OTHER(C)];
            rbnode_t *M = X->child[RB_TREE_LR_INDEX(C)];
            rbnode_t *N = X->child[RB_TREE_LR_OTHER(C)];
			lr = RB_TREE_LR_INDEX(C);

            *path[depth-1] = X; X->lr=P->lr;
            RB_TREE_SET_CHILD(A, lr, N);   // A->N
            RB_TREE_SET_CHILD(X, lr, P);   // X->P
            RB_TREE_SET_CHILD(X, RB_TREE_LR_REVERSE(lr), A);   // X->A
            RB_TREE_SET_CHILD(P, RB_TREE_LR_REVERSE(lr), M);   // P->M

            X->color = P->color;
            RB_TREE_SET_BLACK(P);
            break;
        } else { //远侄 
            /*
                P.X            A.X          P.X            A.X
                / \            / \          / \            / \
              C.B  A.B  ---> P.B Y.B      A.B  C.B  ---> Y.B P.B
                   / \       / \          / \                / \
                 X.X Y.R   C.B X.X      Y.R X.X            X.X C.B
            */
            rbnode_t *X = A->child[RB_TREE_LR_INDEX(C)];
            rbnode_t *Y = A->child[RB_TREE_LR_OTHER(C)];

            *path[depth-1] = A; A->lr=P->lr;
            RB_TREE_SET_CHILD(A, RB_TREE_LR_INDEX(C), P);   // A->P
            RB_TREE_SET_CHILD(P, RB_TREE_LR_OTHER(C), X);   // P->X

            RB_TREE_SET_COLOR(A, P->color);
            RB_TREE_SET_BLACK(P);
            break;
        }
    }
    RB_TREE_SET_BLACK(*path[0]);
    
    return delnode;
}

rbnode_t *rbtree_get(rbtree_t *tree, void* key)
{
    rbnode_t *node = tree->tree;
    int ret;

    while (!RB_TREE_IS_NIL(node)) {
        ret = tree->cmp(node, key);
        if (0 == ret) {
            return node;
        } else if (0 < ret) {
            node = node->child[RB_TREE_LR_RIGHT];
        } else {
            node = node->child[RB_TREE_LR_LEFT];
        }
    }

    return NULL;
}

int rbtree_walk(rbtree_t *tree, rbwalk pcb, void *pcbarg, RBORDER_E order)
{
    switch (order) {
      case RB_TREE_ORDER_PRE:
        return _rb_walk_pre(tree->tree, pcb, pcbarg);
      case RB_TREE_ORDER_IN:
        return _rb_walk_in(tree->tree, pcb, pcbarg);
      case RB_TREE_ORDER_POST:
      default:
        return _rb_walk_post(tree->tree, pcb, pcbarg);
    }
}


void rbtree_destroy(rbtree_t *tree, rbdestroy pcb, void *pcbarg, RBORDER_E order)
{
    switch (order) {
      case RB_TREE_ORDER_PRE:
        _rb_destroy_pre(tree->tree, pcb, pcbarg);
        break;
      case RB_TREE_ORDER_IN:
        _rb_destroy_in(tree->tree, pcb, pcbarg);
        break;
      case RB_TREE_ORDER_POST:
      default:
        _rb_destroy_post(tree->tree, pcb, pcbarg);
        break;
    }

    tree->tree = &tree->nil;
}

#ifdef BFLS_RBTREE_TEST

typedef struct
{
    rbnode_t node;
    int key;
}rbtree_test_t;

static char* printcb(rbnode_t *node, void *arg)
{
    static char buf[64];
    static char COLOR[2] = {'R', 'B'};
    rbtree_test_t *t = (rbtree_test_t*)node;

    sprintf(buf, "%d.%c", t->key, COLOR[node->color]);

    return buf;
}

static void _printtree(rbnode_t *node, int iDepth, int *piHaveY, FILE *fp, rbtreeprintcb pcb, void *pcbarg)
{
    int i;

    for (i=0; i<iDepth; ++i) {
        if (piHaveY[i] != 0) {
			fprintf(fp, "%s", "│  ");
        } else {
			fprintf(fp, "%s", "    ");
        }
    }

	if (iDepth > 0) {
        if (node->lr==RB_TREE_LR_LEFT || RB_TREE_IS_NIL(node)) {
            fprintf(fp, "%s", "└─");
        } else {
            fprintf(fp, "%s", "├─");
        }
	} else {
		fprintf(fp, "  ");
	}

    if (RB_TREE_IS_NIL(node)) {
        fprintf(fp, "%s", "NIL\r\n");
        return;
    } else {
        fprintf(fp, "%s\r\n", pcb(node, pcbarg));
		if (node->lr==RB_TREE_LR_LEFT) {
			piHaveY[iDepth] = 0;
		}
    }
	piHaveY[iDepth+1] = 1;

    _printtree(node->child[RB_TREE_LR_RIGHT], iDepth+1, piHaveY, fp, pcb, pcbarg);
    _printtree(node->child[RB_TREE_LR_LEFT], iDepth+1, piHaveY, fp, pcb, pcbarg);
}

void rbtree_print(rbtree_t *tree, FILE *fp, rbtreeprintcb pcb, void *pcbarg)
{
    int Y[RB_TREE_MAX_DEPTH] = {0};
    _printtree(tree->tree, 0, Y, fp, pcb, pcbarg);
}

static int cmp(rbnode_t *node, void* key)
{
    return (int)key - ((rbtree_test_t*)node)->key;
}

static int walkcb(rbnode_t *node, void *arg)
{
    rbtree_test_t *t = (rbtree_test_t*)node;
    printf("%d ", t->key);
    return 0;
}

static void destroycb(rbnode_t *node, void *arg)
{
    rbtree_test_t *t = (rbtree_test_t*)node;
    printf("%d ", t->key);
    node->child[0] = node->child[1] = NULL;
}

int main(int argc, char *argv[])
{
    rbtree_t tree;
    rbtree_test_t nodes[16];
    int i;

    rbtree_init(&tree, cmp);

    for (i=0; i<15; ++i)
    {
        nodes[i].key = 1*(i+1);
        rbtree_add(&tree, &nodes[i].node, (void*)(nodes[i].key));
    }
    rbtree_print(&tree, stdout, printcb, NULL);
	/*nodes[i].key = 8;
	for (i=15; i>1; --i)
		rbtree_del(&tree, (void*)i);
	rbtree_del(&tree, (void*)i);
	//rbtree_del(&tree, 15);
    rbtree_print(&tree, stdout, printcb, NULL);*/
    //rbtree_walk(&tree, walkcb, NULL, RB_TREE_ORDER_PRE);printf("\n");
    //rbtree_walk(&tree, walkcb, NULL, RB_TREE_ORDER_IN);printf("\n");
    //rbtree_walk(&tree, walkcb, NULL, RB_TREE_ORDER_POST);printf("\n");
    rbtree_destroy(&tree, destroycb, NULL, RB_TREE_ORDER_POST);printf("\n");

    return 0;
}

#endif

#ifdef __cplusplus
}
#endif


