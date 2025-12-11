/**
 * @file cddlib.c
 * @brief CDD 标准库实现
 * @author mozihe
 * 
 * 使用 Linux 系统调用实现标准库函数，不依赖 libc。
 */

#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

// 输出缓冲区
static char _cdd_out_buf[4096];
static int _cdd_out_pos = 0;

// 刷新输出缓冲区
static void _cdd_flush(void) {
    if (_cdd_out_pos > 0) {
        ssize_t written = write(1, _cdd_out_buf, _cdd_out_pos);
        (void)written;
        _cdd_out_pos = 0;
    }
}

// 输出单个字符到缓冲区
static void _cdd_putc(char c) {
    if (_cdd_out_pos >= 4095) {
        _cdd_flush();
    }
    _cdd_out_buf[_cdd_out_pos++] = c;
    if (c == '\n') {
        _cdd_flush();
    }
}

// 输出字符串
static void _cdd_puts(const char* s) {
    while (*s) {
        _cdd_putc(*s++);
    }
}

// 输出无符号整数
static void _cdd_putu(unsigned long n, int base, int uppercase) {
    char buf[32];
    int i = 0;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    if (n == 0) {
        _cdd_putc('0');
        return;
    }
    
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    
    while (--i >= 0) {
        _cdd_putc(buf[i]);
    }
}

// 输出有符号整数
static void _cdd_putd(long n) {
    if (n < 0) {
        _cdd_putc('-');
        n = -n;
    }
    _cdd_putu((unsigned long)n, 10, 0);
}

/* ============================================================================
 * printf 实现
 * ============================================================================ */

/**
 * @brief 格式化输出到标准输出
 * 
 * 支持的格式说明符：
 *   %d, %i  - 有符号十进制整数
 *   %u      - 无符号十进制整数
 *   %x      - 十六进制（小写）
 *   %X      - 十六进制（大写）
 *   %c      - 字符
 *   %s      - 字符串
 *   %p      - 指针
 *   %%      - 输出 %
 *   %ld     - 长整数
 *   %lu     - 无符号长整数
 * 
 * @param format 格式字符串
 * @param ... 可变参数
 * @return 输出的字符数
 */
int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int count = 0;
    const char* p = format;
    
    while (*p) {
        if (*p != '%') {
            _cdd_putc(*p++);
            count++;
            continue;
        }
        
        p++;  // skip '%'
        
        // 检查长度修饰符
        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }
        
        switch (*p) {
            case 'd':
            case 'i':
                if (is_long) {
                    _cdd_putd(va_arg(args, long));
                } else {
                    _cdd_putd(va_arg(args, int));
                }
                break;
                
            case 'u':
                if (is_long) {
                    _cdd_putu(va_arg(args, unsigned long), 10, 0);
                } else {
                    _cdd_putu(va_arg(args, unsigned int), 10, 0);
                }
                break;
                
            case 'x':
                if (is_long) {
                    _cdd_putu(va_arg(args, unsigned long), 16, 0);
                } else {
                    _cdd_putu(va_arg(args, unsigned int), 16, 0);
                }
                break;
                
            case 'X':
                if (is_long) {
                    _cdd_putu(va_arg(args, unsigned long), 16, 1);
                } else {
                    _cdd_putu(va_arg(args, unsigned int), 16, 1);
                }
                break;
                
            case 'c':
                _cdd_putc((char)va_arg(args, int));
                count++;
                break;
                
            case 's': {
                const char* s = va_arg(args, const char*);
                if (s) {
                    _cdd_puts(s);
                } else {
                    _cdd_puts("(null)");
                }
                break;
            }
            
            case 'p': {
                void* ptr = va_arg(args, void*);
                _cdd_puts("0x");
                _cdd_putu((unsigned long)ptr, 16, 0);
                break;
            }
            
            case '%':
                _cdd_putc('%');
                count++;
                break;
                
            default:
                _cdd_putc('%');
                _cdd_putc(*p);
                count += 2;
                break;
        }
        p++;
    }
    
    _cdd_flush();
    va_end(args);
    return count;
}

/* ============================================================================
 * 输入函数
 * ============================================================================ */

// 输入缓冲区
static char _cdd_in_buf[4096];
static int _cdd_in_pos = 0;
static int _cdd_in_len = 0;

// 从标准输入读取一个字符
static int _cdd_getc(void) {
    if (_cdd_in_pos >= _cdd_in_len) {
        _cdd_in_len = read(0, _cdd_in_buf, sizeof(_cdd_in_buf));
        _cdd_in_pos = 0;
        if (_cdd_in_len <= 0) return -1;
    }
    return (unsigned char)_cdd_in_buf[_cdd_in_pos++];
}

// 跳过空白字符
static void _cdd_skip_whitespace(void) {
    int c;
    while ((c = _cdd_getc()) != -1) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            _cdd_in_pos--;  // 退回非空白字符
            break;
        }
    }
}

