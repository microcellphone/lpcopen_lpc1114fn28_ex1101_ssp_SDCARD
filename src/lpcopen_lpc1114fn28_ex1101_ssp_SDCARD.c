/*
===============================================================================
 Name        : lpcopen_lpc1114fn28_ex1101_ssp_SDCARD.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>

// TODO: insert other include files here
#include "my_lpc1114fn28.h"
#include <string.h>
#include "xprintf.h"
#include "diskio.h"
#include "pffconf.h"
#include "pff.h"

// TODO: insert other definitions and declarations here
char Line[128];		/* Console input buffer */

static void put_drc (uint8_t res)
{
    xprintf("rc=%d\n\r", res);
}

static void put_rc (FRESULT rc)
{
	const char *p;
	FRESULT i;
	static const char str[] =
		"OK\0DISK_ERR\0NOT_READY\0NO_FILE\0NOT_OPENED\0NOT_ENABLED\0NO_FILE_SYSTEM\0";

	for (p = str, i = 0; i != rc && *p; i++) {
		while(*p++) ;
	}
	xprintf("rc=%u FR_%S\n", rc, p);
}

static void put_dump (
	const void* buff,		/* Pointer to the array to be dumped */
	unsigned long addr,		/* Heading address value */
	int len,				/* Number of items to be dumped */
	int width				/* Size of the items (DW_CHAR, DW_SHORT, DW_LONG) */
)
{
	int i;
	const unsigned char *bp;
	const unsigned short *sp;
	const unsigned long *lp;


	xprintf("%08lX:", addr);		/* address */

	switch (width) {
	case DW_CHAR:
		bp = buff;
		for (i = 0; i < len; i++)		/* Hexdecimal dump */
			xprintf(" %02X", bp[i]);
		xputc(' ');
		for (i = 0; i < len; i++)		/* ASCII dump */
			xputc((bp[i] >= ' ' && bp[i] <= '~') ? bp[i] : '.');
		break;
	case DW_SHORT:
		sp = buff;
		do								/* Hexdecimal dump */
			xprintf(" %04X", *sp++);
		while (--len);
		break;
	case DW_LONG:
		lp = buff;
		do								/* Hexdecimal dump */
			xprintf(" %08LX", *lp++);
		while (--len);
		break;
	}

#if !_LF_CRLF
	xputc('\r');
#endif
	xputc('\n');
}

void UART_IRQHandler(void)
{
	uint8_t IIRValue, LSRValue;
	uint8_t Dummy = Dummy;
	unsigned char Buf;
	uint8_t d;
	int i, cnt;


	IIRValue = Chip_UART_ReadIntIDReg(LPC_USART);
	IIRValue >>= 1;			/* skip pending bit in IIR */
	IIRValue &= 0x07;			/* check bit 1~3, interrupt identification */

	if (IIRValue == IIR_RLS){		/* Receive Line Status */
		LSRValue = Chip_UART_ReadLineStatus(LPC_USART);
		/* Receive Line Status */
		if (LSRValue & (UART_LSR_OE | UART_LSR_PE | UART_LSR_FE | UART_LSR_RXFE | UART_LSR_BI)){
			/* There are errors or break interrupt */
			/* Read LSR will clear the interrupt */
			UARTStatus = LSRValue;
			Dummy = Chip_UART_ReadByte(LPC_USART);	/* Dummy read on RX to clear interrupt, then bail out */
			return;
		}
		if (LSRValue & UART_LSR_RDR){	/* Receive Data Ready */
			/* If no error on RLS, normal ready, save into the data buffer. */
			/* Note: read RBR will clear the interrupt */
			Buf = Chip_UART_ReadByte(LPC_USART);
		}
	}else if (IIRValue == IIR_RDA){	/* Receive Data Available */
		/* Receive Data Available */
		i = RxBuff.wi;
		cnt = RxBuff.ct;
		while (Chip_UART_ReadLineStatus(LPC_USART) & UART_LSR_RDR) {	/* Get all data in the Rx FIFO */
			d = Chip_UART_ReadByte(LPC_USART);
			if (cnt < BUFF_SIZE) {	/* Store data if Rx buffer is not full */
				RxBuff.buff[i++] = d;
				i %= BUFF_SIZE;
				cnt++;
			}
		}
		RxBuff.wi = i;
		RxBuff.ct = cnt;
	}else if (IIRValue == IIR_CTI){	/* Character timeout indicator */
		/* Character Time-out indicator */
		UARTStatus |= 0x100;		/* Bit 9 as the CTI error */
	}else if (IIRValue == IIR_THRE){	/* THRE, transmit holding register empty */
		cnt = TxBuff.ct;
		if(cnt){/* There is one or more byte to send */
			i = TxBuff.ri;
			for (d = 16; d && cnt; d--, cnt--){	/* Fill Tx FIFO */
				Chip_UART_SendByte(LPC_USART, TxBuff.buff[i++]);
				i %= BUFF_SIZE;
			}
			TxBuff.ri = i;
			TxBuff.ct = cnt;
		}else{
			TxBuff.act = 0; /* When no data to send, next putc() must trigger Tx sequense */
		}
	}
	return;
}

