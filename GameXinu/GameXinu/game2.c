/* game.c - xmain, prntr */

#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>

/*
#define BALL_COLOR_NON_BLINK 112
#define PINK_BACK 80
*/

#define BALL_SIZE 4					//amount of ball pixels
#define BALL_COLOR 240				//white blinking color
#define BALL_VALUE '*'

#define CUBE_VALUE 'X'
#define CUBE_SIZE 4					//amount of cube pixels
#define CUBE_COLOR 32				//green color

#define TRACK_VALUE ' '
#define TRACK_COLOR 16				//drak blue color
#define TRACK_COLOR_2 48			//light blue color

#define BORDER_VALUE '|'
#define BORDER_COLOR 131			//light blue foreground blinking color

#define INIT_COLOR 0				//black color
#define WHITE_BLINKING_FOREGROUD 135

#define DEAD_BALL_COLOR 96+128		//blinking orange color
#define DEAD_BALL_VALUE 'O'

#define SEC 18
#define MAX_X(y) (65-y)
#define MIN_X(y) (60-y)
#define MIN_Y 0
#define MAX_Y 24

typedef struct position
{
	int x, y;

}POSITION;

/*---------------------Parameters---------------------*/

int receiver_pid;
char display_draft_attr[25][80], display_draft_val[25][80];
POSITION *cube_pos = NULL, *ball_pos = NULL, *tun_pos = NULL;

char display_attr[2000], display_val[2000], jump_attr_save[4], jump_val_save[4];

int uppid, dispid, recvpid;

char ch_arr[2048];
int front = -1;			//The counter of handled messages from reciver in updater
int rear = -1;			//The counter of messages recived from new_int9 in reciver

int point_in_cycle;
int gcycle_length;
int gno_of_pids;

int sched_arr_pid[5] = { -1 };
int sched_arr_int[5] = { -1 };
int pixelColorFlag = 0;					//for painting -> will change the rows the next paint

int freq = 0;							//time of loading
int countLoading = 0;
int cubeExist = 0;
int cubeEnd = 0;
int jumpingFlag = 0;
/*---------------------------------------------------*/

/*---------------------Functions---------------------*/
extern SYSCALL sleept(int);
extern struct intmap far *sys_imp;
void displayer();
INTPROC new_int9(int mdevno);
void set_new_int9_newisr();
void receiver();
void updateter();
SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...);

/*-----------------------------MY FUNCTIONS-----------------------------------------*/

void change_value_by_pos(int, unsigned char);
void init_display_adapter_and_board();
void init_params();
int get_pos(int, int);

void create_cube();
void update_cube();
int compare_to_cube_pos(int, int);

void init_board();
void init_board_content();
void display_board();
void update_board();

void finish();
void game_over();

/*(+Or- = {-,+}).
Get as parameter what we want to de/increase x.
Returns 1: in track, 0: not in track, -1:error in input*/
int check_ball_exceeds_x(char);
int compare_to_ball_pos(int, int);
void create_ball();
/*(dir = {a,d})
Get as parameter the direction to move the ball
*/
void update_ball(char);
void ball_jumping();
/*---------------------------------------------------------------xmain---------------------------------------------------------------------------*/

xmain()
{
	init_display_adapter_and_board();
	resume(dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0));
	resume(recvpid = create(receiver, INITSTK, INITPRIO + 3, "RECIVEVER", 0));
	resume(uppid = create(updateter, INITSTK, INITPRIO, "UPDATER", 0));
	receiver_pid = recvpid;
	set_new_int9_newisr();
	schedule(2, freq, dispid, 0, uppid, 29);
} // xmain

  /*-----------------------------------------------------------------------------------------------------------------------------------------------*/

void updateter()
{

	int i, x, y, jumpingCnt = -1;
	char ch;

	while (1)
	{
		if(front != -1)
		{
			ch = ch_arr[front];
			if (front != rear)
				front++;
			else
				front = rear = -1;
			if (ch == 'j' || ch == 'J') {
				ball_jumping();
				jumpingCnt = 0;
			}
			else if ((ch == 'a') || (ch == 'A'))			//left
			{
				if (check_ball_exceeds_x('-') == 1)	//not exceeding from track
					update_ball(ch);
			}
			else if ((ch == 'd') || (ch == 'D'))	//right
			{
				if (check_ball_exceeds_x('+') == 1)	//not exceeding from track
					update_ball(ch);
			}
			else if ((ch == 'w') || (ch == 'W'))	//up
			{
				if (freq / 2 > 0)
					freq /= 2;
				else if (freq == 0)	//at start
					freq = SEC;
				schedule(2, freq, dispid, 0, uppid, 29);
			}
			else if ((ch == 's') || (ch == 'S'))	//down
			{
				if (freq * 2 < SEC * 8) {
					freq *= 2;
					schedule(2, freq, dispid, 0, uppid, 29);
				}
			}
		}
		update_board();
		if (jumpingFlag == 1 && jumpingCnt >= 5) {
			ball_jumping();
			jumpingCnt = -1;
		}
		if (cubeEnd == 1)
		{
			cubeExist = 0;
			cubeEnd = 0;
			countLoading = 0;
			create_cube();
		}
		else if (countLoading >= 1) {	//counter of display updates - every 1 load, cube moving
			update_cube();
			countLoading = 0;
		}

		for (y = MIN_Y; y <= MAX_Y; y++){
			for (x = MIN_X(MAX_Y); x <= MAX_X(MIN_Y); x++){
				display_attr[get_pos(x, y) / 2] = display_draft_attr[y][x];
				display_val[get_pos(x, y) / 2] = display_draft_val[y][x];
			}
		}
		if (jumpingCnt != -1)
			jumpingCnt++;
	} // while(1)

} // updater 

  /*called by displayer*/
