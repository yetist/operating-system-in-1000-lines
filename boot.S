/* LoongArch boot, exception entry and context switching code. */

.equ LOONGARCH_CSR_CRMD,     0x0
.equ LOONGARCH_CSR_PRMD,     0x1
.equ LOONGARCH_CSR_EUEN,     0x2
.equ LOONGARCH_CSR_ECFG,     0x4
.equ LOONGARCH_CSR_ERA,      0x6
.equ LOONGARCH_CSR_EENTRY,   0xc
.equ LOONGARCH_CSR_SAVE0,    0x30
.equ LOONGARCH_CSR_SAVE1,    0x31
.equ LOONGARCH_CSR_PGDL,     0x19
.equ LOONGARCH_CSR_PGDH,     0x1a
.equ LOONGARCH_CSR_DMW0,     0x180
.equ LOONGARCH_CSR_DMW1,     0x181

# Size of one exception vector (2^7 * 4 = 512 bytes).

.section .text.boot
.global boot
.align 2
boot:
  /* DMW0: uncached window for MMIO (0x8000_0000_0000_0000). */
  li.d    $t0, (0x8000 << 48 | 0x1)     # UC, PLV0
  csrwr   $t0, LOONGARCH_CSR_DMW0
  /* DMW1: cached window for RAM (0x9000_0000_0000_0000). */
  li.d    $t0, (0x9000 << 48 | 0x11)    # CA, PLV0
  csrwr   $t0, LOONGARCH_CSR_DMW1

  # jump_to_virt_addr
  li.d    $t0, (0x9000 << 48)
  pcaddi  $t1, 0
  or      $t0, $t0, $t1
  jirl    $zero, $t0, 0xc

  /* Enable paging (the kernel itself runs through DMW1). */
  li.w    $t0, 0b10110000   # PLV=0, IE=0, PG=1, DATF=1, DATM=1
  csrwr   $t0, LOONGARCH_CSR_CRMD
  li.w    $t0, 0b0100       # PPLV=0, PIE=1, PWE=0
  csrwr   $t0, LOONGARCH_CSR_PRMD
  li.w    $t0, 0x0          # FPE/SXE/ASXE/BTE disabled
  csrwr   $t0, LOONGARCH_CSR_EUEN

  # SAVE1 holds the kernel stack pointer used by the trap handler.
  la      $t0, __stack_top
  csrwr   $t0, LOONGARCH_CSR_SAVE1

  la      $sp, __stack_top
  bl      kernel_main
  break 1

/*
 * Exception vector table.  Hardware jumps to
 *   EENTRY + ecode * (4 << ECFG.VS)
 * so reserve one 512-byte slot per exception code and branch to the
 * common entry point from every slot.
 */
.section .text.vectors, "ax"
.balign 4096
.global exception_vectors
exception_vectors:
  .rept 64
  b       exception_entry
  .space  512 - 4, 0
  .endr

.text
.global exception_entry
exception_entry:
.global kernel_entry
kernel_entry:
  csrwr   $sp, LOONGARCH_CSR_SAVE0    # SAVE0 = interrupted sp; sp = old SAVE0
  csrrd   $sp, LOONGARCH_CSR_SAVE1    # sp = kernel stack top

  addi.d  $sp, $sp, -8 * 32

  st.d    $zero, $sp, 0 * 8
  st.d    $ra,   $sp, 1 * 8
  st.d    $tp,   $sp, 2 * 8
  st.d    $a0,   $sp, 4 * 8
  st.d    $a1,   $sp, 5 * 8
  st.d    $a2,   $sp, 6 * 8
  st.d    $a3,   $sp, 7 * 8
  st.d    $a4,   $sp, 8 * 8
  st.d    $a5,   $sp, 9 * 8
  st.d    $a6,   $sp, 10 * 8
  st.d    $a7,   $sp, 11 * 8
  st.d    $t0,   $sp, 12 * 8
  st.d    $t1,   $sp, 13 * 8
  st.d    $t2,   $sp, 14 * 8
  st.d    $t3,   $sp, 15 * 8
  st.d    $t4,   $sp, 16 * 8
  st.d    $t5,   $sp, 17 * 8
  st.d    $t6,   $sp, 18 * 8
  st.d    $t7,   $sp, 19 * 8
  st.d    $t8,   $sp, 20 * 8
  st.d    $r21,  $sp, 21 * 8
  st.d    $fp,   $sp, 22 * 8
  st.d    $s0,   $sp, 23 * 8
  st.d    $s1,   $sp, 24 * 8
  st.d    $s2,   $sp, 25 * 8
  st.d    $s3,   $sp, 26 * 8
  st.d    $s4,   $sp, 27 * 8
  st.d    $s5,   $sp, 28 * 8
  st.d    $s6,   $sp, 29 * 8
  st.d    $s7,   $sp, 30 * 8
  st.d    $s8,   $sp, 31 * 8

  # Save the interrupted sp (SAVE0 was clobbered by csrwr above).
  csrrd   $t0,   LOONGARCH_CSR_SAVE0
  st.d    $t0,   $sp, 3 * 8

  # Remember the previous kernel stack top in the unused "zero" slot
  # and point SAVE1 at this frame.  A nested trap then gets its own
  # frame instead of overwriting ours.
  csrrd   $t0,   LOONGARCH_CSR_SAVE1
  st.d    $t0,   $sp, 0 * 8
  move    $t0,   $sp
  csrwr   $t0,   LOONGARCH_CSR_SAVE1

  move    $a0,   $sp
  bl      handle_trap

  # Restore the kernel stack top used by the next trap (yield() may
  # have replaced SAVE1 while the handler was running).
  ld.d    $t0,   $sp, 0 * 8
  csrwr   $t0,   LOONGARCH_CSR_SAVE1

  ld.d    $ra, $sp, 1 * 8
  ld.d    $tp, $sp, 2 * 8
  ld.d    $a0, $sp, 4 * 8
  ld.d    $a1, $sp, 5 * 8
  ld.d    $a2, $sp, 6 * 8
  ld.d    $a3, $sp, 7 * 8
  ld.d    $a4, $sp, 8 * 8
  ld.d    $a5, $sp, 9 * 8
  ld.d    $a6, $sp, 10 * 8
  ld.d    $a7, $sp, 11 * 8
  ld.d    $t0, $sp, 12 * 8
  ld.d    $t1, $sp, 13 * 8
  ld.d    $t2, $sp, 14 * 8
  ld.d    $t3, $sp, 15 * 8
  ld.d    $t4, $sp, 16 * 8
  ld.d    $t5, $sp, 17 * 8
  ld.d    $t6, $sp, 18 * 8
  ld.d    $t7, $sp, 19 * 8
  ld.d    $t8, $sp, 20 * 8
  ld.d    $r21, $sp, 21 * 8
  ld.d    $fp, $sp, 22 * 8
  ld.d    $s0, $sp, 23 * 8
  ld.d    $s1, $sp, 24 * 8
  ld.d    $s2, $sp, 25 * 8
  ld.d    $s3, $sp, 26 * 8
  ld.d    $s4, $sp, 27 * 8
  ld.d    $s5, $sp, 28 * 8
  ld.d    $s6, $sp, 29 * 8
  ld.d    $s7, $sp, 30 * 8
  ld.d    $s8, $sp, 31 * 8
  ld.d    $sp, $sp, 3 * 8

  ertn

