#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>

/*****

 PINOUT:

	1 - PB0: OUT 	- HSYNC
	2 - PB1: IN		- START
	9  - RESET		- +5V
	10 - VCC		- +5V
	11 - GND
	12 - XTAL1
	13 - XTAL2
	29 - PC7: OUT	- VIDEO
	32 - AREF 		- +5V
	39 - ADC1		- PLAYER 2
	40 - ADC0		- PLAYER 1

******/

#include "fontmap.h"
#define M_PI   		3.14159265358979323846 
#define NAN   		__builtin_nan("")
#define PLAYER1X	10
#define PLAYER2X	122
#define ZERO 		PORTB &=0xFE
#define BLACK 		PORTB |=1

typedef struct {
	char c;
	unsigned short offset;
} CharData;

static const CharData CHAR_DATA[] PROGMEM = {
	{'0',0}, 	{'1',8}, 	{'2',16}, 	{'3',24}, {'4',32}, 	{'5',40}, 	{'6',48}, 	{'7',56},
	{'8',64}, 	{'9',72}, 	{':',80}, 	{';',88}, {'<',96}, 	{'=',104}, 	{'>',112}, 	{'?',120},
	{'@',128},	{'A',136}, 	{'B',144}, 	{'C',152}, {'D',160}, 	{'E',168}, 	{'F',176}, 	{'G',184},
	{'H',192}, 	{'I',200}, 	{'J',208}, 	{'K',216}, {'L',224}, 	{'M',232}, 	{'N',240}, 	{'O',248},
	{'P',256}, 	{'Q',264}, 	{'R',272}, 	{'S',280}, {'T',288}, 	{'U',296}, 	{'V',304}, 	{'W',312},
	{'X',320}, 	{'Y',328}, 	{'Z',336}, 	{'[',344}, {'\\',352}, {']',360}, 	{'^',368},	{' ',376},	
	{'\0',0}
};

static const float ANGLE_TABLE[] PROGMEM = {
	-1.5708, -0.9273, -0.6435, -0.4115, -0.2014, 0,			
	0.2014, 0.4115, 0.6435, 0.9273, 1.5708
};

static const float SIN_TABLE[] PROGMEM = {
	0, 0.174, 0.342, 0.5, 0.643, 0.766, 0.866, 0.94, 0.985, 1, 0.985, 0.94,
	0.866, 0.766, 0.643, 0.5, 0.342, 0.174, 0
};

static const float COS_TABLE[] PROGMEM = {
	1, 0.985, 0.94, 0.866, 0.766, 0.643, 0.5, 0.342, 0.174, 0,
	-0.174, -0.342, -0.5, -0.643, -0.766, -0.866, -0.94, -0.985, -1
};

volatile char buffer[96][17];
volatile char vblank;
volatile short line;
volatile short buffer_line;
volatile short ticks;

short px2,py2,ly1,ly2,oly1,oly2;
float px1,py1;
float dx,dy;
float speed;
unsigned char score1, score2;
short count;
char bounce;

void (*action)();
void (*game_state)();

static void setup();
static void loop();
static void do_sync();
static void draw_line();
static void clear();

static void vlineon(int x, int y, int h);
static void vlineoff(int x, int y, int h);
static void pon(unsigned short x,unsigned short y);
static void poff(unsigned short x,unsigned short y);
static void write(const char* text, unsigned short x, unsigned short y,char progmem);

static void show_presentation();
static void wait_start();
static void game_loop();
static void loose();
static void restart_game();
static void draw_screen();
static void show_winner();
static void wait_restart();
static void draw_scores();
static void read_controls();
static void reset_vars();

static short abs(short x);
static double _atan(double x, double y);
static double _sin(double x);
static double _cos(double x);

static short abs(short x){
	return x >= 0 ? x : -x;
}

static double _sin(double x){
	int index = ((int)((x * 180 / M_PI)/10));
	if (index <= 18){
		return pgm_read_float(SIN_TABLE+index);
	} else {
		return -pgm_read_float(SIN_TABLE+(36-index));
	}
}

