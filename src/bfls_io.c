/* io.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bfls_io.h>

#ifdef __cplusplus
extern "C" {
#endif

/*支持某个%格式化换行输出，但是每次格式化允许换行的%数是有限制的*/
#define MAX_LINE_BREAK_FMT  20

#define DLFMT_MAX_UINT      ((unsigned int)(~0))

/* 前导类型 */
enum
{
    FMT_FLAGS_PLUS = 0, /*+*/
    FMT_FLAGS_MINUS,    /*-*/
    FMT_FLAGS_SPACE,    /* */
    FMT_FLAGS_POUND,    /*#*/
    FMT_FLAGS_ZERO,     /*0*/
    FMT_FLAGS_MAX
};

/* width perc type，宽度类型 */
#define WP_TYPE_NONE         '\0'   // 没指定宽度
#define WP_TYPE_ASTERISK     '*'    // 宽度在参数指定
#define WP_TYPE_DIGITAL      'd'    // 宽度用常数指定

#define DPUT(x) do{pcb((x), pcbarg); ++count;}while(0)

/*
%[flags][width][.perc] [l[l]]type
异常情况各种不同的库不一样，因为我们不追求和其他库一致，正常情况一致即可
*/
typedef struct _fmt_s
{
    char flags[FMT_FLAGS_MAX];         /* 前导，+- #0*/
    char widthtype;     // width的type，WP_TYPE_
    char perctype;      // perc的type，WP_TYPE_
    char type;          // type
    char isl;   /* is long */
    char isll;  /* is longlong 1/0 */
    unsigned int  width;
    unsigned int  perc;
    unsigned int  fllen; /* first line len，首行前面的字符数，“前面”指格式化到当前%之前输出的字符数 */
    //unsigned int  llen;

    union arg
    {
        /* 下面的各个struct名称就是type也即%之后的字母 */
        /* integer */
        struct ll
        {
            unsigned long long value;
        }d, i, u, x, X, o, p;

        /* char */
        struct c 
        {
            char value;
        }c;

        /* string */
        struct s
        {
            const char *value;
        }s;

        /* unsigned string, m/M:mac address  n/N:ipv6 address P:packet */
        struct us
        {
            const unsigned char *value;
        }m, M, n, N, P, B;

        /* ipv4 address */
        struct I
        {
            const unsigned char *ip; /* 字符串类型的，网络序，用于%lI(兼容旧的)*/
            unsigned int value;
        }I;

        /* time not support now */
        struct t
        {
            const unsigned char* DateAndTime;
            const char* timezone;
        }t;

        /* line break string，支持分行 */
        struct S
        {
            const char *string;
            char separate;
            unsigned int index; /* 指向字符的值 */
        }S;

        /* line break bitlist */
        struct L
        {
            const  unsigned char* bitlist;
            unsigned int max;
            int offset;
            unsigned int index;
            int needcomma; /* 上一行没写逗号 */
        }L;

        /* line break long list 用于那种值很大，但是值的个数有限的，比如mpls的标签 */
        struct lL
        {
            const  unsigned long* longlist;
            unsigned int num;
            unsigned int index;
            int needcomma; /* 上一行没写逗号 */
        }lL;

        /* user defined */
        struct U
        {
            bflsiousrcb usercb;
            bflsiousr_t userctx;
        }U;
    }arg;
}_fmt_t;

#define PRINTF_BUF_LEN  128

typedef struct
{
    char    buffer[PRINTF_BUF_LEN];
    unsigned int  index;
    unsigned int  fd;
}printf_arg, dprintf_arg;

typedef struct
{
    char    *buffer;
    unsigned int index;
}sprintf_arg;

typedef struct
{
    char    *buffer;
    unsigned int max;
    unsigned int index;
}snprintf_arg;

static const char *const pchex="0123456789abcdef";
static const char *const pcHEX="0123456789ABCDEF";

static void cb_printf(char ch, void *arg)
{
    printf_arg *fmt = (printf_arg*)arg;

    fmt->buffer[fmt->index++] = ch;
    if (fmt->index == PRINTF_BUF_LEN) {
        write(fmt->fd, fmt->buffer, PRINTF_BUF_LEN);
        fmt->index = 0;
    }
}

static void cb_sprintf(char ch, void *arg)
{
    sprintf_arg *fmt = (sprintf_arg*)arg;

    if (NULL != fmt->buffer) { 
        fmt->buffer[fmt->index] = ch;
    }
    fmt->index++;
}

static void cb_fprintf(char ch, void *arg)
{
    fwrite(&ch, 1, 1, (FILE*)arg);
}

static void cb_fdprintf(char ch, void *arg)
{
    write((int)arg, &ch, 1);
}

static void cb_snprintf(char ch, void *arg)
{
    snprintf_arg *fmt = (snprintf_arg*)arg;

    if (fmt->max > fmt->index) {
        fmt->buffer[fmt->index++] = ch;
    }
}

static void strfmt_init(_fmt_t *fmt)
{
    memset((void*)fmt, 0, sizeof(_fmt_t));
}

/* 读取前导 */
static const char *strfmt_get_flags(const char *fmtstr, _fmt_t *fmt)
{
    while (1) {
        switch (*fmtstr) {
          case '+':
            fmt->flags[FMT_FLAGS_PLUS] = 'y';
            break;
          case '-':
            fmt->flags[FMT_FLAGS_MINUS] = 'y';
            break;
          case ' ':
            fmt->flags[FMT_FLAGS_SPACE] = 'y';
            break;
          case '#':
            fmt->flags[FMT_FLAGS_POUND] = 'y';
            break;
          case '0':
            fmt->flags[FMT_FLAGS_ZERO] = 'y';
            break;
          default:
            return fmtstr;
        }
        ++fmtstr;
    }

    return fmtstr;
}

