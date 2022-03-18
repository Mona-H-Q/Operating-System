## 操作系统实验报告

学号: 191220163	姓名: 张木子苗	email: 1092252649@qq.com

实验进度：我完成了所有内容

#### 1. 准备知识

**你弄清楚本小结标题中各种名词的含义和他们间的关系了吗？请在实验报告中阐述。**

CPU：中央处理器，作为计算机系统的运算和控制核心

内存：它用于暂时存放CPU中的运算数据、与硬盘等外部存储器交换的数据

BIOS：基本输入输出系统

磁盘：计算机中存放信息的主要的存储设备

主引导扇区：位于整个硬盘的0磁头0柱面1扇区，包含加载程序的代码

加载程序：用于将操作系统的代码和数据从磁盘加载到内存中，并且跳转到操作系统的起始地址

操作系统：是管理计算机硬件与软件资源的计算机程序

**关系：计算机启动后，由BIOS加载磁盘上主引导扇区代码，然后跳转到 CS:IP = 0x0000:0x7c00 处执行执行加载程序，加载程序将操作系统的代码和数据从磁盘加载到内存中，并且跳转到操作系统的起始地址**

#### 2. 实验内容

start.s中代码分为三部分，对应我们实验的3个阶段

```
/* Real Mode Hello World */
/* Protected Mode Hello World */
/* Protected Mode Loading Hello World APP */
```

我们在每个阶段中将其余两个部分的代码注释掉，只运行其中一个部分的代码

##### 2.1 在实模式下实现一个Hello World程序 

在实模式下在终端中打印 Hello, World! 

复制indexGuide中的代码，在进入保护模式前传递参数并调用displayStr，然后用死循环loop暂停代码。

可见，QEMU中成功打印了字符串 `Hello, World![In Real Mode]`

![image-20210321190534396](C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210321190534396.png)



##### 2.2 在保护模式下实现一个Hello World程序 

从实模式切换至保护模式，并在保护模式下在终端中打印 Hello, World! 

此时我们需要进入保护模式，显然需要设置 gdt，并且完成以下操作：关闭中断 / 启动A20总线 / 加载GDTR / 设置CR0的PE位为1 / 长跳转切换至保护模式 / 初始化DS ES FS GS SS / 初始化栈顶指针ESP

之后再传递参数，调用从 app.s 中复制来的 diaplayStr 和 nextChar，然后用死循环 loop32 暂停代码

可见，QEMU中成功打印了字符串 `Hello, World![In Protected Mode]`

![image-20210321193846043](C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210321193846043.png)

##### 2.3 在保护模式下加载磁盘中的Hello World程序运行

在第三个阶段中，进入保护模式的前的准备代码和之前一样，只不过在设定好了栈指针%esp之后，我们不再传参和调用 displayStr 函数，而是直接跳转到 bootMain

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210322001040158.png" alt="image-20210322001040158" style="zoom: 67%;" />

在 bootMain 中，我们从磁盘上读取 app.s 的代码到 0x8c00 处，然后直接 jmp 语句跳转

可见，QEMU 中成功打印了字符串 `Hello, World![From app.s]`

![image-20210321200648896](C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210321200648896.png)