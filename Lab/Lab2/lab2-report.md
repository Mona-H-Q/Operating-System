## 操作系统实验报告

学号: 191220163	姓名: 张木子苗	email: 1092252649@qq.com

实验进度：我完成了所有内容

#### **1.**  **实验名称**

Lab 2: 系统调用

#### **2.**  **实验目的**

通过自己实现printf、getchar、getstr函数和它们所用到的系统调用，了解操作系统代码被加载到内存中之后的一系列初始化流程，和其对外中断和系统调用的处理机制

#### **3.**  **实验内容**

在本部分中我将简述自己的工作流程，并附上少量代码。

**具体的工作思路是：从start.s开始，根据代码的执行流程逐步完善，直到能够成功进入用户程序的main函数中；**

**之后再完善库函数、系统调用和中断处理例程**

##### 1. 完善start.s

框架代码中，代码从 start.s 开始，先做好一系列初始化，转入保护模式，之后做好保护模式下一系列寄存器的初始化、设置好内核栈之后，jmp 到 bootloader 的 bootMain 函数，加载内核。

我们先完善 start.s 的代码，此步较为简单，将内核栈的esp设置好即可，根据我们的内存分布可知，内核栈可以设置在 0x1fffff 的位置

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210410124943610.png" alt="image-20210410124943610" style="zoom: 50%;" />

对应代码即为：

```python
movl $0x1fffff, %eax # setting esp of kernel stack
movl %eax, %esp
```



##### 2. 完善bootloader，加载内核

我们利用 bootloader 将内核代码加载到 0x100000 处，具体的实现方法和Lab1类似，**参见 bootloader/boot.c 中具体代码**，此处不粘贴代码

需要注意的是，将内核代码从磁盘复制到了 0x100000 处之后，还要读取其 ELF 头，得到 entry、phoff 和 .text 节的 offset，将代码从偏移 0x100000 的位置往前移到 0x100000 处，之后根据entry，利用函数指针的方法跳转到 kernel 的 main 函数 —— kEntry



##### 3. 完善kernel的main函数

在bootloader中，我们加载了内核代码，并且跳转到了 kernel 的 main 函数 kEntry

kernel 的 main 函数中做了一系列的初始化，以保证中断的发生。观察框架代码可以发现，这些初始化需要用到的函数都是已经定义好的，只需要完善好之后，在kEntry中调用即可

```c
void kEntry(void) {

	// Interruption is disabled in bootloader
	initSerial();// initialize serial port
	initIdt();     // initialize idt
	initIntr();    // initialize 8259a
	initSeg();     // initialize gdt, tss
	initVga();     // initialize vga device
	initKeyTable();// initialize keyboard device
	loadUMain();   // load user program, enter user space

	while(1);
	assert(0);
}
```

以上部分初始化函数需要进一步实现，**具体的实现可参见代码 kernel/kernel/idt.c/initIdt() 和 kernel/kernel/kvm.c/loadUMain() **，此处不复制粘贴

值得一提的是，因为 setIntr() 和 setTrap()  里的错误，导致我之后的代码跑飞了，这个问题将在第4部分 “实验中遇到的问题” 中阐述。



##### 4. 完善系统调用

通过 kEntry 中的 loadUmain 函数，我们成功运行到了用户的main函数中，可以开始执行测试用例了！

可以看到，app 的 main中，主要执行了 printf、getChar、getStr三个函数，在这三个函数里面用到了也是我们需要实现的系统调用

- printf的实现

  在框架代码中，已经做到了 printf 的一些基本实现。观察 printf 的代码，其主要做了两件事：用户态下进行格式串的转换；内核态下，通过系统调用将字符串输出到标准输出设备。

  关于格式控制符的转换，我们支持了%c、%s、%d、%x、%%五种格式控制符。具体实现思路就是遍历format字符串，碰到格式控制符时，就利用栈读取参数，再用已经提供好了的以下函数进行转换

  ```c
  int dec2Str(int decimal, char *buffer, int size, int count);
  int hex2Str(uint32_t hexadecimal, char *buffer, int size, int count);
  int str2Str(char *string, char *buffer, int size, int count);
  ```

  在系统调用中，我们最后会走到 `void syscallPrint(struct TrapFrame *tf)`

  利用以下语句设置段寄存器

  ```c
  asm volatile("movw %0, %%es"::"m"(sel));
  ```

  利用以下语句读取需要打印的字符串的字符

  ```c
  asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
  ```

  最后利用教程中提供的语句打印到显示屏上，注意 \n 的处理和光标的维护即可

  ```c
  data = character | (0x0c << 8);
  pos = (80*displayRow + displayCol) * 2;
  asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
  ```

  **以上完整实现请查看lib/sysycall.c/printf() 和 kernel/kernel/irqHandle.c/syscallPrint()中代码。**

  