/* 获取width和perc */
static const char *strfmt_get_wptype(const char *fmtstr, _fmt_t *fmt)
{
    /*代码重复就重复吧*/

    if (*fmtstr == '*') {
        fmt->widthtype = WP_TYPE_ASTERISK;
        ++fmtstr;
    }
    /* 认为是digit，value是0 */
    else if (*fmtstr == '.') {
        fmt->widthtype = WP_TYPE_DIGITAL;
        fmt->width = 0;
        /*++fmtstr; DONOT change fmtstr here*/
    }
    else if (*fmtstr>='0' && *fmtstr<='9') {
        fmt->widthtype = WP_TYPE_DIGITAL;
        while (*fmtstr>='0' && *fmtstr<='9') {
            fmt->width = 10*fmt->width+(unsigned int)((*fmtstr)-'0');
            ++fmtstr;
        }
    }
    else
    {
        return fmtstr;
    }

    if (*fmtstr == '.') {
        ++fmtstr;
        if (*fmtstr == '*') {
            fmt->perctype = WP_TYPE_ASTERISK;
            ++fmtstr;
        }
        else {
            fmt->perctype = WP_TYPE_DIGITAL;
            while (*fmtstr>='0' && *fmtstr<='9') {
                fmt->perc = 10*fmt->perc+(unsigned int)((*fmtstr)-'0');
                ++fmtstr;
            }
        }
    }
    else {
        return fmtstr;
    }

    return fmtstr;
}

/* 
格式化数字 %(diuxXop)，具体请参考 bfls_format
 */
static unsigned int strfmt_digit(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    char buf[32]; /* long long最多21个 */
    char *prefix = "";
    char fill = ' ';
    unsigned int count = 0;
    unsigned int rlen = 0;      /* 实长 */
    unsigned int prelen = 0;    /* prefix len */
    unsigned int percfill = 0;  /*  */
    unsigned int filllen = 0;   /* 填充长度 */
    unsigned int i;
    unsigned int radix = 10;
    unsigned long long uvalue = pfmt->arg.d.value;
    long long value = (long long)uvalue;
    const char *pcdig = pchex;

    /* 判断前导 */
    if (pfmt->type=='d' || pfmt->type=='i') {
        if (value >= 0) {
            /* 优先+号 */
            if (pfmt->flags[FMT_FLAGS_PLUS] != '\0')
                prefix = "+";
            else if (pfmt->flags[FMT_FLAGS_SPACE] != '\0')
                prefix = " ";
        }
        else
            prefix = "-";
    }
    else {
        if (pfmt->type == 'X') {
            if (pfmt->flags[FMT_FLAGS_POUND] != '\0') 
                prefix = "0X";
            pcdig = pcHEX;
            radix = 16;
        }
        else if (pfmt->type == 'x') {
            if (pfmt->flags[FMT_FLAGS_POUND] != '\0') 
                prefix = "0x";
            radix = 16;
        }
        else if (pfmt->type == 'p') {
            radix = 16;
        }
        else if (pfmt->type == 'o') {
            if (pfmt->flags[FMT_FLAGS_POUND] != '\0') 
                prefix = "0";
            radix = 8;
        }
    }
    prelen = bfls_strlen(prefix);

    /* 计算实际的值，倒序 */
    do {
        buf[rlen++] = pcdig[uvalue%radix];
        uvalue /= radix;
    }while (uvalue > 0);

    /* 输出有可能是:
       1、左对齐
          | prefix | buf | fill( ) |
       2、右对齐
          | fill( ) | prefix | buf |
          | prefix | fill(0) | buf |
       其中buf根据prec可能需要填充0
    */

    /* 计算填充长度和填充类型 */
    if (pfmt->perctype!=WP_TYPE_NONE && pfmt->perc>rlen)
        percfill = pfmt->perc-rlen;

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > prelen+percfill+rlen))
        filllen = pfmt->width - (prelen+percfill+rlen);

    if ((pfmt->flags[FMT_FLAGS_ZERO] != '\0') && (pfmt->flags[FMT_FLAGS_MINUS] == '\0'))
        fill = '0';

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=0; i<prelen; ++i)
            DPUT(prefix[i]);
        for (i=0; i<percfill; ++i)
            DPUT('0');
        for (i=rlen; i>0; --i)
            DPUT(buf[i-1]);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        if (fill == ' ')
            for (i=0; i<filllen; ++i)
                DPUT(' ');

        for (i=0; i<prelen; ++i)
            DPUT(prefix[i]);

        if (fill == '0')
            for (i=0; i<filllen; ++i)
                DPUT('0');

        for (i=0; i<percfill; ++i)
            DPUT('0');
        for (i=rlen; i>0; --i)
            DPUT(buf[i-1]);
    }

    return count;
}

/* 格式化字符 %c */
static unsigned int strfmt_char(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    unsigned int count = 0;
    unsigned int filllen = 0;
    unsigned int i;

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > 1))
        filllen = pfmt->width - 1;

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        DPUT(pfmt->arg.c.value);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
        DPUT(pfmt->arg.c.value);
    }

    return count;
}

/* 格式化字符串 %s */
static unsigned int strfmt_string(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    unsigned int count = 0;
    unsigned int filllen = 0;
    unsigned int i;
    const char *pstr = pfmt->arg.s.value;
    unsigned int rlen = bfls_strlen(pstr);

    if ((pfmt->perctype!=WP_TYPE_NONE) && (pfmt->perc<rlen))
        rlen = pfmt->perc;

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > rlen))
        filllen = pfmt->width - rlen;

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=0; i<rlen; ++i)
            DPUT(pstr[i]);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
        for (i=0; i<rlen; ++i)
            DPUT(pstr[i]);
    }

    return count;
}

/* 格式MAC地址 %(mM) */
static unsigned int strfmt_mac(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    char buf[sizeof("HHHH.HHHH.HHHH")];
    unsigned int count = 0;
    unsigned int filllen = 0;
    unsigned int i;
    unsigned int rlen = 0;
    const unsigned char *pstr = pfmt->arg.m.value;
    const char *parg;

    if (pfmt->type == 'm') {
        parg = pchex;
    }
    else {
        parg = pcHEX;
    }

    for (i=0; i<6; ++i) {
        buf[rlen++] = parg[0xf&(pstr[i]>>4)];
        buf[rlen++] = parg[0xf&(pstr[i])];
        if (i&0x1) {
            buf[rlen++] = '.';
        }
    }
    buf[--rlen] = '\0';

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > rlen))
        filllen = pfmt->width - rlen;

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=0; i<rlen; ++i)
            DPUT(buf[i]);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
        for (i=0; i<rlen; ++i)
            DPUT(buf[i]);
    }

    return count;
}

