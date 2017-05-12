/*
 * main.c for microAptiv_UP MIPS core running on Nexys4DDR
 * Prints \n\rMIPSfpga\n\r out via UART.
 * Default baudrate 115200,8n1.
 * Also display a shifting 0xf on the LEDs
 */

#include "fpga.h"

#define inline_assembly()  asm("ori $0, $0, 0x1234")

#define CLK_FREQ 50000000

#define UART_BASE 0xB0401000	//With 1000 offset that axi_uart16550 has
#define rbr		0*4
#define ier		1*4
#define fcr		2*4
#define lcr		3*4
#define mcr		4*4
#define lsr		5*4
#define msr		6*4
#define scr		7*4

#define thr		rbr
#define iir		fcr
#define dll		rbr
#define dlm		ier

#define PWM_BASE 0xB0C00000

void delay();
void uart_outbyte(char c);
char uart_inbyte(void);
void uart_print(const char *ptr);

// The following is for motor
#define WHEEL_RF 0xB1000000
#define WHEEL_LB 0xB1200000
#define WHEEL_LF 0xB1400000
#define WHEEL_RB 0xB1800000

#define dir_data 0xB1A00000
#define dir_backward 0x0000001B
#define dir_forward 0x000000E4

#define rb_f 0x00000020
#define lb_f 0x00000040
#define rf_f 0x00000080
#define lf_f 0x00000004
#define rb_b 0x00000010
#define lb_b 0x00000008
#define rf_b 0x00000002
#define lf_b 0x00000001

#define speed1 1024*1024-1
#define speed2 768*1024
#define speed3 384*1024
#define speed4 0



int gear2speed(int *gear);
void set_gear(int lf, int lb, int rf, int rb);
void _go(void);
void speedup(void);
void slowdown(void);
void leftforward(void);
void rightforward(void);
void turnright(void);
void turnleft(void);
void mystop(void);
void roundClockwise(void);
void happy();


// The following is for bluetooth
#define BT_UART_BASE 0xB0A01000
char BT_uart_inbyte(void);
void init_bluetooth(void);

volatile int BT_rxData = 0;
int gear_lf = 0;
int gear_rf = 0;
int gear_lb = 0;
int gear_rb = 0;
int state = 0;

#define init_round 432
#define sep_round 182
int round = 0;

#define IO_7SEG 0xb0800000

//------------------
// main()
//------------------
int main() {
	// volatile unsigned int pushbutton, count = 0xF;
	volatile unsigned int j = 1;

	*WRITE_IO(UART_BASE + lcr) = 0x00000080; // LCR[7]  is 1
	delay();
	*WRITE_IO(UART_BASE + dll) = 27; // DLL msb. 115200 at 50MHz. Formula is Clk/16/baudrate. From axi_uart manual.
	delay();
	*WRITE_IO(UART_BASE + dlm) = 0x00000000; // DLL lsb.
	delay();
	*WRITE_IO(UART_BASE + lcr) = 0x00000003; // LCR register. 8n1 parity disabled
	delay();
	*WRITE_IO(UART_BASE + ier) = 0x00000000; // IER register. disable interrupts
	delay();

	*WRITE_IO(UART_BASE + ier) = 0x00000001; // IER register. Enables Receiver Line Status and Received Data Interrupts
	delay();

	init_bluetooth();

	/* Prompt the user to select a brightness value ranging from  0 to 9. */
	int count = 0;

	while(1) {
		// LEDs display

		// *WRITE_IO(IO_7SEG) = gear_lf + gear_lb << 4 + gear_rf << 8 + gear_rb << 12;
		// delay();
		*WRITE_IO(IO_LEDR) = count++;
		delay();


		if (round >= sep_round) {
			rightforward();
			round--;
		}
		else if (round > 0) {
			leftforward();
			round--;
			if(round == 0) round = init_round;
		}

		//inline_assembly();
		// End of LEDs display
	}

	return 0;
}

void delay() {
	volatile unsigned int j;

	for (j = 0; j < (1000); j++) ;	// delay
}

void uart_outbyte(char c) {
	*WRITE_IO(UART_BASE + thr) = (unsigned int) c;
	delay( );
}

char uart_inbyte(void) {
	unsigned int RecievedByte;

	while(!((*READ_IO(UART_BASE + lsr) & 0x00000001)==0x00000001));

	RecievedByte = *READ_IO(UART_BASE + rbr);

	return (char)RecievedByte;
}

void uart_print(const char *ptr)
{
	while (*ptr) {
		uart_outbyte (*ptr);
		ptr++;
	}
}


