#ifndef __BFLS_BITS_H
#define __BFLS_BITS_H

#ifdef __cplusplus
extern "C" {
#endif


#define BFLS_BITSET(list, index) (((unsigned char*)(list))[(index)/8] |= (0xff&(1<<(7-((index)&7)))))
#define BFLS_BITCLR(list, index) (((unsigned char*)(list))[(index)/8] &= ~(0xff&(1<<(7-((index)&7)))))
#define BFLS_BITTST(list, index) ((unsigned int)((((unsigned char*)(list))[(index)/8])>>(7-((index)&7))))


#ifdef __cplusplus
}
#endif


#endif