/* 格式IPv4 %(lI|I) */
static unsigned int strfmt_ipv4(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    char buf[64];
    unsigned int i = 0;
    unsigned int rlen = 0;
    unsigned int count = 0;
    unsigned int filllen = 0;
    unsigned int value = 0;
    unsigned int ip = pfmt->arg.I.value;
    const unsigned char *ipstr = pfmt->arg.I.ip;

    for (i=0; i<4; ++i) {
        if (pfmt->isl == 0) {
            value = (unsigned int)ipstr[3-i];
        } else {
            value = ip&0xff;
            ip >>= 8;
        }
        do {
            buf[rlen++] = pchex[value%10];
            value /= 10;
        }while(value > 0);
        buf[rlen++] = '.';
    }
    --rlen;

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > rlen))
        filllen = pfmt->width - rlen;

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=rlen; i>0; --i)
            DPUT(buf[i-1]);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
        for (i=rlen; i>0; --i)
            DPUT(buf[i-1]);
    }

    return count;
}

/* 格式IPv6 %(nN) */
static unsigned int strfmt_ipv6(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    char buf[64];
    unsigned int i = 0;
    unsigned int rlen = 0;
    unsigned int count = 0;
    unsigned int filllen = 0;
    unsigned int value = 0;
    unsigned int zerolen = 0;   /* 连续0的长度 */
    unsigned int maxzero = 0;   /* 最大连续0的长度 */
    unsigned int zeroidx = 0;   /*  */
    const unsigned char *pstr = pfmt->arg.n.value;
    const char *parg;

    if (pfmt->type == 'n') {
        parg = pchex;
    }
    else {
        parg = pcHEX;
    }

    /* 查找最长连续的0(两个字节都是才算) */
    for (i=0; i<=16; i+=2) {
        if (i==16 || (pstr[i]!=0x0 || pstr[i+1]!=0x0)){
            if (maxzero < zerolen) {
                maxzero = zerolen;
                zeroidx = i-maxzero;
            }
            zerolen = 0;
        }
        else {
            zerolen += 2;
        }
    }

    /* 逆序格式化 ::A.B.C.D或者::FFFF:A.B.C.D 的情况 */
    if (zeroidx==0 && (maxzero==12 || (maxzero==10 && pstr[10]==0xff && pstr[11]==0xff))) {
        /* ipv4 section */
        for (i=15; i>11; --i) {
            value = (unsigned int)pstr[i];
            do {
                buf[rlen++] = parg[value%10];
                value /= 10;
            }while(value > 0);
            buf[rlen++] = '.';
        }
        --rlen;

        if (maxzero == 10) {
            buf[rlen++] = ':';
            memset(&buf[rlen], 'F', 4);
            rlen += 4;
        }
        buf[rlen++] = ':';
        buf[rlen++] = ':';
    }
    else {
        i = 16;
        while (i > 0) {
            /* 双引号部分 */
            if (maxzero!=0 && i==zeroidx+maxzero) {
                buf[rlen++] = ':';
                if (i == 16) {
                    buf[rlen++] = ':';
                }
                i -= maxzero;
                continue;
            }

            value = (unsigned int)pstr[i-2];
            value = (value<<8)|(unsigned int)pstr[i-1];
            value &= 0xffff;
            do {
                buf[rlen++] = parg[value%16];
                value /= 16;
            }while(value > 0);
            buf[rlen++] = ':';
            i -= 2;
        }
        if (zeroidx!=0 || maxzero==0) {
            buf[--rlen] = '\0';
        }
    }

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > rlen))
        filllen = pfmt->width - rlen;

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=rlen; i>0; --i)
            DPUT(buf[i-1]);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
        for (i=rlen; i>0; --i)
            DPUT(buf[i-1]);
    }

    return count;
}

/* 格式字节 %B，和%P类似，但是不换行不加空格 */
static unsigned int strfmt_byte(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    unsigned int i = 0;
    unsigned int rlen = pfmt->perc;
    unsigned int count = 0;
    unsigned int filllen = 0;
    const unsigned char *pstr = pfmt->arg.B.value;

    if ((pfmt->widthtype!=WP_TYPE_NONE) && (pfmt->width > rlen*2))
        filllen = pfmt->width - rlen*2;

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }

    for (i=0; i<rlen; ++i) {
        DPUT(pcHEX[0xf&(pstr[i]>>4)]);
        DPUT(pcHEX[0xf&(pstr[i])]);
    }

    /* 右对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] == '\0') {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }

    return count;
}


/* 格式内存 %P */
static unsigned int strfmt_memory(bflsiocb pcb, void *pcbarg, const _fmt_t*pfmt)
{
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int rlen = pfmt->width;
    unsigned int count = 0;
    unsigned int linelen = 0;
    unsigned int spacelen;
    const unsigned char *pstr = pfmt->arg.P.value;
    unsigned int addr = rlen+(unsigned int)pstr;
    unsigned int addrlen;

    /* 计算地址长度，至少2字节，目前最大支持4字节，64位再看 */
    if (addr > 0xffffff)
        addrlen = 8;
    else if (addr > 0xffff)
        addrlen = 6;
    else
        addrlen = 4;

    /* 强制给一个换行 */
    DPUT('\n');

    /* 前导 */
    for (i=0; i<addrlen; ++i)
        DPUT(' ');
    DPUT(' ');
    for (i=0; i<16; ++i) {
        DPUT(' ');
        DPUT(' ');
        DPUT(pchex[i]);
        if (i == 7)
            DPUT(' ');
    }
    DPUT('\n');

    /* 内容 */
    addr = (unsigned int)pstr;
    for (i=0; i<rlen; i+=16) {
        linelen = rlen-i;
        linelen = linelen>16?16:linelen;

        /* address */
        for (j=addrlen; j>0; --j)
            DPUT(pchex[0xf&(addr>>(4*j-4))]);
        DPUT(' ');

        /* hex */
        for (j=0; j<linelen; ++j) {
            DPUT(' ');
            DPUT(pchex[(pstr[i+j]>>4)&0xf]);
            DPUT(pchex[(pstr[i+j])&0xf]);
            if (j == 7)
                DPUT(' ');
        }

        /* space */
        spacelen = 3*(16-linelen)+2;
        if (j < 7)
            ++spacelen;
        for (j=0; j<spacelen; ++j)
            DPUT(' ');

        /* ASCII */
        for (j=0; j<linelen; ++j) {
            if (pstr[i+j]<127 && pstr[i+j]>31)
                DPUT(pstr[i+j]);
            else
                DPUT('.');
        }

        DPUT('\n');
        addr += 16;
    }

    return count;
}