static double _cos(double x){
	int index = ((int)((x * 180 / M_PI)/10));
	if (index <= 18){
		return pgm_read_float(COS_TABLE+index);
	} else {
		return pgm_read_float(COS_TABLE+(36-index));
	}
}

static double _atan(double y, double x){
	double r;
	double pw;
	double v;
	if (x == 0 && y == 0){
		return NAN;
	}
	r = y/x;
	pw = r*r*r;
	v = r;
	v-=pw/3.0;
	pw*=r*r;
	v+=pw/5.0;
	pw*=r*r;
	v-=pw/7.0;
	pw*=r*r;
	v+=pw/9.0;
	return v;
}

static void change_dir(char p1){
	int delta = (int)(5+(py1 - (p1 ? ly1 : ly2)) / 10.0);
	double angle = _atan(dy, dx);
	angle = (p1 ? 2*M_PI : M_PI) - angle + pgm_read_float(ANGLE_TABLE+delta);
	
	dx = _cos(angle) * speed;
	dy = _sin(angle) * speed;
}

static void write(const char* text, unsigned short x, unsigned short y,char progmem){
	int i,j,k,l;
	char* charpointer;
	CharData* char_data;
	char c,cc;
	unsigned short offset;

	l = strlen(text);
	y <<= 3;

	for (i=0;i<l;i++){
		cc = progmem ? pgm_read_byte(text+i) : text[i];
		for (j=0;;j++){
			char_data = (CharData*)(CHAR_DATA+j);
			
			c = pgm_read_byte(&char_data->c);
			
			if (c == 0){
				break;
			}
			
			if (c == cc){
				offset = pgm_read_word(&char_data->offset);
				charpointer = (char*)FONTMAP+offset;
				for (k=0;k<8;k++,charpointer++){
					buffer[y + k][x+i] = pgm_read_byte(charpointer);
				}
				break;
			}
		}
	}
}


static void show_presentation(){
	score1 = score2 = 0;
	reset_vars();
	clear();
	write(PSTR("P I N G"),5,5,1);
	write(PSTR("P O N G"),5,8,1);
	game_state = wait_start;
	bounce = 0;
}

static void wait_start(){
	if (PINB & 2){
		clear();
		draw_scores();
		game_state = game_loop;
	}
}

static void show_winner(){
	clear();
	write(PSTR("GANADOR:"),4,4,1);
	write(score1 > score2 ? PSTR("JUGADOR 1") : PSTR("JUGADOR 2"),4,6,1);
	game_state = wait_restart;
}

static void wait_restart(){
	if (PINB & 2){
		game_state = show_presentation;
	}
}

static void game_loop(){

	read_controls();

	if (ticks > 2){
		ticks = 0;
		draw_screen();
		px2 = px1;
		py2 = py1;

		
		if (dx <0 && px1 <= PLAYER1X+1 && px1 >=PLAYER1X && abs(py1-ly1) <=5 && bounce){
			change_dir(1);
			count++;
			bounce = 0;
		} else if (dx > 0 && px2 <= PLAYER2X && px2 >= PLAYER2X-1 && abs(py1-ly2) <= 5 && !bounce){
			change_dir(0);
			count++;
			bounce = 1;
		} else if (px1 > 134){
			score1++;
			game_state = loose;
		} else if (px1 < 2){
			score2++;
			game_state = loose;
		}
		
		if (py1 >81){
			dy=-dy;
			py1--;
		} else if (py1 < 10){
			dy=-dy;
			py1++;
		}

		if (count > 6){
			speed *= 1.1;
			count = 0;
		}

		px1+=dx;
		py1+=dy;
	}
}

static void loose(){
	if (score1 > 4 || score2 > 4){
		game_state = show_winner;
	} else {
		ticks = 0;
		draw_scores();
		game_state = restart_game;
	}
}

static void restart_game(){
	if (ticks > 100){
		reset_vars();
		game_state = game_loop;
	}
}

static void reset_vars(){
	ly1 = ly2 = 47;
	px1 = px2 = 20;
	py1 = py2 = 20;
	dx = dy = 1;
	count = 0;
	speed = 1.4;
}