void display_board()
{
	int x, y, pos;
	for (y = MIN_Y; y <= MAX_Y; y++){
		for (x = MIN_X(MAX_Y); x <= MAX_X(MIN_Y); x++) {
			pos = get_pos(x, y);
			change_value_by_pos(pos + 1, display_attr[pos / 2]);
			change_value_by_pos(pos, display_val[pos / 2]);
		}
	}
}

void create_cube()
{
	int i;
	cube_pos = (POSITION*)(getmem(CUBE_SIZE * sizeof(POSITION)));
	if (cube_pos == NULL)
		finish();
	for (i = 0; i < CUBE_SIZE; i++) {
		cube_pos[i].x = 61 + i;	
		cube_pos[i].y = 0;
	}
}

void update_cube()
{
	int i;
	/*check if the cube reached to the end*/
	for (i = 0; i < CUBE_SIZE; i++)
	{
		if (cubeEnd == 1 || (cube_pos[i].y >= MAX_Y))
		{
			display_draft_attr[cube_pos[i].y][cube_pos[i].x] = TRACK_COLOR;
			display_draft_val[cube_pos[i].y][cube_pos[i].x] = TRACK_VALUE;
			cubeEnd = 1;
		}
	}
	if (cubeEnd == 0) {
		for (i = CUBE_SIZE - 1; i >= 0; i--)
		{
			if (compare_to_ball_pos(cube_pos[i].x, cube_pos[i].y) == 0) {
				game_over();
				return;
			}
			display_draft_attr[cube_pos[i].y][cube_pos[i].x] = TRACK_COLOR;
			display_draft_val[cube_pos[i].y][cube_pos[i].x] = TRACK_VALUE;
			cube_pos[i].x--;	cube_pos[i].y++;
			display_draft_attr[cube_pos[i].y][cube_pos[i].x] = CUBE_COLOR;
			display_draft_val[cube_pos[i].y][cube_pos[i].x] = CUBE_VALUE;
		}
	}
	else
		freemem(cube_pos);
}

void update_ball(dir)
char dir;
{
	int i;
	if (dir == 'd' || dir == 'D') {		//right
		for (i = BALL_SIZE; i >= 0; i--) {
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = TRACK_COLOR;
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = TRACK_VALUE;
			ball_pos[i].x++;
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = BALL_COLOR;
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = BALL_VALUE;
		}
	}
	else if (dir == 'a' || dir == 'A') {	//left
		for (i = 0; i < BALL_SIZE; i++) {
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = TRACK_COLOR;
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = TRACK_VALUE;
			ball_pos[i].x--;
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = BALL_COLOR;
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = BALL_VALUE;
		}
	}
}

void ball_jumping()
{
	int i;
	if(jumpingFlag==0)
	{
		for (i = 0; i < BALL_SIZE; i++) {
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = jump_attr_save[i];
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = jump_val_save[i];
			ball_pos[i].y -= 2;
			jump_attr_save[i] = display_draft_attr[ball_pos[i].y][ball_pos[i].x];
			jump_val_save[i] = display_draft_val[ball_pos[i].y][ball_pos[i].x];
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = BALL_COLOR;
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = BALL_VALUE;
		}
		jumpingFlag = 1;
	}
	else
	{
		for (i = 0; i < BALL_SIZE; i++) {
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = jump_attr_save[i];
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = jump_val_save[i];
			ball_pos[i].y += 2;
			jump_attr_save[i] = display_draft_attr[ball_pos[i].y][ball_pos[i].x];
			jump_val_save[i] = display_draft_val[ball_pos[i].y][ball_pos[i].x];
			display_draft_attr[ball_pos[i].y][ball_pos[i].x] = BALL_COLOR;
			display_draft_val[ball_pos[i].y][ball_pos[i].x] = BALL_VALUE;
		}
		jumpingFlag = 0;
	}
}

