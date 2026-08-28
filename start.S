.section .text.start
.global start
start:
     la.abs $sp, __stack_top
     #move $sp, __stack_top
     bl     main
     bl     exit
