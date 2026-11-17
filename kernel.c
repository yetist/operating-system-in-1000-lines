#include "kernel.h"
#include "common.h"

extern char __bss[], __bss_end[];
extern char __free_ram[], __free_ram_end[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

extern char exception_vectors[];
extern char tlb_refill[];
extern void user_entry(void);
extern void switch_context(uint64_t *prev_sp, uint64_t *next_sp);

struct process procs[PROCS_MAX];
struct process *current_proc;
struct process *idle_proc;

#define UART_BASE       0x1fe001e0UL
#define UART_REG_LSR    0x5       // 线路状态寄存器
#define LSR_DR          (1<<0)    // 接收数据有效位, 1为有数据
#define LSR_TFE         (1<<5)    // 传输FIFO位空表示位, 1为空

/*
 * Physical address of the I/O BAR assigned to our virtio-blk device.
 *
 * The loongarch "virt" machine exposes PCI I/O space at 0x18004000
 * (the window starts at PCI I/O address 0x4000), so BAR0 is programmed
 * to 0x4100 and the CPU accesses the legacy virtio registers through
 * the uncached direct-map window.
 */
#define VIRTIO_BLK_IO_ADDR  0x18004100UL
#define VIRTIO_BLK_IO_BAR   0x4101UL

/*
 * Allocate zeroed pages from the free RAM region.  The returned address
 * is a kernel virtual address in the cached DMW1 window; use
 * virt_to_phys() when it has to be handed to hardware (page tables,
 * virtio descriptors, ...).
 */
paddr_t alloc_pages(uint64_t n)
{
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

void map_page(uint64_t *pgd, uint64_t vaddr, paddr_t paddr, uint64_t flags)
{
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);

    uint32_t pgd_i = (vaddr >> PT_PGD_SHIFT) & 0x1ff;
    if (pgd[pgd_i] == 0) {
        paddr_t pmd_paddr = virt_to_phys(alloc_pages(1));
        pgd[pgd_i] = pmd_paddr;
    }

    uint64_t *pmd = (uint64_t *) (uintptr_t) phys_to_virt(pgd[pgd_i]);
    uint32_t pmd_i = (vaddr >> PT_PMD_SHIFT) & 0x1ff;
    if (pmd[pmd_i] == 0) {
        paddr_t pte_paddr = virt_to_phys(alloc_pages(1));
        pmd[pmd_i] = pte_paddr;
    }

    uint64_t *pte = (uint64_t *) (uintptr_t) phys_to_virt(pmd[pmd_i]);
    uint32_t pte_i = (vaddr >> PT_PTE_SHIFT) & 0x1ff;
    pte[pte_i] = (virt_to_phys(paddr) & ~(PAGE_SIZE - 1)) | flags;
}

/*
 * Minimal PCI support: scan the ECAM configuration space on bus 0 and
 * assign an I/O BAR to the legacy virtio-blk device.
 */
static uint32_t pci_config_addr(int bus, int dev, int fn, int off)
{
    return PCI_ECAM_BASE + (bus << 20) + (dev << 15) + (fn << 12) + off;
}

static uint32_t pci_read32(int bus, int dev, int fn, int off)
{
    return read32(pci_config_addr(bus, dev, fn, off));
}

static void pci_write32(int bus, int dev, int fn, int off, uint32_t value)
{
    write32(pci_config_addr(bus, dev, fn, off), value);
}

static bool pci_find_virtio_blk(int *out_bus, int *out_dev)
{
    for (int dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read32(0, dev, 0, 0x00);
        uint16_t vendor = id & 0xffff;
        uint16_t device = id >> 16;

        if (vendor == 0x1af4 && device == 0x1001) {
            *out_bus = 0;
            *out_dev = dev;
            return true;
        }
    }
    return false;
}

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
uint64_t blk_capacity;

static bool virtq_is_busy(struct virtio_virtq *vq)
{
    return vq->last_used_index != *vq->used_index;
}

static void virtq_kick(struct virtio_virtq *vq, int desc_index)
{
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    write16(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

static struct virtio_virtq *virtq_init(unsigned index)
{
    size_t size = align_up(sizeof(struct virtio_virtq), PAGE_SIZE);
    paddr_t virtq_paddr = alloc_pages(size / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *) (uintptr_t) virtq_paddr;

    vq->queue_index = index;
    vq->used_index = &vq->used.index;
    vq->last_used_index = 0;

    write16(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_QUEUE_SEL, index);
    uint16_t queue_num = read16(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_QUEUE_NUM);
    if (queue_num != VIRTQ_ENTRY_NUM)
        PANIC("virtio: unexpected queue size %d", queue_num);

    write32(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_QUEUE_PFN,
            virt_to_phys(virtq_paddr) / PAGE_SIZE);
    return vq;
}

void virtio_blk_init(void)
{
    int bus, dev;
    if (!pci_find_virtio_blk(&bus, &dev))
        PANIC("virtio: blk device not found");

    /* Disable decoding, program BAR0, then enable I/O + bus mastering. */
    pci_write32(bus, dev, 0, 0x04, 0);
    pci_write32(bus, dev, 0, 0x10, 0xffffffff);
    uint32_t bar_mask = pci_read32(bus, dev, 0, 0x10);
    uint32_t bar_size = ~(bar_mask & ~0x3UL) + 1;
    if (bar_size > 0x1000)
        PANIC("virtio: unexpected BAR0 size 0x%x", bar_size);
    pci_write32(bus, dev, 0, 0x10, VIRTIO_BLK_IO_BAR);
    pci_write32(bus, dev, 0, 0x04, 0x7);

    /* Reset the device. */
    write8(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_STATUS, 0);
    while (read8(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_STATUS) != 0)
        ;

    write8(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK);
    write8(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_STATUS,
           VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    /* No optional features are needed. */
    write32(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_GUEST_FEATURES, 0);

    blk_request_vq = virtq_init(0);

    write8(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_STATUS,
           VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    uint32_t cap_lo = read32(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_CONFIG);
    uint32_t cap_hi = read32(VIRTIO_BLK_IO_ADDR + VIRTIO_PCI_CONFIG + 4);
    blk_capacity = (((uint64_t) cap_hi << 32) | cap_lo) * SECTOR_SIZE;
    printf("virtio-blk: capacity is %d bytes\n", (int) blk_capacity);

    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req *) (uintptr_t) blk_req_paddr;
}

void read_write_disk(void *buf, unsigned sector, int is_write)
{
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
              sector, (int) (blk_capacity / SECTOR_SIZE));
        return;
    }

    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;

    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    struct virtio_virtq *vq = blk_request_vq;
    vq->descs[0].addr = virt_to_phys(blk_req_paddr);
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = virt_to_phys(blk_req_paddr) +
                        offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = virt_to_phys(blk_req_paddr) +
                        offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    virtq_kick(vq, 0);
    while (virtq_is_busy(vq))
        ;

    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
               sector, blk_req->status);
        return;
    }

    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}