// 读取整数
static int _cdd_read_int(int* result) {
    _cdd_skip_whitespace();
    
    int c = _cdd_getc();
    if (c == -1) return 0;
    
    int sign = 1;
    if (c == '-') {
        sign = -1;
        c = _cdd_getc();
    } else if (c == '+') {
        c = _cdd_getc();
    }
    
    if (c < '0' || c > '9') {
        _cdd_in_pos--;
        return 0;
    }
    
    int n = 0;
    while (c >= '0' && c <= '9') {
        n = n * 10 + (c - '0');
        c = _cdd_getc();
    }
    
    if (c != -1) _cdd_in_pos--;  // 退回非数字字符
    *result = n * sign;
    return 1;
}

/**
 * @brief 格式化输入
 * 
 * 支持的格式说明符：
 *   %d      - 读取整数
 *   %c      - 读取字符
 *   %s      - 读取字符串（到空白为止）
 * 
 * @param format 格式字符串
 * @param ... 指针参数
 * @return 成功读取的项数
 */
int scanf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int count = 0;
    const char* p = format;
    
    while (*p) {
        if (*p != '%') {
            // 跳过格式字符串中的普通字符
            if (*p == ' ' || *p == '\t' || *p == '\n') {
                _cdd_skip_whitespace();
            }
            p++;
            continue;
        }
        
        p++;  // skip '%'
        
        switch (*p) {
            case 'd': {
                int* ptr = va_arg(args, int*);
                if (_cdd_read_int(ptr)) {
                    count++;
                } else {
                    goto end;
                }
                break;
            }
            
            case 'c': {
                char* ptr = va_arg(args, char*);
                int c = _cdd_getc();
                if (c == -1) goto end;
                *ptr = (char)c;
                count++;
                break;
            }
            
            case 's': {
                char* ptr = va_arg(args, char*);
                _cdd_skip_whitespace();
                int i = 0;
                int c;
                while ((c = _cdd_getc()) != -1) {
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                        _cdd_in_pos--;
                        break;
                    }
                    ptr[i++] = (char)c;
                }
                ptr[i] = '\0';
                if (i > 0) count++;
                break;
            }
            
            default:
                break;
        }
        p++;
    }
    
end:
    va_end(args);
    return count;
}

/* ============================================================================
 * 基本 I/O 函数
 * ============================================================================ */

/**
 * @brief 输出字符
 */
int putchar(int c) {
    char ch = (char)c;
    ssize_t written = write(1, &ch, 1);
    (void)written;
    return c;
}

/**
 * @brief 读取字符
 */
int getchar(void) {
    return _cdd_getc();
}

/**
 * @brief 输出字符串并换行
 */
int puts(const char* s) {
    _cdd_puts(s);
    _cdd_putc('\n');
    _cdd_flush();
    return 1;
}

/* ============================================================================
 * 字符串函数
 * ============================================================================ */

/**
 * @brief 字符串长度
 */
int strlen(const char* s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

/**
 * @brief 字符串拷贝
 */
char* strcpy(char* dest, const char* src) {
    char* ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

/**
 * @brief 字符串比较
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/**
 * @brief 字符串连接
 */
char* strcat(char* dest, const char* src) {
    char* ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
}

/* ============================================================================
 * 内存函数
 * ============================================================================ */

/**
 * @brief 内存拷贝
 */
void* memcpy(void* dest, const void* src, unsigned long n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

/**
 * @brief 内存设置
 */
void* memset(void* s, int c, unsigned long n) {
    char* p = (char*)s;
    while (n--) *p++ = (char)c;
    return s;
}

/* ============================================================================
 * 内存管理（简单实现，使用 brk 系统调用）
 * ============================================================================ */

static void* _cdd_heap_end = 0;

/**
 * @brief 分配内存
 */
void* malloc(unsigned long size) {
    if (_cdd_heap_end == 0) {
        _cdd_heap_end = (void*)syscall(SYS_brk, 0);
    }
    
    void* ptr = _cdd_heap_end;
    void* new_end = (void*)syscall(SYS_brk, (char*)_cdd_heap_end + size);
    
    if (new_end == _cdd_heap_end) {
        return 0;  // 分配失败
    }
    
    _cdd_heap_end = new_end;
    return ptr;
}

/**
 * @brief 释放内存（简单实现，不做实际释放）
 */
void free(void* ptr) {
    (void)ptr;  // 简单实现不回收
}

/* ============================================================================
 * 程序控制
 * ============================================================================ */

/**
 * @brief 退出程序
 */
void exit(int status) {
    _cdd_flush();
    syscall(SYS_exit, status);
    __builtin_unreachable();
}

/* ============================================================================
 * 数学函数
 * ============================================================================ */

/**
 * @brief 绝对值
 */
int abs(int n) {
    return n < 0 ? -n : n;
}