void update_board()
{
	int x, y, color, rowFlag = 0;
	for (y = 24; y >= 1; y--) {
		for (x = MIN_X(y); x <= MAX_X(y); x++){
			if (compare_to_ball_pos(x, y) == 0)
				color = BALL_COLOR;
			else if (compare_to_cube_pos(x, y) == 0)
				color = CUBE_COLOR;
			else if (pixelColorFlag <= 1)					//color swap every 2 rows
			{
				if (rowFlag <= 1)	color = TRACK_COLOR;
				else				color = TRACK_COLOR_2;
			}
			else
			{
				if (rowFlag <= 1)	color = TRACK_COLOR_2;
				else				color = TRACK_COLOR;
			}
			display_draft_attr[y][x] = color;
		}
		rowFlag = (rowFlag + 1) % 4;
	}
	pixelColorFlag = (pixelColorFlag + 1) % 4;				//color swap every 2 updates
}

void displayer(void)
{
	while (1)
	{
		receive();
		//sleept(18);
		if (getFlag() == 1) {
			display_board();
			countLoading++;
			setFlag(0);
		}
	} //while
}

void receiver()
{
	while (1)
	{
		char temp;
		temp = receive();
		rear++;
		ch_arr[rear] = temp;
		if (front == -1)
			front = 0;
		//getc(CONSOLE);
	} // while

} //  receiver

SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...)
{
	int i;
	int ps;
	int *iptr;

	disable(ps);

	gcycle_length = cycle_length;
	point_in_cycle = 0;
	gno_of_pids = no_of_pids;

	iptr = &pid1;
	for (i = 0; i < no_of_pids; i++)
	{
		sched_arr_pid[i] = *iptr;
		iptr++;
		sched_arr_int[i] = *iptr;
		iptr++;
	} // for
	restore(ps);

} // schedule 

void set_new_int9_newisr()
{
	int i;
	for (i = 0; i < 32; i++)
		if (sys_imp[i].ivec == 9)
		{
			sys_imp[i].newisr = new_int9;
			return;
		}

} // set_new_int9_newisr

INTPROC new_int9(int mdevno)
{
	char result = 0;
	int scan = 0;
	int ascii = 0;

	asm{
		MOV AH,1
		INT 16h
		JZ Skip1
		MOV AH,0
		INT 16h
		MOV BYTE PTR scan,AH
		MOV BYTE PTR ascii,AL
	} //asm
	if (scan == 57)			//Space
		result = 'j';//jump
	else if (scan == 75)		//Left
			result = 'a';
	else if (scan == 77)		//Right
			result = 'd';
	else if (scan == 72)		//Up
			result = 'w';
	else if (scan == 80)		//Down
			result = 's';
	else if ((scan == 46) && (ascii == 3) || scan == 1) {// Ctrl-C?
			if (cube_pos != NULL)	freemem(cube_pos);	else;
			if (ball_pos != NULL)	freemem(ball_pos);	else;
			asm{
				MOV  AX,2
				INT  10h
				INT	 27
			}
	}
	send(receiver_pid, result);

Skip1:

} // new_int9

  //Change the value of the ES:[pos] address to be 'value'
void change_value_by_pos(pos, value)
int pos;
unsigned char value;
{
	asm{
		PUSH			 BX
		PUSH 			 ES
		MOV              AX,0B800h     			// Segment address of memory on color adapter
		MOV				 ES,AX
		MOV 			 BX, WORD PTR pos		//The (x,y) pixel or attribute
		MOV				 AL, BYTE PTR value		//The new value
		MOV              BYTE PTR ES : [BX], AL
		POP				 ES
		POP				 BX
	}
}

int check_ball_exceeds_x(plusOrminus)
char plusOrminus;
{
	int i;
	if (plusOrminus == '+' || plusOrminus == 'p' || plusOrminus == 'P') {
		for (i = 0; i < BALL_SIZE; i++) {
			if (ball_pos[i].x >= MAX_X(ball_pos[i].y)) {
				return 0;
			}
		}
	}
	else {
		if (plusOrminus == '-' || plusOrminus == 'm' || plusOrminus == 'M') {
			for (i = 0; i < BALL_SIZE; i++)
				if (ball_pos[i].x <= MIN_X(ball_pos[i].y))
					return 0;
		}
		else return -1;
	}
	return 1;
}

int get_pos(x, y)
int x;
int y;
{
	return 2 * (y * 80 + x);
}

int compare_to_ball_pos(x, y)
int x;
int y;
{
	int i;
	for (i = 0; i<BALL_SIZE; i++)
		if (get_pos(ball_pos[i].x, ball_pos[i].y) == get_pos(x, y))
			return 0;
	return 1;
}