/* 可分行格式化字符 %S 字符串中的空格保留 */
static unsigned int strfmt_lb_string(bflsiocb pcb, void *pcbarg, _fmt_t*pfmt)
{
    unsigned int count = 0;
    unsigned int filllen = 0;
    unsigned int lastsep = 0;
    unsigned int i;
    unsigned int endidx = 0;
    const char   *pstr = pfmt->arg.S.string;
    char         sep = pfmt->arg.S.separate;

    /* 计算这次输出多少字符 */
    for (i=pfmt->arg.S.index; pstr[i]!='\0'; ++i) {
        if (pstr[i] == sep)
            lastsep = i;
        if ((i-pfmt->arg.S.index) == pfmt->width)
            break;
    }

    if (pstr[i] == '\0')
        endidx = i-1;
    else if (pstr[i]==sep || lastsep==0)
        endidx = i;
    else
        endidx = lastsep;

    if (pfmt->width > (endidx-pfmt->arg.S.index))
        filllen = pfmt->width - (endidx-pfmt->arg.S.index);

    /* 左对齐 */
    if (pfmt->flags[FMT_FLAGS_MINUS] != '\0') {
        for (i=pfmt->arg.S.index; i<=endidx; ++i)
            DPUT(pstr[i]);
        for (i=0; i<filllen; ++i)
            DPUT(' ');
    }
    else {
        for (i=0; i<filllen; ++i)
            DPUT(' ');
        for (i=pfmt->arg.S.index; i<=endidx; ++i)
            DPUT(pstr[i]);
    }

    if (pstr[endidx+1]=='\0')
        pfmt->type = '\0';
    else
        pfmt->arg.S.index = endidx+1;

    return count;        
}


/* 可分行格式化bitlist %L */
static unsigned int strfmt_lb_bitlist(bflsiocb pcb, void *pcbarg, _fmt_t*pfmt)
{
    char buf[64];/*a-b,*/
    unsigned int rlen = 0;  /* a-b 的长度 */
    unsigned int count = 0;
    unsigned int value = 0;
    unsigned int i;
    unsigned int cur = pfmt->arg.L.index;
    unsigned int max = pfmt->arg.L.max;
    int offs = pfmt->arg.L.offset;
    unsigned int endidx = (unsigned int)((int)max-offs)+1; /* 用于处理最后的一段，所以+1 */
    unsigned int last = DLFMT_MAX_UINT;
    const unsigned char *pstr = pfmt->arg.L.bitlist;
    unsigned char wanted = 0x80;

    if (pfmt->arg.L.needcomma) {
        DPUT(',');
        pfmt->arg.L.needcomma = 0;
    }

    /* 格式化输出，规则有点复杂a,b-c,d:
       1、为了避免后面内容为空的情况，逗号由后面的负责输出，比如输出
          a之后不输出逗号，直到确定后面还有内容再输出
       2、为了避免死循环，即使长度不够也得输出一行
    */
    while (cur <= endidx) {
        if (cur==endidx || ((pstr[cur>>3] << (cur&0x7)) & 0x80)==wanted) {
            if (wanted == 0x80) { /* wait for 1 */
                if (cur == endidx) {  /* end */
                    break;
                }
                last = (int)cur;
                wanted = 0;

                /* 后面还有内容，可以输出逗号了 */
                if (count < pfmt->width) {
                    if (pfmt->arg.L.needcomma) {
                        DPUT(',');
                        pfmt->arg.L.needcomma = 0;
                    }
                }
                else {/* 空间不够换行 */
                    break;
                }
            }
            else {
                wanted = 0x80;
                rlen = 0;

                if (cur == endidx)
                    --cur;

                /* 逆序格式化出a-b的格式，如果是连续的两个数则格式化为a,b(比如1,2)
                   而不是1-2，因为这里有长度限制，有可能1可以输出，而1-2会过长，
                   这种情况下先格式化a，避免长度判断太复杂 */
                if (cur-last > 2) {
                    value = cur-1+(unsigned int)offs;
                    do {
                        buf[rlen++] = pchex[value%10];
                        value /= 10;
                    }while(value>0);

                    buf[rlen++] = '-';
                }
                else {
                    cur = last;
                }
                value = last+(unsigned int)offs;
                do {
                    buf[rlen++] = pchex[value%10];
                    value /= 10;
                }while(value>0);

                if (count+rlen<=pfmt->width || count==0) {
                    while (rlen > 0) {
                        DPUT(buf[rlen-1]);
                        --rlen;
                    }
                    pfmt->arg.L.needcomma = 1;
                }
                else {
                    cur = last;
                    break;
                }

                if (cur+1 >= endidx) {
                    ++cur;
                    break;
                }
            }
        }

        if (wanted==0x80 && pstr[cur>>3]==0) { /* wait for 1 */
            cur += 8;
            cur &= (~7);
        }
        else {
            ++cur;
        }
    }

    if (pfmt->width != DLFMT_MAX_UINT)
        for (i=count; i<pfmt->width; ++i)
            DPUT(' ');

    if (cur >= endidx) {
        pfmt->type = '\0';
    }
    else {
        pfmt->arg.L.index = cur;
    }

    return count;
}


