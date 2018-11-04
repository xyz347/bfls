/* bfls_io.h */

#include <stdarg.h>

#ifndef __BFLS_IO_H
#define __BFLS_IO_H

#ifdef __cplusplus
extern "C" {
#endif

/* 通用格式化回调函数，ch是待输出的字符，parg是回调函数参数 */
typedef void (*bflsiocb)(char ch, void *parg);

/* %U传递给 bflsiousrcb 的回调参数 */
typedef struct
{
    bflsiocb    pfunc;      /* bflsiousrcb调用该函数格式化输出字符 */
    void*       cbparg;     /* 给bflsiousrcb用的回调参数 */
    void*       userarg;    /* %U需要传递给两个参数给，一个dlibiousercb, 一个 userarg*/
    int         finish;     /* 输出完成，1表示完成，0表示需要继续 */
}bflsiousr_t;

/*
  返回: 格式化输出的字符数
*/
typedef unsigned int (*bflsiousrcb)(bflsiousr_t *ctx);

unsigned int bfls_format(bflsiocb pcb, void *pcbarg, const char *fmtstr, va_list arg);

unsigned int bfls_strlen(const char *str);
unsigned int bfls_printf(const char *fmtstr, ...);
unsigned int bfls_sprintf(char *buffer, const char *fmtstr, ...);
unsigned int bfls_fprintf(FILE *fp, const char *fmtstr, ...);
unsigned int bfls_dprintf(int fd, const char *fmtstr, ...);
unsigned int bfls_snprintf(char *buffer, unsigned int max, const char *fmtstr, ...);
unsigned int bfls_vprintf(const char *fmtstr, va_list arglist);
unsigned int bfls_vsprintf(char *buffer, const char *fmtstr, va_list arglist);
unsigned int bfls_vfprintf(FILE *fp, const char *fmtstr, va_list arglist);
unsigned int bfls_vdprintf(int fd, const char *fmtstr, va_list arglist);
unsigned int bfls_vsnprintf(char *buffer, unsigned int max, const char *fmtstr, va_list arglist);

#ifdef __cplusplus
}
#endif

#endif


