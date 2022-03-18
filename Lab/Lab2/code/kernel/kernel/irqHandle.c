#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;


void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		case -1:break;
		case 0xd:GProtectFaultHandle(tf);break;
		case 0x21:KeyboardHandle(tf);break;
		case 0x80:syscallHandle(tf);break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	if(code == 0xe) // 退格符
	{
		// TODO: 要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol <= 0);
		else
		{
			char character = ' ';
			uint16_t data = character | (0x0c << 8);
			int	pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol--;
		}
	}
	else if(code == 0x1c) // 回车符
	{
		displayRow++;
		displayCol = 0;
		if(displayRow == 25)
		{
			displayRow = 24;
			scrollScreen();	
		}
	}
	else if(code < 0x81) // 正常字符
	{
		char character = code & 0xFF;
		uint16_t data = character | (0x0c << 8);
		int	pos = (80*displayRow + displayCol) * 2;
		asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		displayCol++;
		if(displayCol == 80)
		{
			displayRow++;
			if(displayRow == 25)
			{
				displayRow = 24;
				scrollScreen();
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel =  USEL(SEG_UDATA);//segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if(displayRow == 25)
			{
				displayRow = 24;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0c << 8);
			pos = (80*displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol == 80)
			{
				displayRow++;
				if(displayRow == 25)
				{
					displayRow = 24;
					scrollScreen();
				}
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
}
