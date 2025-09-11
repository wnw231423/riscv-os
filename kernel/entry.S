.section .text
.global _entry
_entry: 
        li t0, 0x10000000    # UART 基地址 
        li t1, 'S'    # 启动标记 
        sb t1, 0(t0)    # 输出字符 S 表示启动

        # set sp for CPU
        la sp, stack0
        li a0, 1024*4
        add sp, sp, a0

        li t1, 'p'    # 栈设置完成标记 
        sb t1, 0(t0)
         
        # jump to start
        call start
 
       
loop:
    j loop


