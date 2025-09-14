.section .text
.global _entry
_entry: 
        # set sp for CPU
        la sp, stack0
        li a0, 1024*4
        add sp, sp, a0
         
        # jump to start
        call start
  
loop:
    j loop


