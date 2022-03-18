## 操作系统实验报告

学号: 191220163	姓名: 张木子苗	email: 1092252649@qq.com

**实验进度：我完成了包括 “选做: 生产者-消费者问题和读者-写者问题” 在内的所有内容**

**写在前面：因为本次实验有多个需要测试的地方，所以对于测试用例，我采取了条件编译的方法。**

在 app/main.c 中定义：

1. #define Test 1：Test 'scanf' & Test 'Semaphore'
2. #define Test 2：Philosopher Problem Test
3. #define Test 3：Producer-Consumer Problem Test
4. #define Test 4：Reader-Writer Problem Test

#### **1.**  **实验名称**

Lab 4：进程同步

#### **2.**  **实验目的**

实现操作系统的信号量及对应的系统调用，基于信号量解决哲学家就餐问题、读者写者问题和生产者消费者问题，加深对信号量与PV操作和进程同步问题的理解。

#### **3.**  **实验内容**

##### 3.1 实现格式化输入函数

**修改的代码：kernel/kernel/irqhandle.c**

为了降低实验难度，`syscall.c `中的 `scanf` 已经完成，只需要完成对应的中断处理例程

需要完善的代码在 keyboardHandle() 和 syscallReadStdIn()

`keyboardHandle`要做的事情就两件：

1. 将读取到的`keyCode`放入到`keyBuffer`中
2. 唤醒阻塞在`dev[STD_IN]`上的一个进程

`syscallReadStdIn`要做的事情也就两件：

1. 如果`dev[STD_IN].value == 0`，将当前进程阻塞在`dev[STD_IN]`上
2. 进程被唤醒，读`keyBuffer`中的所有数据

重点理解一下框架代码的 scanf 函数。**框架代码 scanf 的实现方法是：一个一个字符读取输入，并且做格式比较**。每次读取时调用 syscall，参数为 `SYS_READ, STD_IN` ，最后走到 syscallReadStdIn 函数中处理。

当进程进入 syscallReadStdIn 时，如果此时尚未有进程阻塞在 STD_IN上，则该进程阻塞自身，触发时钟中断，呼唤操作系统进行调度，直到用户输入内容之后，在 keyboardHandle 中被唤醒；如果已经有进程阻塞在 STD_IN 上，则返回 -1，本次读取失败。

阻塞、唤醒进程的代码教程中都有提供，具体实现请查看源文件。

**此步的测试结果在系统调用例程完成后一起展示。**



##### 3.2 系统调用例程

**修改的代码：kernel/kernel/irqhandle.c**

在讲述实现思路之前，先分析框架代码对信号量的实现。

在内核空间中，信号量是一个 Semaphore 类型的结构体。用户手中所掌握的 sem_t 类型的信号量，实际上是真正的信号量在内核数据结构 `Semaphore sem[MAX_SEM_NUM]` 数组中的下标。是用户程序通过 sem_init 函数所申请并返回的，之后用户也需要靠信号量相关的系统调用对该信号量进行操作。

```c
typedef int32_t sem_t;
Semaphore sem[MAX_SEM_NUM];
```

###### 3.2.1 sem_init

sem_init 对应中断处理函数为：syscallSemInit

实现思路：在系统数据结构的 sem 数组中，找一个空闲 (state == 0) 的信号量。如果找到了则返回下标的值，否则返回 -1。在 sem_init 中，将用户的信号量设置为该下标（框架代码已经实现）。

```c
int sem_init(sem_t *sem, uint32_t value) {
	*sem = syscall(SYS_SEM, SEM_INIT, value, 0, 0, 0);
	//...
}
```

###### 3.2.2 sem_post

前面已经提及，用户手中类型为 sem_t 的信号量实际上只是一个下标，所以以下3个函数均需要先判断下标是否合法，可用以下代码：

```c
if (i < 0 || i >= MAX_SEM_NUM) 
{
	pcb[current].regs.eax = -1;
	return;
}
```

sem_post 对应中断处理函数为：syscallSemPost，实现思路如下：

1. sem[i].value--;
2. 如果sem[i].value > 0，表明没有进程被阻塞，不需要其他操作；
3. 否则sem[i].value <= 0，释放在该信号量上等待的一个进程，将其状态设置为 STATE_RUNNABLE。

