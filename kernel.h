#pragma once
#include "common.h"

#define PROCS_MAX 8
#define PROC_UNUSED   0
#define PROC_RUNNABLE 1
#define PROC_EXITED   2

/* LoongArch control/status registers. */
#define LOONGARCH_CSR_CRMD      0x0
#define LOONGARCH_CSR_PRMD      0x1
#define LOONGARCH_CSR_EUEN      0x2
#define LOONGARCH_CSR_ECFG      0x4
#define LOONGARCH_CSR_ESTAT     0x5
#define LOONGARCH_CSR_ERA       0x6
#define LOONGARCH_CSR_BADV      0x7
#define LOONGARCH_CSR_BADI      0x8
#define LOONGARCH_CSR_EENTRY    0xc
#define LOONGARCH_CSR_TLBIDX    0x10
#define LOONGARCH_CSR_TLBEHI    0x11
#define LOONGARCH_CSR_TLBELO0   0x12
#define LOONGARCH_CSR_TLBELO1   0x13
#define LOONGARCH_CSR_ASID      0x18
#define LOONGARCH_CSR_PGDL      0x19
#define LOONGARCH_CSR_PGDH      0x1a
#define LOONGARCH_CSR_PGD       0x1b
#define LOONGARCH_CSR_PWCL      0x1c
#define LOONGARCH_CSR_PWCH      0x1d
#define LOONGARCH_CSR_SAVE0     0x30
#define LOONGARCH_CSR_SAVE1     0x31
#define LOONGARCH_CSR_TLBRENTRY 0x88
#define LOONGARCH_CSR_TLBRERA   0x8a
#define LOONGARCH_CSR_TLBRSAVE  0x8b
#define LOONGARCH_CSR_TLBREHI   0x8e
#define LOONGARCH_CSR_TLBRPRMD  0x8f

#define ESTAT_ECOCD_MASK    0x3f0000
#define ESTAT_ESUBCOCD_MASK 0x7fc00000
#define ESTAT_ECODE_SYS     0xb

/*
 * Page table entries (LoongArch TLB / PTE format).
 *
 *   0     V        valid
 *   1     D        dirty (writable)
 *   3:2   PLV      privilege level
 *   5:4   MAT      memory access type (1 = coherent cached)
 *   6     G        global
 *   7     P        present (software walker)
 *   8     W        writable (software walker)
 *   47:12 PPN      physical page number
 *   61    NR       no read
 *   62    NX       no execute
 *   63    RPLV
 */
#define PAGE_V       (1UL << 0)
#define PAGE_D       (1UL << 1)
#define PAGE_PLV0    (0UL << 2)
#define PAGE_PLV3    (3UL << 2)
#define PAGE_MAT_CC  (1UL << 4)
#define PAGE_G       (1UL << 6)
#define PAGE_P       (1UL << 7)
#define PAGE_W       (1UL << 8)
#define PAGE_NX      (1UL << 62)

/* Internal pages of the kernel (accessed through DMW1). */
#define PAGE_KERNEL (PAGE_V | PAGE_D | PAGE_MAT_CC | PAGE_G | PAGE_P | PAGE_W)
/* User pages, accessible from PLV3. */
#define PAGE_USER   (PAGE_V | PAGE_D | PAGE_MAT_CC | PAGE_P | PAGE_W | PAGE_PLV3)

/* Page table geometry: 3 levels, 9 bits each, 4KiB pages. */
#define PT_PGD_SHIFT 30
#define PT_PMD_SHIFT 21
#define PT_PTE_SHIFT 12
#define PT_INDEX_BITS 9

#define USER_BASE 0x1000000

#define FILES_MAX   2
#define DISK_MAX_SIZE     align_up(sizeof(struct file) * FILES_MAX, SECTOR_SIZE)
#define SECTOR_SIZE       512

#define VIRTQ_ENTRY_NUM   256
#define VIRTIO_DEVICE_BLK 2
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

/* Legacy virtio-pci register offsets (BAR0, I/O space). */
#define VIRTIO_PCI_HOST_FEATURES   0x00
#define VIRTIO_PCI_GUEST_FEATURES  0x04
#define VIRTIO_PCI_QUEUE_PFN       0x08
#define VIRTIO_PCI_QUEUE_NUM       0x0c
#define VIRTIO_PCI_QUEUE_SEL       0x0e
#define VIRTIO_PCI_QUEUE_NOTIFY    0x10
#define VIRTIO_PCI_STATUS          0x12
#define VIRTIO_PCI_ISR             0x13
#define VIRTIO_PCI_CONFIG          0x14
#define VIRTIO_STATUS_ACK          1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4

#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2

/* PCI host bridge on the QEMU loongarch "virt" machine. */
#define PCI_ECAM_BASE   0x20000000UL
#define PCI_IO_BASE     0x18004000UL
#define PCI_IO_WINDOW   0x100UL

struct process {
    int pid; // 0 if it's an idle process
    int state; // PROC_UNUSED, PROC_RUNNABLE, PROC_EXITED
    vaddr_t sp; // kernel stack pointer
    uint64_t *page_table; // points to first level page table
    uint8_t stack[8192] __attribute__((aligned(16))); // kernel stack
};

struct trap_frame {
    uint64_t zero;
    uint64_t ra;
    uint64_t tp;
    uint64_t sp;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    uint64_t t7;
    uint64_t t8;
    uint64_t x;
    uint64_t fp;
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
} __attribute__((packed));

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
};

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

struct virtq_used {
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
};

struct virtio_virtq {
    struct virtq_desc descs[VIRTQ_ENTRY_NUM];
    struct virtq_avail avail;
    struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
};

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} __attribute__((packed));

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
    char data[];
} __attribute__((packed));

struct file {
    bool in_use;
    char name[100];
    char data[1024];
    size_t size;
};

#define TO_STR_HELPER(x) #x
#define TO_STR(x) TO_STR_HELPER(x)

#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrrd %0, " TO_STR(reg) : "=r"(__tmp));          \
        __tmp;                                                                 \
    })

#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint64_t __tmp = (value);                                              \
        __asm__ __volatile__("csrwr %0, " TO_STR(reg) ::"r"(__tmp));           \
    } while (0)

static inline void invtlb_all(void)
{
    __asm__ __volatile__("invtlb 0x0, $zero, $zero" ::: "memory");
}

#define PANIC(fmt, ...)                                                        \
    do {                                                                       \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
        while (1) {}                                                           \
    } while (0)