static void clear(){
	int i,j;
	for (i=0;i<96;i++){
		for (j=0;j<17;j++){
			buffer[i][j]=0x0;
		}
	}
}

static void pon(unsigned short x,unsigned short y){
	buffer[y][x >> 3] |= 1 << ((~x) & 7);
}

static void poff(unsigned short x,unsigned short y){
	buffer[y][x >> 3] &= ~(1 << ((~x) & 7));
}

int main(){
	setup();
	while (1) loop();
	return 0;
}

static void setup(){
	game_state = show_presentation;
	line = -1;
	buffer_line = 0;
	vblank = 0;
	action = do_sync;

	DDRC = 0x80;
	DDRB = 0x03;
	DDRA = 0xFC;

	// timer
    TCCR1B |= (1 << WGM12)|(1 << CS10);
    TCNT1 = 0;
    OCR1A = (F_CPU/1000000)*64;
    TIMSK |= (1 << OCIE1A);

	// ADC
	ADMUX = 0;
	ADCSRA = (1<<ADEN) | (1<<ADSC);
    sei();

}

static void read_controls(){
	if (!(ADCSRA & (1 << ADSC))){
		unsigned short y = 14 + (((unsigned short)ADC)  / 16);
		switch (ADMUX & 3){
			case 0:
				ly1 = y;
				ADMUX = 1;
				break;
			case 1:
				ly2 = y;
				ADMUX = 0;
				break;
		}
		ADCSRA |= 1 << ADSC;
	}
}

static void loop(){
	if (vblank){
		game_state();
	}
}	

ISR (TIMER1_COMPA_vect){
	action();
}

static void do_sync(){
	_delay_us(28);BLACK;_delay_us(4);ZERO;_delay_us(28);BLACK;_delay_us(4);	
	TCNT1 = 0;
	action = draw_line;
}

static void draw_line(){
	line++;

	ZERO; 
	_delay_us(4); 
	BLACK; 
	_delay_us(11.5);

	if (line < 60 || line >= 252){
		BLACK;
		if(!vblank){
			vblank = 1;
			ticks++;
		}
	} else {
		volatile char* current= buffer[buffer_line>>1];
		int j;
		unsigned char k;
		vblank = 0;
		for (j=0;j<17;j++,current++){
			PORTC = *current;
			for (k=0;k<7;k++){
				PORTC <<=1;
			}
		}
		asm volatile ("nop");
		PORTC=0;
		buffer_line++;
	}
	if (line >=312){
		line = -1;
		buffer_line = 0;
		TIFR |= 1<<OCF1A;
		action = do_sync;
	}
}

static void draw_scores(){
	char buff[3];

	sprintf(buff,"%d",score1);
	write(buff,0,0,0);

	sprintf(buff,"%d",score2);
	write(buff,16,0,0);
}


static void draw_screen(){
	int i;

	buffer[9][0]=0x7f;
	buffer[82][0]=0x7f;
	for (i=1;i<16;i++){
		buffer[9][i]=0xff;
		buffer[82][i]=0xff;
	}
	buffer[9][16]=0xfe;
	buffer[82][16]=0xfe;

	for (i=10;i<82;i++){
		buffer[i][0]=0x40;
		buffer[i][16]=0x02;
		buffer[i][7]=0x1;
		buffer[i][8]=0x80;
	}

	vlineoff(PLAYER1X,oly1-5,10);
	vlineon(PLAYER1X,ly1-5,10);
	vlineoff(PLAYER2X,oly2-5,10);
	vlineon(PLAYER2X,ly2-5,10);

	oly1 = ly1;
	oly2 = ly2;

	poff(px2,py2);
	pon(px1,py1);

	ticks = 0;
}

static void vlineon(int x,int y,int h){
	int i;

	for (i=0;i<h;i++,y++){
		pon(x,y);
	}
}

static void vlineoff(int x, int y,int h){
	int i;
	for (i=0;i<h;i++,y++){
		poff(x,y);
	}
}