###### 3.2.3 sem_wait

sem_wait 对应中断处理函数为：syscallSemWait，实现思路如下：

1. sem[i].value++;
2. 如果sem[i].value >= 0，表明资源足够，不需要其他操作；
3. 否则sem[i].value < 0，该进程阻塞自身，并且利用 `asm volatile("int $0x20");` 触发时钟中断，呼唤操作系统进行调度。

###### 3.2.4 sem_destroy

sem_destroy 对应中断处理函数为：syscallSemDestroy

直接将该下标对应的信号量的value和state均置0即可。

###### 3.2.5 Test

实验完成了一个小的阶段，接下来进行测试。

```c
#define Test 1
```

测试结果如下：

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210523012248310.png" alt="image-20210523012248310" style="zoom: 80%;" />

scanf的测试正确性显然。

对于信号量的测试，流程如下：

1. 父进程创立并初始化 mutex，mutex 初值为2；
2. fork() 结束后，父进程继续运行，进入循环，i-- 后 sleep；之后子进程被调度运行，执行两次 sem_wait，第3次时 mutex = -1，被阻塞，此时子进程 i = 1；
3. 之后父进程 sleep 结束，被调度执行；sem_post 后，再次 sleep，此时父进程 i = 2；
4. 子进程被唤醒，继续运行，再次执行 sem_wait 后被阻塞，此时子进程 i = 0，但被阻塞，未退出循环；
5. 父进程sleep结束，被调度执行；sem_post 后，再次sleep，此时父进程 i = 1；
6. 子进程被唤醒，此时子进程的 i = 0，跳出循环，Destroy 信号量并且 exit()；
7. 父进程sleep结束，被调度执行 sem_post 后，再次sleep，此时父进程 i = 0；
8. 因为子进程已经exit()，此时没有别的进程被唤醒，父进程sleep结束后，再次执行一次sem_post，跳出循环，Destroy信号量并且exit()；

由此可见，信号量 sem 实际上分别被父子进程destroy了两次。



##### 3.3. 解决进程同步问题

**修改的代码：app/main.c 和 kernel/kernel/irqhandle.c**

3个问题的实现在教程中均有伪码，将伪码中的P操作改成 sem_wait，V操作改成 sem_post 即可，在接下来的模块，我将主要展示我的测试结果。

对于如何创建多个进程，我没有采取 getpid 的方法；我采取的方法是：每次都由新创建的子进程来创建新进程，代码如下：

```c
int ret = fork();
if (ret == 0) //Child
{
	ret = fork();
	if (ret == 0)
	{
		ret = fork();
		if (ret == 0)
		{
			ret = fork();
			if (ret == 0)
				Philosopher(sem, 4);
			else if (ret != -1)
				Philosopher(sem, 3);
		}
		else if (ret != -1)
			Philosopher(sem, 2);
	}
	else if (ret != -1) 
		Philosopher(sem, 1);
}
else if (ret != -1) //Father
	Philosopher(sem, 0);
```

这段代码是哲学家问题的，其他问题只需要将函数 Philosopher() 改变一下即可，为了解决这几个问题，我一共定义了以下5个函数，均在main.c中。

```c
void Philosopher(sem_t* sem, int id);
void Producer(sem_t* mutex, sem_t* empty,sem_t* buffer, int id);
void Comsumer(sem_t* mutex, sem_t* empty,sem_t* buffer);
void Reader(sem_t* mutex, sem_t* writeblock, sem_t* Rcount, int id);
void Writer(sem_t* mutex, sem_t* writeblock, sem_t* Rcount, int id);
```

除此之外还有一个难题：在读者写者问题中，需要在多个进程之间共享Rcount。

**单纯的 fork 解决不了这个问题，所以我新加了两个系统调用。**

```c
#define SEM_GET_VALUE 4
#define SEM_SET_VALUE 5
int sem_get_value(sem_t *sem);
int sem_set_value(sem_t *sem, int value);
```

**利用一个信号量的 value ，在多个进程之间传递数据，当作一个多进程共享变量**。

###### 3.3.1 哲学家就餐问题

哲学家代码详见app/main.c，此处展示测试结果。