- getChar的实现

  getChar函数的主体很简单，直接进行系统调用即可，注意参数要写对

  ```c
  char getChar(){ // 对应SYS_READ STD_IN
  	return syscall(SYS_READ, STD_IN, 0, 0, 0, 0);
  }
  ```

  最后 getChar 会走到 syscallGetChar

  这时我们需要先实现一个中断处理，即KeyboardHandle，此处仅简述实现思路：

  （1）用KeyBuffer作为缓冲区，暂存输入的字符

  ```c
  extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
  extern int bufferHead;
  extern int bufferTail;
  ```

  （2）每次接收到字符输入时，根据其类型做好缓冲区、显示屏和光标的维护

  ​	退格：显示屏上用空格覆盖一个字符，缓冲区尾部用 '\0' 覆盖，光标前移

  ​	回车：将 \n 传入缓冲区，diaplayCol = 0，displayRow++，如果到了底端还要调用scrollScreen();

  ​	正常字符：存入缓冲区，做好光标维护即可

  ​	**具体实现可查看 kernel/kernel/irqHandle.c/KeyboardHandle() 代码**

  <div STYLE="page-break-after: always;"></div>

  syscallGetChar 的代码如下，具体思路参见注释：

  ```c
  void syscallGetChar(struct TrapFrame *tf){
  	bufferHead = 0;
  	bufferTail = 0; //初始化缓冲区
  	while(1)
  	{
  		enableInterrupt();
  		if(bufferTail >= 1 && keyBuffer[bufferTail - 1] == '\n') break;
  	}                  //开中断，接收键盘输入，检测到输入回车时，跳出循环
  	disableInterrupt();//关中断
  	tf->eax = keyBuffer[bufferHead];    //读取缓冲区里的第1个字符作为返回值
  	for(int i = 0; i < bufferTail - 1; i++)
  		keyBuffer[i] = keyBuffer[i + 1];//缓冲区字符前移
  	keyBuffer[bufferTail--] = 0;        //用结束符覆盖尾端
  }
  ```

  

- getStr的实现

  getStr函数的主体很简单，直接进行系统调用即可，注意参数要写对

  ```c
  void getStr(char *str, int size){ // 对应SYS_READ STD_STR
  	syscall(SYS_READ, STD_STR, (uint32_t)str, (uint32_t)size, 0, 0);
  	return;
  }
  ```

  最后getStr会走到syscallGetStr

  接收输入的方法和 syscallGetChar类似，最后我们用如下语句将 KeyBuffer 中的元素复制到 str 中

  ```c
  int sel =  USEL(SEG_UDATA);
  asm volatile("movw %0, %%es"::"m"(sel));
  size = size > (bufferTail - 1) ? (bufferTail - 1) : size;
  for(int i = 0; i < size; i++)
  {
  	character = keyBuffer[i];
  	if(character > 0)
  		asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+i));
  }
  asm volatile("movb $0x00, %%es:(%0)"::"r"(str+size));
  ```

  **具体查看kernel/kernel/irqHandle.c/syscallGetStr()代码**
  
  <div STYLE="page-break-after: always;"></div>

#### 4. 实验结果

运行结果如下：

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210410142544011.png" alt="image-20210410142544011" style="zoom:80%;" />



#### 5. 实验中遇到的问题

##### 1. boot failed

在Ubuntu20.04上运行，QEMU会报boot failed的错误

换到Ubuntu18.04上之后，问题解决了

##### 2. codes jump off into nowhere

初次成功编译之后，代码报了如下错，显然是因为某个原因，代码跑偏了

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210410130507065.png" alt="image-20210410130507065" style="zoom: 67%;" />

debug了很久之后发现，错在 setIntr() 中

传入的参数selector是索引，应该左移3位才是段选择符，正确实现如下：

```c
ptr->segment = selector << 3;
```

而我错误地以为selector就是选择符，写成了：

```c
ptr->segment = selector & 0xFFFF;
```



##### 3. getStr的错误实现

在实现当前版本的getStr之前，还有一个错误实现的版本

通过寄存器读取了getStr的参数，然后while接受键盘输入直到碰到回车；之后就是简单的判断size+复制。

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210410123841582.png" alt="image-20210410123841582" style="zoom:80%;" />

错误的原因是：string定义在用户数据段，KeyBuffer 定义在内核数据段，因为寄存器基址的不同导致寻址错误。所以最后采用了实验内容中的实现方法。



##### 4. 曾经出现的奇怪的编译错误

如图，当我用如下循环复制KeyBuffer中的字符串时，可以编译通过

```c
for(int i = 0; i < size; i++)
	{
		character = keyBuffer[i];
		if(character > 0)
			asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+i));
	}
```

但是一但我去掉if语句，如下：

```c
for(int i = 0; i < size; i++)
	{
		character = keyBuffer[i];
		//if(character > 0)
			asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+i));
	}
```

就会报如下编译错误，其中第239行是当前函数的最后一行：

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210410124140614.png" alt="image-20210410124140614" style="zoom: 80%;" />

查询资料、询问老师之后也没有得到原因，但是因为加上if之后可以编译通过，就没有深究了

<div STYLE="page-break-after: always;"></div>

#### 6. 思考题解答

**1. ring3的堆栈在哪里?**

IA-32提供了4个特权级, 但TSS中只有3个堆栈位置信息, 分别用于ring0, ring1, ring2的堆栈切换。为什么TSS中没有ring3的堆栈信息？

答：ring3是用户态，ring3的堆栈信息即用户堆栈；在进行状态切换时，用户堆栈的信息被保存进了核心栈中。

**2. 保存寄存器的旧值**

我们在使用eax, ecx, edx, ebx, esi, edi前将寄存器的值保存到了栈中，如果去掉保存和恢复的步骤，从内核返回之后会不会产生不可恢复的错误？

答：视情况而定。如果在内核中没有修改这些寄存器的值，即使没有保存/恢复，也不会发生错误；但是一旦做了修改，就可能发生意想不到的结果。