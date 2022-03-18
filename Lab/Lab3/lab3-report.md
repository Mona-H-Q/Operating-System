## 操作系统实验报告

学号: 191220163	姓名: 张木子苗	email: 1092252649@qq.com

实验进度：我完成了除 **“选做：临界区”** 外的所有内容

#### **1.**  **实验名称**

Lab 3：进程切换

#### **2.**  **实验目的**

实现时钟中断处理 `timerHandle()` 和系统调用 `fork()`, `sleep()`, `exit()` ，了解操作系统中用于进程维护的数据结构PCB，以及操作系统在进程切换时所做的一系列操作。

#### **3.**  **实验内容**

##### 3.1 完成库函数

**修改的代码：lib/syscall.c**

此步实现较简单，直接在函数里调用syscall，正确传参即可

```c
pid_t fork() {
	return syscall(SYS_FORK, 0, 0, 0, 0, 0);
}
int sleep(uint32_t time) {
	return syscall(SYS_SLEEP, (uint32_t)time, 0, 0, 0, 0);
}
int exit() {
	return syscall(SYS_EXIT, 0, 0, 0, 0, 0);
}
```

##### 3.2 时钟中断处理

**修改的代码：kernel/kernel/irqhandle.c**

首先根据教程提示，修改irqhandle函数，加入对pcb的维护和timehandle的处理

```c
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;
	switch(sf->irq) {
		//...
		case 0x20:
			timerHandle(sf);
			break;
		//...
	}
	pcb[current].stackTop = tmpStackTop;
```

之后再实现timerHandle()，思路如下，具体见代码：

1. 遍历进程控制块，若某个进程的 state 为 STATE_BLOCKED，则将其 sleeptime 减 1。若减为 0，状态改为 RUNNABLE。 

2. 当前运行的进程 timecount 加一，若达到上限，说明当前进程已经消耗完一轮运行的时间片，需要进行进程切换。如果没有运行到时间上限，可以直接结束 timerHandle，再经过正常的中断返回流程，回到用户态。

3. 若需要切换进程，把 current 的状态改为 RUNNABLE。从current开始遍历进程控制块：

   ```c
   	int i = current + 1;
   	for (; i != current; i = (i + 1) % MAX_PCB_NUM)
           //...
   ```

   找到一个除current进程以外的，状态为 RUNNABLE 的进程，并切换到这个进程，timecount 计为 0。如果找不到其他进程，则继续运行current。

##### 3.3 系统调用例程

**修改的代码：kernel/kernel/irqhandle.c**

###### syscallFork 

syscallFork 需要寻找一个空闲进程控制块作为子进程的进程控制块，将父进程的资源完全复制给子进程，包括代码段和数据段。

如果遍历 PCB 池结束，没有找到空闲 PCB，则 fork 失败，进程返回 -1。成功则子进程返回 0，父进程返回子进程 的 pid。作为系统调用，其返回值也存放在 eax 中。

当成功被分配了子进程号之后，将进程的地址空间、用户态堆栈完全拷贝至进程的内存中。

根据教程中关于 gdt 的设定：GDT共有10项。第0项为空；第9项指向TSS段，内核代码段和数据段占了第1、2两项;剩下的6项可以作为3个用户进程的代码段和数据段。可知：新进程的cs寄存器高位应该为 pid \* 2 + 1, 其他段寄存器高位应该为 pid * 2 + 2。 

```c
	pcb[i].regs.cs = USEL((1 + i * 2));
	pcb[i].regs.ss = USEL((2 + i * 2));
	//...其它段寄存器的赋值
```

**注意：有一处不可以直接复制，需要通过计算父进程与子进程PCB之间的差值，得到StackTop，代码如下：**

```c
pcb[i].stackTop = pcb[current].stackTop + (uint32_t)&pcb[i] - (uint32_t)&pcb[current];
pcb[i].prevStackTop = pcb[current].prevStackTop + \
					  (uint32_t)&pcb[i] -(uint32_t)&pcb[current];
```

###### syscallSleep

```
pcb[current].state = STATE_BLOCKED;
pcb[current].sleepTime += sf->ecx;
```

之后同TimerHandle中做进程切换即可。

###### syscallExit

```
pcb[current].state = STATE_DEAD;
```

之后同TimerHandle中做进程切换即可。

**此处注意：与timerHandle中不同的是，在找不到其它Runnable的进程时，timeHandle调度当前进程继续执行，而syscallSleep和syscallExit调度第0号进程（idle进程）执行。**

##### 3.4 选做：中断嵌套

在irqhandle中，我们所加入的那4条语句，已经可以保证中断嵌套的实现。

手动模拟时钟中断：

```c
	enableInterrupt();
	for (j = 0; j < 0x100000; j++) 
	{
		*(uint8_t *)(j + (i + 1) * 0x100000) = *(uint8_t *)(j + (current +1) * 0x100000);
		asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
	}
	disableInterrupt()
```

测试运行结果如下：

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210506002118402.png" alt="image-20210506002118402" style="zoom:67%;" />

此处没有做其它的测试，但是仅运行教程上的以上测试是可以成功完成嵌套的。

#### 4. 实验结果

3.4中已经展示了中断嵌套的测试结果，此处展示基本功能的测试运行。结果如下：

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210505230401189.png" alt="image-20210505230401189" style="zoom: 67%;" />

#### 5. 实验相关问题的思考

1. 在irqHandle中，为什么需要加上以下语句？

   ```c
   	uint32_t tmpStackTop = pcb[current].stackTop;
   	pcb[current].prevStackTop = pcb[current].stackTop;
   	pcb[current].stackTop = (uint32_t)sf;
   	//中断处理......
   	pcb[current].stackTop = tmpStackTop;
   ```

   可以将prevStackTop看作内核栈的%ebp，stackTop看作内核栈的%esp。

   每次进入中断，先暂存之前的栈顶指针，将其值设为prevStackTop，再将内核栈顶指针stackTop更新为sf的值。实际上就是在内核栈中为新中断的发生开辟了空间保存现场。

   等到中断处理结束后将stackTop的值恢复，释放空间。在中断嵌套时，根据栈的性质，也可以保证不出错。

   

2. 教程中提供的如下代码是如何实现进程切换的？

   ```c
   	uint32_t tmpStackTop = pcb[current].stackTop;
   	pcb[current].stackTop = pcb[current].prevStackTop;
   	tss.esp0 = (uint32_t)&(pcb[current].stackTop);
   	asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
   	asm volatile("popl %gs");
   	asm volatile("popl %fs");
   	asm volatile("popl %es");
   	asm volatile("popl %ds");
   	asm volatile("popal");
   	asm volatile("addl $8, %esp");
   	asm volatile("iret");
   ```

   `uint32_t tmpStackTop = pcb[current].stackTop;` 暂存进程当前内核栈顶指针

   `pcb[current].stackTop = pcb[current].prevStackTop;` 更新内核栈指针，释放进程此次中断占用的内核栈空间

   `tss.esp0 = (uint32_t)&(pcb[current].stackTop);` 更新tss中存放的内核栈指针

   之后切换到当前进程的内核栈，恢复寄存器，并且用 iret 指令回到用户态

<div STYLE="page-break-after: always;"></div>
