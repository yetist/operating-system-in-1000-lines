#pragma once

typedef int bool;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#if defined(__LP64__)
typedef unsigned long int   uint64_t;
typedef unsigned long int   uintptr_t;
typedef uint64_t            size_t;
typedef uint64_t            paddr_t;
typedef uint64_t            vaddr_t;
#else
typedef unsigned long long  uint64_t;
typedef unsigned long long  uintptr_t;
typedef uint32_t            size_t;
typedef uint32_t            paddr_t;
typedef uint32_t            vaddr_t;
#endif

#define true  1
#define false 0
#define NULL  ((void *) 0)
#define align_up(value, align) (((value) + ((align) - 1)) & ~((align) - 1))
#define is_aligned(value, align) (((value) & ((align) - 1)) == 0)
#define offsetof(type, member)   __builtin_offsetof(type, member)
#define va_list  __builtin_va_list
#define va_start __builtin_va_start
#define va_end   __builtin_va_end
#define va_arg   __builtin_va_arg
#define PAGE_SIZE 4096
#define SYS_PUTCHAR 1
#define SYS_GETCHAR 2
#define SYS_EXIT    3
#define SYS_READFILE  4
#define SYS_WRITEFILE 5

#define DBAR(hint) __asm__ __volatile__("dbar %0 " : : "I"(hint) : "memory")
#define rmb()   DBAR(0b00101)
#define wmb()   DBAR(0b01010)

/*
 * LoongArch direct mapping windows configured in boot.S:
 *   DMW0: 0x8000_0000_0000_0000 + phys, uncached (MMIO)
 *   DMW1: 0x9000_0000_0000_0000 + phys, coherent cached (RAM)
 * The low 48 bits of a kernel virtual address are its physical address.
 */
#define DMW_UC_BASE   0x8000000000000000ULL
#define DMW_CC_BASE   0x9000000000000000ULL
#define PHYS_ADDR_MASK 0x0000ffffffffffffULL

#define virt_to_phys(v)     ((uint64_t)(v) & PHYS_ADDR_MASK)
#define phys_to_virt(p)     ((uint64_t)(p) | DMW_CC_BASE)
#define phys_to_uncached(p) ((uint64_t)(p) | DMW_UC_BASE)

#define read8(addr)     ({ uint8_t  __v = (*(volatile uint8_t  *)(uintptr_t)phys_to_uncached(addr)); rmb(); __v; })
#define write8(addr, v)   do { wmb(); (*(volatile uint8_t  *)(uintptr_t)phys_to_uncached(addr)) = (v); } while (0)
#define read16(addr)    ({ uint16_t __v = (*(volatile uint16_t *)(uintptr_t)phys_to_uncached(addr)); rmb(); __v; })
#define write16(addr, v)  do { wmb(); (*(volatile uint16_t *)(uintptr_t)phys_to_uncached(addr)) = (v); } while (0)
#define read32(addr)    ({ uint32_t __v = (*(volatile uint32_t *)(uintptr_t)phys_to_uncached(addr)); rmb(); __v; })
#define write32(addr, v)  do { wmb(); (*(volatile uint32_t *)(uintptr_t)phys_to_uncached(addr)) = (v); } while (0)

void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
void printf(const char *fmt, ...);
