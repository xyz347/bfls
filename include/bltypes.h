/* type define  */

#ifndef __BFLS_TYPES_H
#define __BFLS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BFLS_DONOT_DEFINE_UTYPES
  typedef unsigned char  uchar;
  typedef unsigned short ushort;
  typedef unsigned int   uint;
  typedef unsigned long  ulong;
#endif

#ifndef BFLS_DONOT_DEFINE_BITSTYPES
  typedef char  s8;
  typedef short s16;
  typedef int   s32;
  typedef unsigned char  u8;
  typedef unsigned short u16;
  typedef unsigned int   u32;
  #ifdef __X86_64__ //todo
  typedef long  s64;
  typedef unsigned long u64;
  #else
    #if defined(__WIN32__) || defined(WIN32)
      typedef long long s64;
      typedef unsigned long long u64;
    #else
      typedef __int64   s64;
      typedef __uint64  u64;
    #endif
  #endif
#endif

#ifdef __cplusplus
}
#endif

#endif