int main(void) {

#if defined (__USE_LPCOPEN)
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
#if !defined(NO_BOARD_LIB)
    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);
#endif
#endif

    // TODO: insert code here
	char *ptr;
	long p1, p2;
	BYTE res;
	UINT s1, s2, s3, ofs, cnt, w;
	FATFS fs;			/* File system object */
	DIR dir;			/* Directory object */
	FILINFO fno;		/* File information */

	IOCON_Config_Request();
	UART_Config_Request(115200);
	xdev_out(uart0_putc);
	xdev_in(uart0_getc);
	xprintf ("lpcopen_lpc1114fn28_ex1101_ssp_SDCARD\n") ;

    // Force the counter to be placed into memory
    volatile static int i = 0 ;
    // Enter an infinite loop, just incrementing a counter
    while(1) {
		xputc('>');
		xgets(Line, sizeof Line);
		ptr = Line;

		switch (*ptr++) {

		case 'd' :
			switch (*ptr++) {
			case 'i' :	/* di - Initialize physical drive */
				res = disk_initialize();
				put_drc(res);
				break;

			case 'd' :	/* dd <sector> <ofs> - Dump partial secrtor 128 bytes */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				s2 = p2;
				res = disk_readp((BYTE*)Line, p1, s2, 128);
				if (res) { put_drc(res); break; }
				s3 = s2 + 128;
				for (ptr = Line; s2 < s3; s2 += 16, ptr += 16, ofs += 16) {
					s1 = (s3 - s2 >= 16) ? 16 : s3 - s2;
					put_dump((BYTE*)ptr, s2, s1, 16);
				}
				break;
			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi - Mount the volume */
				put_rc(pf_mount(&fs));
				break;

			case 'o' :	/* fo <file> - Open a file */
				while (*ptr == ' ') ptr++;
				put_rc(pf_open(ptr));
				break;
#if PF_USE_READ
			case 'd' :	/* fd - Read the file 128 bytes and dump it */
				ofs = fs.fptr;
				res = pf_read(Line, sizeof Line, &s1);
				if (res != FR_OK) { put_rc(res); break; }
				ptr = Line;
				while (s1) {
					s2 = (s1 >= 16) ? 16 : s1;
					s1 -= s2;
					put_dump((BYTE*)ptr, ofs, s2, DW_CHAR);
					ptr += 16; ofs += 16;
				}
				break;

			case 't' :	/* ft - Type the file data via dreadp function */
				do {
					res = pf_read(0, 32768, &s1);
					if (res != FR_OK) { put_rc(res); break; }
				} while (s1 == 32768);
				break;
#endif
#if PF_USE_WRITE == 1
			case 'w' :	/* fw <len> <val> - Write data to the file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				for (s1 = 0; s1 < sizeof Line; Line[s1++] = (BYTE)p2) ;
				p2 = 0;
				while (p1) {
					if ((UINT)p1 >= sizeof Line) {
						cnt = sizeof Line; p1 -= sizeof Line;
					} else {
						cnt = (UINT)p1; p1 = 0;
					}
					res = pf_write(Line, cnt, &w);	/* Write data to the file */
					p2 += w;
					if (res != FR_OK) { put_rc(res); break; }
					if (cnt != w) break;
				}
				res = pf_write(0, 0, &w);		/* Finalize the write process */
				put_rc(res);
				if (res == FR_OK)
					xprintf("%lu bytes written.\n", p2);
				break;

			case 'p' :	/* fp - Write console input to the file */
				xputs("Enter lines to write. A blank line finalize the write operation.\n");
				for (;;) {
					xgets(Line, sizeof Line);
					if (!Line[0]) break;
					strcat(Line, "\r\n");
					res = pf_write(Line, strlen(Line), &w);	/* Write a line to the file */
					if (res) break;
				}
				res = pf_write(0, 0, &w);		/* Finalize the write process */
				put_rc(res);
				break;
#endif
#if PF_USE_LSEEK == 1
			case 'e' :	/* fe - Move file pointer of the file */
				if (!xatoi(&ptr, &p1)) break;
				res = pf_lseek(p1);
				put_rc(res);
				if (res == FR_OK)
					xprintf("fptr = %lu(0x%lX)\n", fs.fptr, fs.fptr);
				break;
#endif
#if PF_USE_DIR == 1
			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				res = pf_opendir(&dir, ptr);
				if (res) { put_rc(res); break; }
				s1 = 0;
				for(;;) {
					res = pf_readdir(&dir, &fno);
					if (res != FR_OK) { put_rc(res); break; }
					if (!fno.fname[0]) break;
					if (fno.fattrib & AM_DIR)
						xprintf("   <DIR>   %s\n", fno.fname);
					else
						xprintf("%9lu  %s\n", fno.fsize, fno.fname);
					s1++;
				}
				xprintf("%u item(s)\n", s1);
				break;
#endif
			}
			break;
		}

        i++ ;
        // "Dummy" NOP to allow source level single
        // stepping of tight while() loop
        __asm volatile ("nop");
    }

    return 0 ;
}