/*
 * TLB refill exception entry (CSR.TLBRENTRY).  The CPU enters with
 * CRMD.DA=1/CRMD.PG=0, so all addresses here are physical.
 *
 * The page table is laid out as a 3-level tree:
 *   PGD index: VA[38:30], PMD index: VA[29:21], PTE index: VA[20:12]
 * as configured by kernel_main() through CSR.PWCL/CSR.PWCH.
 */
.section .text.tlb_refill, "ax"
.balign 4096
.global tlb_refill
tlb_refill:
  csrwr   $t0, 0x8b                   # CSR.TLBRSAVE
  csrrd   $t0, 0x1b                   # CSR.PGD (PGDL/PGDH)
  lddir   $t0, $t0, 3                 # PGD -> PMD
  lddir   $t0, $t0, 1                 # PMD -> PTE page
  ldpte   $t0, 0
  ldpte   $t0, 1
  tlbfill
  csrrd   $t0, 0x8b
  ertn

.text

/*
 * The first user context starts here.  s0 was initialised by
 * create_process() with the user stack top.
 */
.global user_entry
user_entry:
  move    $sp, $s0
  li.d    $t0, 0x1000000              # USER_BASE
  csrwr   $t0, LOONGARCH_CSR_ERA
  li.d    $t0, 0b0111                 # PPLV=3, PIE=1
  csrwr   $t0, LOONGARCH_CSR_PRMD
  ertn

/*
 * void switch_context(uint64_t *prev_sp, uint64_t *next_sp);
 *
 * Save callee-saved registers on the current kernel stack, switch the
 * stack pointer and restore the next process' registers.
 * Layout: ra, tp, fp, s0..s8  (12 * 8 bytes).
 */
.global switch_context
switch_context:
  addi.d  $sp, $sp, -12 * 8
  st.d    $ra, $sp, 0 * 8
  st.d    $tp, $sp, 1 * 8
  st.d    $fp, $sp, 2 * 8
  st.d    $s0, $sp, 3 * 8
  st.d    $s1, $sp, 4 * 8
  st.d    $s2, $sp, 5 * 8
  st.d    $s3, $sp, 6 * 8
  st.d    $s4, $sp, 7 * 8
  st.d    $s5, $sp, 8 * 8
  st.d    $s6, $sp, 9 * 8
  st.d    $s7, $sp, 10 * 8
  st.d    $s8, $sp, 11 * 8

  st.d    $sp, $a0, 0
  ld.d    $sp, $a1, 0

  ld.d    $ra, $sp, 0 * 8
  ld.d    $tp, $sp, 1 * 8
  ld.d    $fp, $sp, 2 * 8
  ld.d    $s0, $sp, 3 * 8
  ld.d    $s1, $sp, 4 * 8
  ld.d    $s2, $sp, 5 * 8
  ld.d    $s3, $sp, 6 * 8
  ld.d    $s4, $sp, 7 * 8
  ld.d    $s5, $sp, 8 * 8
  ld.d    $s6, $sp, 9 * 8
  ld.d    $s7, $sp, 10 * 8
  ld.d    $s8, $sp, 11 * 8

  addi.d  $sp, $sp, 12 * 8
  ret