int compare_to_cube_pos(x, y)
int x;
int y;
{
	int i;
	for (i = 0; i<CUBE_SIZE; i++)
		if (get_pos(cube_pos[i].x, cube_pos[i].y) == get_pos(x, y))
			return 0;
	return 1;
}

void init_display_adapter_and_board()
{
	asm{
		MOV              AH,0          // Select function = 'Set mode'
		MOV              AL,3          // 80 by 25 color image
		INT              10h           // Adapter initialized. Page 0 displayed

		MOV              AX,0B800h		// Segment address of memory on color adapter
		MOV              ES,AX			// Set up extra segment register
		MOV              DI,0			// Initial offset address into segment
		MOV              AL,' '			// Character space to fill adapter memory
		MOV              AH,00h			// Attribute byte : 
		MOV              CX,2000		// Initialize count, 1 Screen
		CLD								// Write forward
		REP              STOSW			// Do CX times: MOV ES:[DI],AX ->ADD DI,2
										// Do 1000 times: MOV 0B800h:[DI],' ' yellow -> DI++
	}
	init_board();
}

void init_board()
{
	init_board_content();
	create_ball();
	update_board();
	display_board();
}

void finish()
{
	if (cube_pos != NULL)
		freemem(cube_pos);
	if(ball_pos!= NULL)
		freemem(ball_pos);
	asm{
		MOV  AX,2
		INT  10h
		INT	 27
	}
}

void init_board_content()
{
	int x, x1, x2, y, pos_x1, pos_x2;
	for (y = MIN_Y; y <= MAX_Y; y++)
		for (x = MIN_X(MAX_Y); x <= MAX_X(MIN_Y); x++)
			display_draft_attr[y][x] = display_attr[get_pos(x, y) / 2] = INIT_COLOR;  // blank

	for (y = 0; y <= 24; y++)
		for (x = MIN_X(y); x <= MAX_X(y); x++)
			display_draft_attr[y][x] = display_attr[get_pos(x, y) / 2] = TRACK_COLOR;
	
	/*Blinking Borders*/
	for (y = 24; y >= 0; y--) {
		x1 = MIN_X(y) - 1;
		x2 = MAX_X(y) + 1;
		pos_x1 = get_pos(x1, y);
		pos_x2 = get_pos(x2, y);
		change_value_by_pos(pos_x1 + 1, BORDER_COLOR);
		change_value_by_pos(pos_x1, BORDER_VALUE);
		change_value_by_pos(pos_x2 + 1, BORDER_COLOR);
		change_value_by_pos(pos_x2, BORDER_VALUE);
		display_draft_attr[y][x1] = display_attr[pos_x1 / 2] = BORDER_COLOR;
		display_draft_val[y][x1] = display_val[pos_x1 / 2] = BORDER_VALUE;
		display_draft_attr[y][x2] = display_attr[pos_x2 / 2] = BORDER_COLOR;
		display_draft_val[y][x2] = display_val[pos_x2 / 2] = BORDER_VALUE;
	}
	
}

void create_ball()
{
	int i, pos;
	ball_pos = (POSITION*)(getmem(BALL_SIZE * sizeof(POSITION)));
	if (ball_pos == NULL)
		finish();
	ball_pos[0].x = 38;	ball_pos[0].y = 23;
	ball_pos[1].x = 38;	ball_pos[1].y = 24;
	ball_pos[2].x = 39;	ball_pos[2].y = 23;
	ball_pos[3].x = 39;	ball_pos[3].y = 24;

	for (i = 0; i<BALL_SIZE; i++){
		pos = get_pos(ball_pos[i].x, ball_pos[i].y);
		display_draft_attr[ball_pos[i].y][ball_pos[i].x] = display_attr[pos / 2] = BALL_COLOR;
		display_draft_val[ball_pos[i].y][ball_pos[i].x] = display_val[pos / 2] = BALL_VALUE;
	}
}

void init_params()
{
	int i;
	for(i=0;i<BALL_SIZE;i++)
	{
		jump_attr_save[i] = TRACK_COLOR;
		jump_val_save[i] = TRACK_VALUE;
	}
}

void game_over()
{
	int pos, y = 0, i=0,x;
	char str[10] = "Game Over";
	for(x=0;x<=9;x++)
	{
		pos = get_pos(x, y);
		change_value_by_pos(pos + 1, WHITE_BLINKING_FOREGROUD);
		change_value_by_pos(pos, str[i]);
		i++;
	}
	/*for(i=0;i<BALL_COLOR;i++)
	{
		pos = get_pos(ball_pos[i].x, ball_pos[i].y);
		change_value_by_pos(pos + 1, DEAD_BALL_COLOR);
		change_value_by_pos(pos, DEAD_BALL_VALUE);
	}*/
	kill(dispid);
}