void _mips_handle_irq(void* ctx, int reason) {
	unsigned int value = 0;
	unsigned int period = 0;

	// *WRITE_IO(UART_BASE + ier) = 0x00000000; // IER register. disable interrupts

	*WRITE_IO(IO_LEDR) = 0xF00F;  // Display 0xFFFF on LEDs to indicate receive data from uart
	delay();

	BT_rxData = BT_uart_inbyte();
	if (BT_rxData == 'w') {
		round = 0;
		*WRITE_IO(IO_LEDR) = 0x1;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		speedup();
	}
	else if (BT_rxData == 's') {
		round = 0;
		*WRITE_IO(IO_LEDR) = 0x2;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		slowdown();
	}
	else if (BT_rxData == 'q') {
		*WRITE_IO(IO_LEDR) = 0x4;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		leftforward();
	}
	else if (BT_rxData == 'e') {
		*WRITE_IO(IO_LEDR) = 0x8;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		rightforward();
	}
	else if (BT_rxData == 'd') {
		round = 0;
		*WRITE_IO(IO_LEDR) = 0x10;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		turnright();
	}
	else if (BT_rxData == 'a') {
		round = 0;
		*WRITE_IO(IO_LEDR) = 0x20;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		turnleft();
	}
	else if (BT_rxData == '8') {
		*WRITE_IO(IO_LEDR) = 0x20;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		roundClockwise();
	}
	else if (BT_rxData == 'h') {
		round = 0;
		*WRITE_IO(IO_LEDR) = 0x20;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		happy();
	}
	else {
		round = 0;
		*WRITE_IO(IO_LEDR) = 0x40;  // Display 0xFFFF on LEDs to indicate receive data from uart
		delay();
		mystop();
	}
	//

		*WRITE_IO(IO_LEDR) = 0xFFFF;  // Display 0xFFFF on LEDs to indicate receive data from uart
	return;
}

void _go(void)
{
	int direction = 0;
	direction = direction | (gear_rf>=0?rf_f:rf_b);
	direction = direction | (gear_rb>=0?rb_f:rb_b);
	direction = direction | (gear_lf>=0?lf_f:lf_b);
	direction = direction | (gear_lb>=0?lb_f:lb_b);
	*WRITE_IO(dir_data) = direction;
	delay();
	*WRITE_IO(WHEEL_RF) = gear2speed(&gear_rf);
	delay();
	*WRITE_IO(WHEEL_LB) = gear2speed(&gear_lb);
	delay();
	*WRITE_IO(WHEEL_LF) = gear2speed(&gear_lf);
	delay();
	*WRITE_IO(WHEEL_RB) = gear2speed(&gear_rb);
	delay();
}

// The following is for bluetooth
void init_bluetooth(void) {
	*WRITE_IO(BT_UART_BASE + lcr) = 0x00000080; // LCR[7]  is 1
	delay();
	*WRITE_IO(BT_UART_BASE + dll) = 69; // DLL msb. 115200 at 50MHz. Formula is Clk/16/baudrate. From axi_uart manual.
	delay();
	*WRITE_IO(BT_UART_BASE + dlm) = 0x00000001; // DLL lsb.
	delay();
	*WRITE_IO(BT_UART_BASE + lcr) = 0x00000003; // LCR register. 8n1 parity disabled
	delay();
	*WRITE_IO(BT_UART_BASE + ier) = 0x00000001; // IER register. disable interrupts
	delay();
}

char BT_uart_inbyte(void) {
	unsigned int RecievedByte;

	while(!((*READ_IO(BT_UART_BASE + lsr) & 0x00000001)==0x00000001));

	RecievedByte = *READ_IO(BT_UART_BASE + rbr);

	return (char)RecievedByte;
}

int gear2speed(int *gear) {
	if (*gear == 1) {
		return speed3;
	}
	else if (*gear == 2) {
		return speed2;
	}
	else if (*gear == 3) {
		return speed1;
	}
	else if (*gear == -1) {
		return speed3;
	}
	else if (*gear == -2) {
		return speed2;
	}
	else if (*gear == -3) {
		return speed1;
	}
	else if (*gear >= 3) {
		*gear = 3;
		return speed1;
	}
	else if (*gear <= -3) {
		*gear = -3;
		return speed1;
	}
	else {
		return 0;
	}
}

void speedup(void) {
	//小车当前状态不是直行
	if (state != 0) {
		state = 0;
		set_gear(2, 2, 2, 2);
	}
	else {
		set_gear(gear_lf + 1, gear_lb + 1, gear_rf + 1, gear_rb + 1);
	}
}

void slowdown(void) {
	if (state != 0) {
		state = 0;
		set_gear(0, 0, 0, 0);
	}
	else {
		set_gear(gear_lf - 1, gear_lb - 1, gear_rf - 1, gear_rb - 1);
	}
}
void leftforward(void) {
	state = 1;
	set_gear(0, 1, 3, 3);
}

void rightforward(void) {
	state = 1;
	set_gear(3, 3, 0, 1);
}

void turnright(void) {
	state = 1;
	set_gear(2, 2, -2, -2);
}

void turnleft(void) {
	state = 1;
	set_gear(-2, -2, 2, 2);
}

void mystop(void) {
	state = 0;
	set_gear(0, 0, 0, 0);
}

void set_gear(int lf, int lb, int rf, int rb) {
	gear_lb = lb;
	gear_lf = lf;
	gear_rb = rb;
	gear_rf = rf;
	_go();
}

void roundClockwise()
{
	round = init_round;
}

void happy()
{
	int i = 100,k;
	speedup();
	speedup();
	speedup();
	k = i;
	while(k--)	delay();
	slowdown();
	slowdown();
	slowdown();
	slowdown();
	slowdown();
	slowdown();
	k = i;
	while(k--)	delay();
	turnleft();
	k = i/2;
	while(k--)	delay();
	turnright();
	k = i;
	while(k--)	delay();
	turnleft();
	k = i/2;
	while(k--)	delay();
	mystop();
}