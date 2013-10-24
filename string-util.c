#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include <stdarg.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


/* Base */
#define Base_2 2
#define Base_8 8
#define Base_10 10
#define Base_16 16

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1/UCHAR_MAX)                                                                      
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

#define SS (sizeof(size_t))
void *memset(void *dest, int c, size_t n)
{
	unsigned char *s = dest;
	c = (unsigned char)c;
	for (; ((uintptr_t)s & ALIGN) && n; n--) *s++ = c;
	if (n) {
		size_t *w, k = ONES * c;
		for (w = (void *)s; n>=SS; n-=SS, w++) *w = k;
		for (s = (void *)w; n; n--, s++) *s = c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	void *ret = dest;
	
	//Cut rear
	uint8_t *dst8 = dest;
	const uint8_t *src8 = src;
	switch (n % 4) {
		case 3 : *dst8++ = *src8++;
		case 2 : *dst8++ = *src8++;
		case 1 : *dst8++ = *src8++;
		case 0 : ;
	}
	
	//stm32 data bus width
	uint32_t *dst32 = (void *)dst8;
	const uint32_t *src32 = (void *)src8;
	n = n / 4;
	while (n--) {
		*dst32++ = *src32++;
	}
	
	return ret;
}

char *strchr(const char *s, int c)
{
	for (; *s && *s != c; s++);
	return (*s == c) ? (char *)s : NULL;
}

char *strcpy(char *dest, const char *src)
{
	const unsigned char *s = src;
	unsigned char *d = dest;
	while ((*d++ = *s++));
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	const unsigned char *s = src;
	unsigned char *d = dest;
	while (n-- && (*d++ = *s++));
	return dest;
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
    for ( ; n > 0; s1++, s2++, --n)
  if (*s1 != *s2)
      return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
  else if (*s1 == '\0')
      return 0;
    return 0;
}
//support from 0~999 integer to string
void Myitoa(int in_num, char *out_str){
    int tmp1, tmp2, i = 0;

    if(in_num == 0){
    out_str[i] = '0';
    i++;
    }else if(in_num > 0){
        tmp1 = in_num % 100;
        if((in_num - tmp1) / 100 != 0){
          out_str[i] = '0' + (in_num - tmp1) / 100;
          i++;
    }
    tmp2 = in_num % 10;
    if((tmp1 - tmp2) / 10 != 0){
            out_str[i] = '0' + (tmp1 - tmp2) / 10;
            i++;
    }
        out_str[i] = '0' + tmp2;
        i++;
    }
    out_str[i] = '\0';
}



//reference from zzz0072
size_t strlen(const char *string)
{
    size_t chars = 0;

    while(*string++) {
        chars++;
    }
    return chars;
}




char *strcat(char *dest, const char *src)
{
    size_t src_len = strlen(src);
    size_t dest_len = strlen(dest);

    if (!dest || !src) {
        return dest;
   }

    memcpy(dest + dest_len, src, src_len + 1);
    return dest;

}
int puts(const char *msg)
{
    if (!msg) {
       return -1;
    }

    return (int)fio_write(1, msg, strlen(msg));
}

static int printf_cb(char *dest, const char *src)
{
    return puts(src);
}

static int sprintf_cb(char *dest, const char *src)
{
    return (int)strcat(dest, src);

}
typedef int (*proc_str_func_t)(char *, const char *);

/* Common body for sprintf and printf */
static int base_printf(proc_str_func_t proc_str, \
                char *dest, const char *fmt_str, va_list param)
{
    char param_chr[] = {0, 0};
    int param_int = 0;

    char *str_to_output = 0;
    int curr_char = 0;

    /* Make sure strlen(dest) is 0
* for first strcat */
    if (dest) {
        dest[0] = 0;
    }

    /* Let's parse */
    while (fmt_str[curr_char]) {
        /* Deal with normal string
* increase index by 1 here */
        if (fmt_str[curr_char++] != '%') {
            param_chr[0] = fmt_str[curr_char - 1];
            str_to_output = param_chr;
        }
        /* % case-> retrive latter params */
        else {
            switch (fmt_str[curr_char]) {
                case 'S':
                case 's':
                    {
                        str_to_output = va_arg(param, char *);
                    }
                    break;

                case 'd':
                case 'D':
                case 'u':
                    {
                       param_int = va_arg(param, int);
                       Myitoa(param_int, str_to_output);
                       //str_to_output = itoa(param_int);
                    }
                    break;

                case 'X':
                case 'x':
                    {
                       //param_int = va_arg(param, int);
                       //str_to_output = htoa(param_int);
                    }
                    break;

                case 'c':
                case 'C':
                    {
                        param_chr[0] = (char) va_arg(param, int);
                        str_to_output = param_chr;
                        break;
                    }

                default:
                    {
                        param_chr[0] = fmt_str[curr_char];
                        str_to_output = param_chr;
                    }
            } /* switch (fmt_str[curr_char]) */
            curr_char++;
        } /* if (fmt_str[curr_char++] == '%') */
        proc_str(dest, str_to_output);
    } /* while (fmt_str[curr_char]) */

    return curr_char;
}

int sprintf(char *str, const char *format, ...)
{
    int rval = 0;
    va_list param = {0};

    va_start(param, format);
    rval = base_printf(sprintf_cb, (char *)str, format, param);
    va_end(param);

    return rval;
}

char *itoa(int integral,int base,char *string)
{
	int i=0;
     	int digit= 1;

     switch(base)
	{
	case Base_2:{
		string[i++]='0';
		string[i++]='b';
		}break;
	case Base_8:{
		string[i++]='0';
		string[i++]='o';
		}break;
	case Base_10:
		{
		string[i++]='#';		
		}break;
	case Base_16:{
		string[i++]='0';
		string[i++]='x';		
		}break;
	default:{
		string[i++]='e';
		string[i++]='r';
		string[i++]='r';
		string[i++]='o';
		string[i++]='r';
		string[i] = '\0';
		return string;
		}break;
	}
	

     while( integral >= (digit*base))
          digit*=base;
     
     while( digit >= 1 )
     {     
           if( (integral/(digit)) < 10 )
	   	string[i++] = (integral/(digit)) + '0';
           else
		string[i++] = (integral/(digit)) + 'A';
	   integral = integral % digit;
           digit/=base;
     }
           string[i] = '\0';

     return string;
}