struct file files[FILES_MAX];
uint8_t disk[DISK_MAX_SIZE];

int oct2int(char *oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

void fs_flush(void) {
    memset(disk, 0, sizeof(disk));
    unsigned off = 0;
    for (int file_i = 0; file_i < FILES_MAX; file_i++) {
        struct file *file = &files[file_i];
        if (!file->in_use)
            continue;

        struct tar_header *header = (struct tar_header *) &disk[off];
        memset(header, 0, sizeof(*header));
        strcpy(header->name, file->name);
        strcpy(header->mode, "000644");
        strcpy(header->magic, "ustar");
        strcpy(header->version, "00");
        header->type = '0';

        int filesz = file->size;
        for (int i = sizeof(header->size); i > 0; i--) {
            header->size[i - 1] = (filesz % 8) + '0';
            filesz /= 8;
        }

        int checksum = ' ' * sizeof(header->checksum);
        for (unsigned i = 0; i < sizeof(struct tar_header); i++)
            checksum += (unsigned char) disk[off + i];

        for (int i = 5; i >= 0; i--) {
            header->checksum[i] = (checksum % 8) + '0';
            checksum /= 8;
        }

        memcpy(header->data, file->data, file->size);
        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    printf("wrote %d bytes to disk\n", (int) sizeof(disk));
}

void fs_init(void) {
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, false);

    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct tar_header *header = (struct tar_header *) &disk[off];
        if (header->name[0] == '\0')
            break;

        if (strcmp(header->magic, "ustar") != 0)
            PANIC("invalid tar header: magic=\"%s\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file *file = &files[i];
        file->in_use = true;
        strcpy(file->name, header->name);
        memcpy(file->data, header->data, filesz);
        file->size = filesz;
        printf("file: %s, size=%d\n", file->name, (int) file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

struct file *fs_lookup(const char *filename) {
    for (int i = 0; i < FILES_MAX; i++) {
        struct file *file = &files[i];
        if (!strcmp(file->name, filename))
            return file;
    }

    return NULL;
}

void putchar(char ch) {
    while (!(read8(UART_BASE + UART_REG_LSR) & LSR_TFE))
        ;
    write8(UART_BASE, ch);
}

int getchar(void) {
    while (!(read8(UART_BASE + UART_REG_LSR) & LSR_DR))
        ;
    return read8(UART_BASE);
}

struct process *create_process(const void *image, size_t image_size)
{
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");

    /*
     * Build the initial kernel context for switch_context():
     * ra, tp, fp, s0..s8.  s0 carries the initial user stack pointer.
     */
    uint64_t user_sp = USER_BASE + align_up(image_size, 16);
    uint64_t *sp = (uint64_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                 // s8
    *--sp = 0;                 // s7
    *--sp = 0;                 // s6
    *--sp = 0;                 // s5
    *--sp = 0;                 // s4
    *--sp = 0;                 // s3
    *--sp = 0;                 // s2
    *--sp = 0;                 // s1
    *--sp = user_sp;           // s0: user stack top
    *--sp = 0;                 // fp
    *--sp = 0;                 // tp
    *--sp = (uint64_t) user_entry;  // ra

    uint64_t *pgd = (uint64_t *) alloc_pages(1);

    // User pages. The kernel runs through the direct mapping windows,
    // so kernel pages do not need to be mapped here.
    for (uint64_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;
        memcpy((void *) page, image + off, copy_size);
        map_page(pgd, USER_BASE + off, virt_to_phys(page), PAGE_USER);
    }

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint64_t) sp;
    proc->page_table = pgd;
    return proc;
}

void yield(void) {
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    if (next == current_proc)
        return;

    struct process *prev = current_proc;
    current_proc = next;

    // Switch the page table and the kernel stack used by the trap handler.
    WRITE_CSR(LOONGARCH_CSR_PGDL, virt_to_phys(next->page_table));
    WRITE_CSR(LOONGARCH_CSR_PGDH, virt_to_phys(next->page_table));
    WRITE_CSR(LOONGARCH_CSR_SAVE1, (uint64_t) &next->stack[sizeof(next->stack)]);
    invtlb_all();

    switch_context(&prev->sp, &next->sp);
}

void handle_syscall(struct trap_frame *f) {
    switch (f->a3) {
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_GETCHAR:
            f->a0 = (unsigned char) getchar();
            break;
        case SYS_EXIT:
            printf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        case SYS_READFILE:
        case SYS_WRITEFILE: {
            const char *filename = (const char *) f->a0;
            char *buf = (char *) f->a1;
            size_t len = f->a2;
            struct file *file = fs_lookup(filename);
            if (!file) {
                printf("file not found: %s\n", filename);
                f->a0 = -1;
                break;
            }

            if (len > sizeof(file->data))
                len = file->size;

            if (f->a3 == SYS_WRITEFILE) {
                memcpy(file->data, buf, len);
                file->size = len;
                fs_flush();
            } else {
                memcpy(buf, file->data, len);
            }

            f->a0 = len;
            break;
        }
        default:
            PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f)
{
    uint64_t estat = READ_CSR(LOONGARCH_CSR_ESTAT);
    uint64_t era = READ_CSR(LOONGARCH_CSR_ERA);
    uint64_t badv = READ_CSR(LOONGARCH_CSR_BADV);
    uint64_t badi = READ_CSR(LOONGARCH_CSR_BADI);

    uint8_t ecode = (estat & ESTAT_ECOCD_MASK) >> 16;
    uint8_t esubcode = (estat & ESTAT_ESUBCOCD_MASK) >> 22;

    if (ecode == ESTAT_ECODE_SYS) {
        handle_syscall(f);
        era += 4;
        WRITE_CSR(LOONGARCH_CSR_ERA, era);
    } else {
        PANIC("unexpected trap, estat=%x, ecode=%x, esubcode=%x\n"
              "\tera=%x, badv=%x, badi=%x",
              (unsigned) estat, ecode, esubcode,
              (unsigned) era, (unsigned) badv, (unsigned) badi);
    }
}

void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
    printf("\n1000 Lines OS on LoongArch!\n\n");

    // Exception vector table (ECFG.VS = 7 -> 512-byte vectors).
    WRITE_CSR(LOONGARCH_CSR_EENTRY, (uint64_t) exception_vectors);
    WRITE_CSR(LOONGARCH_CSR_ECFG, 7 << 16);

    // Page table walker geometry for the TLB refill handler.
    WRITE_CSR(LOONGARCH_CSR_PWCL,
              PT_PTE_SHIFT | ((PT_PTE_SHIFT - 3) << 5) |
              (PT_PMD_SHIFT << 10) | ((PT_PTE_SHIFT - 3) << 15));
    WRITE_CSR(LOONGARCH_CSR_PWCH,
              PT_PGD_SHIFT | ((PT_PTE_SHIFT - 3) << 6));
    WRITE_CSR(LOONGARCH_CSR_TLBRENTRY, (uint64_t) tlb_refill);

    virtio_blk_init();
    fs_init();

    idle_proc = create_process(NULL, 0);
    idle_proc->pid = 0; // idle
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);
    yield();

    PANIC("switched to idle process");
}