```c
#define Test 2
...
void Philosopher(sem_t* sem, int id);
{
    ...
    printf("Philosopher %d: start eating\n", id);
    sleep(128);                    // 吃面条
	printf("Philosopher %d: finish eating\n", id);
    ...
}
```

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210523020011273.png" alt="image-20210523020011273" style="zoom:80%;" />

在Philosopher中，我用 printf 打印 start eating 和 finish eating 来标志一个哲学家的 Eating 时间区间。

根据测试结果可见，同一时间内，没有相邻的2个哲学家处于 Eating 状态。

###### 3.3.2 生产者-消费者问题 (选做)

代码详见app/main.c，核心在以下两个函数，此处展示测试结果。

```c
#define Test 3
void Producer(sem_t* mutex, sem_t* empty,sem_t* buffer, int id);
void Comsumer(sem_t* mutex, sem_t* empty,sem_t* buffer);
```

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210523020545228.png" alt="image-20210523020545228" style="zoom: 67%;" />

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210523020626700.png" alt="image-20210523020626700" style="zoom:67%;" />

可见，在开始阶段，由于缓冲区较空，会连着有不同的Producer往缓冲区中放入商品。

由于只有一个Consumer，消费速度比生产速度慢，当缓冲区满之后，仅当消费者消费了一个商品，生产者才会放入一个。进程同步性正确。

###### 3.3.2 读者-写者问题 (选做)

代码详见app/main.c，核心在以下两个函数，此处展示测试结果。

```c
#define Test 4
void Reader(sem_t* mutex, sem_t* writeblock, sem_t* Rcount, int id);
void Writer(sem_t* mutex, sem_t* writeblock, sem_t* Rcount, int id);
```

**为了便于辨认，我在这里也加入了print finish reading的语句**。

<img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210523021105797.png" alt="image-20210523021105797" style="zoom:67%;" />

可见，在读进程读时，写进程不可写；在写进程写时，读进程也不可读。

因为不同进程sleep的时长相同（按要求设置了sleep(128)），所以进程的读写节奏比较规整。

如果采用随机数可能会产生不同结果，但是同步性都是正确的。



#### 4. 实验结果

本次实验的实验结果已经在实验内容中展示，此处不再赘述。



#### 5. 思考题

1. 有没有更好的方式处理这个就餐问题？

   答：再定义一个信号量，规定最多同时有4名哲学家持有叉子；根据鸽笼原理，肯定至少有一名哲学家持有2把叉子；如此该名哲学家便可执行吃的操作，并且在有限时间内吃完通心面放下叉子，让其它哲学家取得叉子并且进入吃的状态。

2. P、V的操作顺序有影响吗？

   有影响。假设在以下代码中，P(mutex) 在 P(emptyBuffers) 之前。

   假设某个时间，mutex  = 1，emptyBuffers = 0，fullBuffers = k，执行以下程序步：

   （1）在Deposit(c)中，执行 P(mutex)，mutex = 0；

   （2）在Deposit(c)中，执行 P(emptyBuffers)，emptyBuffers = -1，Deposit(c)被阻塞；

   （3）在Remove(c)中，执行 P(fullBuffers)，fullBuffers = k - 1；

   （4）在Remove(c)中，执行 P(mutex)，mutex = -1，Remove(c)被阻塞；

   由此可见，两个进程均被阻塞，陷入了死锁状态，无法恢复。

   <img src="C:\Users\admin\AppData\Roaming\Typora\typora-user-images\image-20210523002909821.png" alt="image-20210523002909821" style="zoom:80%;" />

<div STYLE="page-break-after: always;"></div>

#### 6. 框架代码的一个小Bug

在kernel/include/x86/memory.h中，NR_SEGMENTS的值定义得太小，无法支持5个进程一起运行。

```c
#define NR_SEGMENTS      10           // GDT size
#define MAX_PCB_NUM ((NR_SEGMENTS-2)/2)
```

这种情况下， MAX_PCB_NUM仅为4；所以我把NR_SEGMENTS修改成了20。

在kernel/include/x86/memory.h中，MAX_SEM_NUM的值定义得太小，无法满足5个哲学家的信号量需求。

```c
#define MAX_SEM_NUM 4
```

之后我把MAX_SEM_NUM修改成了10。