/* 可分行格式化long bitlist %lL */
static unsigned int strfmt_lb_longlist(bflsiocb pcb, void *pcbarg, _fmt_t*pfmt)
{
    char buf[64];/*a-b,*/
    unsigned int rlen = 0;  /* a-b 的长度 */
    unsigned int count = 0;
    unsigned int i;
    unsigned int last = pfmt->arg.lL.index;
    unsigned int cur = last+1;
    unsigned int max = pfmt->arg.lL.num;
    const unsigned long *list = pfmt->arg.lL.longlist;
    unsigned long value = 0;

    if (pfmt->arg.lL.needcomma) {
        DPUT(',');
        pfmt->arg.lL.needcomma = 0;
    }

    /* 格式化输出，规则有点复杂a,b-c,d:
       1、为了避免后面内容为空的情况，逗号由后面的负责输出，比如输出
          a之后不输出逗号，直到确定后面还有内容再输出
       2、为了避免死循环，即使长度不够也得输出一行
    */
    while (cur <= max) {
        if (cur<max && list[cur]==list[cur-1]+1) {
            ++cur;
            continue;
        }
        else {
            --cur;
            rlen = 0;

            /* 逆序格式化出a-b的格式，如果是连续的两个数则格式化为a,b(比如1,2)
               而不是1-2，因为这里有长度限制，有可能1可以输出，而1-2会过长，
               这种情况下先格式化a，避免长度判断太复杂 */
            if (cur > last+1) {
                value = list[cur];
                do {
                    buf[rlen++] = pchex[value%10];
                    value /= 10;
                }while(value>0);

                buf[rlen++] = '-';
            }
            else {
                cur = last;
            }

            value = list[last];
            do {
                buf[rlen++] = pchex[value%10];
                value /= 10;
            }while(value>0);

            if (count+rlen<=pfmt->width || count==0) {
                while (rlen > 0) {
                    DPUT(buf[rlen-1]);
                    --rlen;
                }
                last = cur+1;
                cur = last+1;

                if (count<pfmt->width && last<max) {
                    DPUT(',');
                }
                else {
                    pfmt->arg.lL.needcomma = 1;
                    break;
                }
                
            }
            else {
                cur = last;
                break;
            }
        }
    }

    if (pfmt->width != DLFMT_MAX_UINT)
        for (i=count; i<pfmt->width; ++i)
            DPUT(' ');

    if (cur >= max) {
        pfmt->type = '\0';
    }
    else {
        pfmt->arg.lL.index = cur;
    }

    return count;
}


/* 可分行用户自定义格式化 %U */
static unsigned int strfmt_lb_user(bflsiocb pcb, void *pcbarg, _fmt_t*pfmt)
{
    unsigned int count = 0;

    count = pfmt->arg.U.usercb(&pfmt->arg.U.userctx);
    if (pfmt->arg.U.userctx.finish) {
        pfmt->type = '\0';
    }

    return count;
}


unsigned int bfls_strlen(const char *str)
{
    unsigned int len = 0;

    while (str[len] != '\0') {
        ++len;
    }

    return len;
}

