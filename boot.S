.equ LOONGARCH_CSR_SAVE0, 0x30

.section .text.boot
.global boot
boot:
  li.d    $t0, 0x8000000000000001 # UC, PLV0, 0x8000 xxxx xxxx xxxx
  csrwr   $t0, 0x180
  li.d    $t0, 0x9000000000000011 # CA, PLV0, 0x9000 xxxx xxxx xxxx
  csrwr   $t0, 0x181

  la      $ra, .jump_to_64bit_addr
  ret

.jump_to_64bit_addr:
  la      $sp, __stack_top
  bl      kernel_main

.text
.global kernel_entry
kernel_entry:
  csrwr   $sp, LOONGARCH_CSR_SAVE0
  csrrd   $sp, LOONGARCH_CSR_SAVE0    # Read the old stack pointer.
  addi.d  $sp,   $sp, -8 * 32

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
  csrrd   $a0,   LOONGARCH_CSR_SAVE0    # Read the old stack pointer.
  st.d    $a0,   $sp, 3 * 8             # Save old sp

  addi.d  $a0,   $sp, 8 * 32            # a0 = sp
  csrwr   $a0,   LOONGARCH_CSR_SAVE0
  move    $a0,   $sp

  bl      handle_trap

  addi.d  $sp, $sp, -8 * 32

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
  ld.d    $r21,$sp, 21 * 8
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

  ertn // Return from exception.

.global user_entry
user_entry:
  idle 0
# la    $t0, USER_BASE
# csrwr $t0, 0x6
  #csrw sepc, USER_BASE
  #csrw sstatus, SSTATUS_SPIE | SSTATUS_SUM
  #sret

#void switch_context(uint64_t *prev_sp, uint64_t *next_sp)
.global switch_context
switch_context:
  # 将被调用者保存寄存器保存到当前进程的栈上
  addi.d  $sp, $sp, -10 * 8 # 为10个8字节寄存器分配栈空间
  st.d    $ra, $sp, 0 * 8   # 仅保存被调用者保存的寄存器
  st.d    $s0, $sp, 1 * 8
  st.d    $s1, $sp, 2 * 8
  st.d    $s2, $sp, 3 * 8
  st.d    $s3, $sp, 4 * 8
  st.d    $s4, $sp, 5 * 8
  st.d    $s5, $sp, 6 * 8
  st.d    $s6, $sp, 7 * 8
  st.d    $s7, $sp, 8 * 8
  st.d    $s8, $sp, 9 * 8

  # 切换栈指针
  st.d   $sp, $a0, 0        # *prev_sp = sp;
  ld.d   $sp, $a1, 0        # 在这里切换栈指针sp

  # 从下一个进程的栈中恢复被调用者保存的寄存器
  ld.d   $ra, $sp, 0 * 8    # 仅恢复被调用者保存的寄存器
  ld.d   $s0, $sp, 1 * 8
  ld.d   $s1, $sp, 2 * 8
  ld.d   $s2, $sp, 3 * 8
  ld.d   $s3, $sp, 4 * 8
  ld.d   $s4, $sp, 5 * 8
  ld.d   $s5, $sp, 6 * 8
  ld.d   $s6, $sp, 7 * 8
  ld.d   $s7, $sp, 8 * 8
  ld.d   $s8, $sp, 9 * 8

  addi.d $sp, $sp, 10 * 8   # 我们已从栈中弹出10个8字节寄存器
  ret