/*
这个函数，注释得写多点T_T
格式化方式:%[+- #0][*|0-9][.(*|0-9)][l[l]](diuxXopcsSmMnNBPILU)
为了便于说明，给这几段起名字：
flags:[+- #0]
width:[*|0-9]
perc: [.(*|0-9)](diuxXopcsSmMnNBPILU)
llong:[l[l]]
type: (diuxXopcsSmMnNBPILU)
注意，对于%U的情况，具体如何使用flags/width/perc/llong是由自定义函数定义的，因此不在这里讨论

flags用来做对齐和前缀控制。
+对di有效。表示如果是非负数，则前面加一个'+'号。
 对di有效。表示如果是非负数，则前面加一个' '，用于输出有正有负，而正数前不想有'+'，但数据部分要对齐的情况。优先级低于'+'
#对oxX有效。用于增加前缀，o时增加'0', x时增加'0x', X时增加'0X'
-对PL无效。用于左对齐，当输出的宽度小于width的时候，后面填充空格补齐
0对diuxXop有效。表示用'0'而不是' '来填充。优先级低于'-'，也即左对齐的情况下，后面只能用空格填充。
【注意】对于'+ #'，如果是右对齐，当填充为'0'的时候，前缀放在填充('0')左边，否则放在填充(' ')的右边

width用来控制输出的宽度。如果输出的实际宽度大于width，那么按实际宽度输出，否则根据flags进行填充。
width对P无效。对于(S|[l]L)则标明一行最大输出宽度，因此%(S|[l]L)后面如果还有输出，最好显式加空格。
如果width为*，则吞噬一个参数作为长度

perc用来控制输出的长度（不计对齐和前缀）
    对于diuxXop而言，如果实际长度不小于perc，则perc不生效，否则在前面填0
    对于sB而言，perc是用来截断的，如果实际长度大于perc，则需要截断
如果perc为*，则吞噬一个参数作为长度

llong用来标明数据类型。
    对于diuxXop而言，缺省是int，l表示long, ll表示long long
    对于I而言，l表示参数是一个主机序的int，否则表示是一个网络序的unsigned char*
    对于L而言，l表示bitlist是long *，否则表示是unsigned char*
    
type（下面说的参数都不包括由于width和perc为'*'被吞噬的参数）:
d: 有符号十进制数格式     [+- 0][width][perc]
i: 同d
u: 无符号十进制数格式。   [-0][width][perc]
x: 十六进制数格式，小写   [#-0][width][perc]
X: 十六进制数格式，大写   [#-0][width][perc]
o: 八进制数格式           [#-0][width][perc]
p: 指针，十六进制数格式   [-0][width][perc]
c: 一个字符               [-][width]
s: 一个字符串，以'\0'结束 [-][width][perc]
S: 可换行的字符串，以'\0'结束，[width]
   这个需要两个参数：("%S", char *string, char seprate)
   string表示字符串指针，seprate表示分隔判定标准。一般按照空格(' ')，逗号(',')来分隔。
   并不是遇到分隔符就换行，而是在width的范围内，尽可能多的输出，比如("%8S", "abc,def,hij", ',')的输出结果是
   abc,def,
   hij
m: 输出MAC地址，HHHH.HHHH.HHHH格式，小写。[-][width]
M: 同'm'，大写
n: 输出IPv6，小写。[-][width]
N: 同'n'，大写
B: 把unsigned char*格式化成十六进制串输出。[-][width][perc]
P: 打印内存内容。所有flags无效，width必须指定，也即: width
I: 输出IPv4，点分十进制格式，如果是%lI，则参数是一个主机序的unsigned int。如果是%I，参数是一个网络序的unsigned char*
L: 输出bitlist，所有flags无效，也即[width]，有两种调用方式
   ("%L", const unsigned char *clist, unsigned int max, int offset);
   ("%lL", const unsigned long *ulist, unsigned int count);
   clist表示是字符串的bitlist，从第0bit开始检测。offset表示值和bit索引的偏差，比如bit0表示1则offset为1。
        max表示最大的值，是加上offset之后的。
   ulist是一个unsigned long的数组，必须是有序的。count用来表示数组元素的个数。一般在值非常大但是值的个数
        不多的情况下使用这个方式。比如值的范围是0-(2^32-1)，但是一般值的个数只有很有限的个数。那么如果用
        %L的方式，那clist需要(2^32)/8=512M的大小，而且遍历2^32次效率基本是不能忍受的。
   %[l]L是支持换行的，每行输出不超过width，如果width小于2，则认为不换行。
U: 用户自定义输出格式

P输出的格式比较特殊，没16字节会自动换行，因此一般不建议和其他的格式化参数一块使用。
SL支持换行输出。换行之后也会自动对齐，比如
id     name      description       member
-----------------------------------------------------
1      Tool-soft The dev-tool      XMZhang, XMWang, 
                 development       XMLi, XMChen
                 department 
在输出后面的信息只需要("%-7d%-10s%16S  %16S\n", id, name, des, ' ', member, ',');即可。
注意description对应的是"%16S  "而不是"%18S"，因为用"%18S"可能会导致description和member
粘连在一起。
*/
unsigned int bfls_format(bflsiocb pcb, void *pcbarg, const char *fmtstr, va_list arg)
{
    _fmt_t afmt[MAX_LINE_BREAK_FMT+1];
    _fmt_t *pfmt;
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int nextfmt = 0;   /* 下一个可用的fmt，用来支持分行 */
    unsigned int tlen = 0;
    unsigned int count = 0;     /* 总的字符数目 */
    unsigned int linelen = 0;   /* 当前行的字符数目 */

    while (*fmtstr != '\0') {
        if (*fmtstr != '%') {
            DPUT(*fmtstr);
            ++linelen;
            if (*fmtstr == '\n')
                linelen = 0;
            ++fmtstr;
            continue;
        }
        ++fmtstr;

        pfmt = &afmt[nextfmt];
        strfmt_init(pfmt);
        fmtstr = strfmt_get_flags(fmtstr, pfmt);
        fmtstr = strfmt_get_wptype(fmtstr, pfmt);
        if (pfmt->widthtype == WP_TYPE_ASTERISK)
            pfmt->width = va_arg(arg, unsigned int);
        if (pfmt->perctype == WP_TYPE_ASTERISK)
            pfmt->perc = va_arg(arg, unsigned int);

        if (*fmtstr == 'l') {
            ++fmtstr;
            pfmt->isl = 'y';
            if (*fmtstr == 'l') {
                pfmt->isll = 'y';
                ++fmtstr;
            }
        }

        pfmt->type = *fmtstr;
        switch (*fmtstr) {
          case 'd':
          case 'i':
          case 'u':
          case 'x':
          case 'X':
          case 'o':
          case 'p':
            if (pfmt->isll != '\0') {
                pfmt->arg.d.value = va_arg(arg, unsigned long long);
            } else if (pfmt->isl != '\0') {
                pfmt->arg.d.value = (unsigned long long)va_arg(arg, unsigned long);
            } else {
                pfmt->arg.d.value = (unsigned long long)va_arg(arg, int);
            }
            tlen = strfmt_digit(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 'c':
            pfmt->arg.c.value = (char)va_arg(arg, int);
            tlen = strfmt_char(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 's':
            pfmt->arg.s.value = va_arg(arg, const char*);
            if (NULL == pfmt->arg.s.value)
                pfmt->arg.s.value= "(null)";
            tlen = strfmt_string(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 'm':
          case 'M':
            pfmt->arg.m.value = va_arg(arg, const unsigned char*);
            if (NULL == pfmt->arg.m.value)
                pfmt->arg.m.value= "\0\0\0\0\0\0";
            tlen = strfmt_mac(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 'n':
          case 'N':
            pfmt->arg.n.value = va_arg(arg, const unsigned char*);
            if (NULL == pfmt->arg.n.value) {
                DPUT(':');
                DPUT(':');
                break;
            }
            tlen = strfmt_ipv6(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 'B':
            pfmt->arg.B.value = va_arg(arg, const unsigned char*);
            if (NULL == pfmt->arg.B.value) {
                pfmt->perc = 0;
            }
            tlen = strfmt_byte(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 'P':
            pfmt->arg.P.value = va_arg(arg, const unsigned char*);
            if (NULL == pfmt->arg.P.value) {
                pfmt->width = 0;
            }
            tlen = strfmt_memory(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          case 'I':
            if (pfmt->isl) {
                pfmt->arg.I.value = va_arg(arg, unsigned int);
            } else {
                pfmt->arg.I.ip = va_arg(arg, unsigned char*);
            }
            tlen = strfmt_ipv4(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            break;

          /* 以下是支持分行显示的 */
          case 'S':
            pfmt->arg.S.string = va_arg(arg, const char*);
            pfmt->arg.S.separate = (char)va_arg(arg, int);
            if (NULL == pfmt->arg.S.string)
                pfmt->arg.S.string = "(null)";
            if (pfmt->width == 0)
                pfmt->width = 1; /* 防止死循环 */
            pfmt->fllen = linelen;
            pfmt->arg.S.index = 0;
            tlen = strfmt_lb_string(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            if (pfmt->type!='\0' && nextfmt<MAX_LINE_BREAK_FMT)
                ++nextfmt;
            break;

          case 'L':
            if (pfmt->isl == '\0') {
                pfmt->arg.L.bitlist = va_arg(arg, const unsigned char*);
                pfmt->arg.L.max = va_arg(arg, unsigned int);
                pfmt->arg.L.offset = va_arg(arg, int);
                if (NULL == pfmt->arg.L.bitlist) {
                    pfmt->arg.L.bitlist = (const unsigned char*)"";
                    pfmt->arg.L.max = 0;
                    pfmt->arg.L.offset = 0;
                }
                if (pfmt->width < 2)
                    pfmt->width = DLFMT_MAX_UINT; /* 不分行 */
                if (pfmt->arg.L.offset>0 && (unsigned int)pfmt->arg.L.offset>pfmt->arg.L.max)
                    pfmt->arg.L.offset = 0;
                pfmt->fllen = linelen;
                pfmt->arg.L.index = 0;
                tlen = strfmt_lb_bitlist(pcb, pcbarg, pfmt);
            }
            else {
                pfmt->arg.lL.longlist = va_arg(arg, const unsigned long*);
                pfmt->arg.lL.num = va_arg(arg, unsigned int);
                if (NULL == pfmt->arg.lL.longlist) {
                    pfmt->arg.lL.num = 0;
                }
                pfmt->fllen = linelen;
                pfmt->arg.lL.index = 0;
                tlen = strfmt_lb_longlist(pcb, pcbarg, pfmt);
            }
            count += tlen;
            linelen += tlen;
            if (pfmt->type!='\0' && nextfmt<MAX_LINE_BREAK_FMT)
                ++nextfmt;
            break;

          case 'U':
            pfmt->arg.U.usercb = va_arg(arg, bflsiousrcb);
            pfmt->arg.U.userctx.pfunc = pcb;
            pfmt->arg.U.userctx.cbparg = pcbarg;
            pfmt->arg.U.userctx.userarg = va_arg(arg, void*);
            pfmt->arg.U.userctx.finish = 0;
            pfmt->fllen = linelen;
            tlen = strfmt_lb_user(pcb, pcbarg, pfmt);
            count += tlen;
            linelen += tlen;
            if (pfmt->type!='\0' && nextfmt<MAX_LINE_BREAK_FMT)
                ++nextfmt;
            break;

          default:
            DPUT(*fmtstr);
            ++linelen;
            break;
        }

        ++fmtstr;
    }

    do {
        linelen = 0;

        for (i=0; i<nextfmt; ++i) {
            pfmt = &afmt[i];
            if (pfmt->type == '\0')
                continue;
            for (j=linelen; j<pfmt->fllen; ++j)
                DPUT(' ');
            linelen = pfmt->fllen;

            switch (pfmt->type) {
              case 'S':
                tlen = strfmt_lb_string(pcb, pcbarg, pfmt);
                count += tlen;
                linelen += tlen;
                break;

              case 'L':
                if (pfmt->isl == '\0')
                    tlen = strfmt_lb_bitlist(pcb, pcbarg, pfmt);
                else
                    tlen = strfmt_lb_longlist(pcb, pcbarg, pfmt);
                count += tlen;
                linelen += tlen;
                break;

              case 'U':
                tlen = strfmt_lb_user(pcb, pcbarg, pfmt);
                count += tlen;
                linelen += tlen;
                break;

              default:
                break;
            }
        }
        if (linelen != 0)
            DPUT('\n');
    }while (linelen != 0);

    return count;
}

unsigned int bfls_printf(const char *fmtstr, ...)
{
    va_list         arglist;
    printf_arg     fmtarg;
    unsigned int   len;

    fmtarg.index = 0;
    fmtarg.fd = 1; /* fileno(stdout) */

    va_start(arglist, fmtstr);
    len = bfls_format(cb_printf, &fmtarg, fmtstr, arglist);
    va_end(arglist);

    if (fmtarg.index != 0) {
        write(fmtarg.fd, fmtarg.buffer, fmtarg.index);
    }

    return len;
}

unsigned int bfls_sprintf(char *buffer, const char *fmtstr, ...)
{
    va_list         arglist;
    sprintf_arg     fmtarg;

    fmtarg.buffer = buffer;
    fmtarg.index = 0;

    va_start(arglist, fmtstr);
    (void)bfls_format(cb_sprintf, &fmtarg, fmtstr, arglist);
    va_end(arglist);

    if (NULL != buffer) {
        buffer[fmtarg.index] = '\0';
    }

    return fmtarg.index;
}

unsigned int bfls_fprintf(FILE *fp, const char *fmtstr, ...)
{
    va_list arglist;
    unsigned int len;

    va_start(arglist, fmtstr);
    len = bfls_format(cb_fprintf, (void*)fp, fmtstr, arglist);
    va_end(arglist);

    return len;
}

unsigned int bfls_dprintf(int fd, const char *fmtstr, ...)
{
    va_list         arglist;
    dprintf_arg    fmtarg;
    unsigned int    len;

    fmtarg.index = 0;
    fmtarg.fd = fd;

    va_start(arglist, fmtstr);
    len = bfls_format(cb_printf, &fmtarg, fmtstr, arglist);
    va_end(arglist);

    if (fmtarg.index != 0) {
        write(fmtarg.fd, fmtarg.buffer, fmtarg.index);
    }

    return len;
}

unsigned int bfls_snprintf(char *buffer, unsigned int max, const char *fmtstr, ...)
{
    va_list         arglist;
    snprintf_arg    fmtarg;

    if (NULL==buffer || 0==max) {
        return 0;
    } else if (1 == max) {
        buffer[0] = '\0';
        return 0;
    }

    fmtarg.buffer = buffer;
    fmtarg.max = max-1;
    fmtarg.index = 0;

    va_start(arglist, fmtstr);
    (void)bfls_format(cb_snprintf, &fmtarg, fmtstr, arglist);
    va_end(arglist);

    buffer[fmtarg.index] = '\0';

    return fmtarg.index;
}

unsigned int bfls_vprintf(const char *fmtstr, va_list arglist)
{
    printf_arg     fmtarg;
    unsigned int   len;

    fmtarg.index = 0;
    fmtarg.fd = 1; /* fileno(stdout) */

    len = bfls_format(cb_printf, &fmtarg, fmtstr, arglist);

    if (fmtarg.index != 0) {
        write(fmtarg.fd, fmtarg.buffer, fmtarg.index);
    }

    return len;
}

unsigned int bfls_vsprintf(char *buffer, const char *fmtstr, va_list arglist)
{
    sprintf_arg     fmtarg;

    fmtarg.buffer = buffer;
    fmtarg.index = 0;

    (void)bfls_format(cb_sprintf, &fmtarg, fmtstr, arglist);

    if (NULL != buffer) {
        buffer[fmtarg.index] = '\0';
    }

    return fmtarg.index;
}

unsigned int bfls_vfprintf(FILE *fp, const char *fmtstr, va_list arglist)
{
    unsigned int len;

    len = bfls_format(cb_fprintf, (void*)fp, fmtstr, arglist);

    return len;
}

unsigned int bfls_vdprintf(int fd, const char *fmtstr, va_list arglist)
{
    dprintf_arg    fmtarg;
    unsigned int    len;

    fmtarg.index = 0;
    fmtarg.fd = fd;

    len = bfls_format(cb_printf, &fmtarg, fmtstr, arglist);

    if (fmtarg.index != 0) {
        write(fmtarg.fd, fmtarg.buffer, fmtarg.index);
    }

    return len;
}

unsigned int bfls_vsnprintf(char *buffer, unsigned int max, const char *fmtstr, va_list arglist)
{
    snprintf_arg    fmtarg;

    if (NULL==buffer || 0==max) {
        return 0;
    } else if (1 == max) {
        buffer[0] = '\0';
        return 0;
    }

    fmtarg.buffer = buffer;
    fmtarg.max = max-1;
    fmtarg.index = 0;

    (void)bfls_format(cb_snprintf, &fmtarg, fmtstr, arglist);

    buffer[fmtarg.index] = '\0';

    return fmtarg.index;
}


#ifdef DLIB_STR_FMT_TEST
#include <dlibbit.h>
#include <dl_sprintf.h>
#include <dl_printf.h>

typedef struct
{
    const unsigned char portlist[64];
    int offset;
    unsigned int width;
    unsigned int index;
}portnamelist_t;

static unsigned int strfmt_user(bflsiousr_t *ctx)
{
    /* 1-24:Fe0/1/1-24 25-28:Fe0/2/1-4 29-30:Ge0/1/1-2 */

    portnamelist_t *pport = (portnamelist_t *)ctx->userarg;
    unsigned int count = 0;
    unsigned int len = 0;
    unsigned int prlen = 0;
    unsigned int offset = 0;
    unsigned int i,j = 0;
    char buf[64];
    char *p;
    unsigned char tmp;

    while (pport->index <= 30) {
        if (pport->index <= 24) {
            len += dl_sprintf(&buf[len], "%s", "Fe0/1/");
            prlen = len;
            len += dl_sprintf(&buf[len], "%L", pport->portlist, 24, 1);
            offset = 0;
        }
        else if (pport->index <= 28) {
            tmp = pport->portlist[3];
            len += dl_sprintf(&buf[len], "%s", "Fe0/2/");
            prlen = len;
            len += dl_sprintf(&buf[len], "%L", &tmp, 4, 1);
            offset = 24;
        }
        else {
            tmp = pport->portlist[3];
            tmp &= 0xf;
            len += dl_sprintf(&buf[len], "%s", "Ge0/2/");
            prlen = len;
            len += dl_sprintf(&buf[len], "%L", &tmp, 2, -3);
            offset = 28;
        }

        if (len != prlen) {
            if (count+len <= pport->width) {
                for (i=0; i<len; ++i)
                    ctx->pfunc(buf[i], ctx->cbparg);
                count += len;
            }
            else {
                i = len;
                while (i > 0) {
                    if (buf[i-1] = ',') {
                        if (count+i <= pport->width) {
                            buf[i-1] = '\0';
                        }
                    }
                }
            }
        }

        len = 0;
        prlen = 0;
    }

    return count;
}

int main(int argc, char **argv)
{

    int i;
    unsigned char bl[256] = {0};
    unsigned long ll[256] = {0};

    dl_printf("%s",
              "index  name                type  declare\n"
              "----------------------------------------\n");
    /*memset(bl, 0, sizeof(bl));
    for (i=0; i<16; ++i) {
        ll[i]=i+5;
        DLIB_BITSET(bl, ll[i]);
    }
    for (i=16; i<30; ++i){
        ll[i] = 50+i*3;
        DLIB_BITSET(bl, ll[i]);
    }
    DLIB_BITCLR(bl, ll[19]);DLIB_BITCLR(bl, ll[20]);DLIB_BITCLR(bl, ll[22]);
    ll[19] = 105;ll[20] = 106;ll[22] = 114;
    DLIB_BITSET(bl, ll[19]);DLIB_BITSET(bl, ll[20]);DLIB_BITSET(bl, ll[22]);
    for (i=0; i<2; ++i)
        dl_printf("%-7d%18lL  %-6s%20lL\n",i,
                  ll, 30, "nice", ll, 30);

    for (i=0; i<2; ++i)
        dl_printf("%-7d%18L  %-6s%20L\n",i,
                  bl, 150, 0, "nice", bl, 150, 0);
    for (i=0; i<4; ++i)
        dl_printf("%-7d%-18S  %-6s%20S\n",i, //
                  "This is a nice day. Isn't it? Oh my god "
                  "The data model described in this memo is defined in the following", ' ',
                  "Nice",
                  "This YANG module imports typedefs from [RFC6021] and references"
                  "[RFC4741], [RFC4742], [RFC4743], [RFC4744], [RFC5539], [xmlschema-1],"
                  "[RFC6020], [ISO/IEC19757-2:2008], and [RFC5717].\n", ' ');

    memset(bl, 0xAA, 5);
    memset(bl+5, 0xFF, 3);
    memset(bl+8, 0xa4, 4);
    dl_printf("%s",
              "index  name                type  declare\n"
              "----------------------------------------\n");
    for (i=0; i<4; ++i)
        dl_printf("%-7d%18L  %-6s%20L\n", i, //
                  bl, 96, 1, "nice", bl, 24, 1);

    memset(bl, 0, 16);bl[1]=1;
    memset(&bl[16], 0xff, 16);
    memset(&bl[32], 0x00, 12);memset(&bl[44], 0xac, 4);
    memset(&bl[48], 0x00, 10);memset(&bl[58], 0xff, 2);memset(&bl[60], 0x1, 4);
    memset(&bl[64], 0x00, 2);bl[66]=0x0f;memset(&bl[67], 0xac, 13);
    memset(&bl[80], 0xac, 13);bl[93]=0xf;bl[94]=0x0;bl[95]=0x0;

    for (i=0; i<81; i+=16)
        dl_printf("%-4d%.16B\n", i/16, &bl[i]); 

    for (i=0; i<4; ++i) {
        ++bl[67];
        dl_printf("%-4d%lI\n", i, &bl[67]); 
    }*/
    for (i=0; i<63; ++i) bl[i] = 31+i;
    dl_printf("%*P\n", 63, bl);

    return 0;
}
#endif

#ifdef __cplusplus
}
#endif

