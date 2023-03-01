#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>


/*
*/

#define NO_FUNCTION 0
#define NO_SYMBOL 1<<6


//////////////////////////////////////////////////////////////
///////////////   INSTRUCTION SET SUMMARY ////////////////////
//////////////////////////////////////////////////////////////

#define MOV 	0
#define CAL 	1
#define RET 	2
#define REF 	3
#define ADD 	4
#define PRINT 	5
#define NOT 	6
#define EQU 	7

#define CNST 	0
#define RGSTR 	1
#define SYMBL 	2
#define PNTR 	3

#define SP		5
#define SZ		6
#define PC		7


const char *opcodes[8] = {
	[0] = "MOV  ", [1] = "CAL  ", [2] = "RET  ", [3] = "REF  ",
	[4] = "ADD  ", [5] = "PRINT", [6] = "NOT  ", [7] = "EQU  ",
};

const char *argcodes[4] = {
	[0] = "CNST  ", [1] = "RGSTR ", [2] = "SYMBL ", [3] = "PNTR  ",
};

const char stack_symbols[32] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
	'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
	'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
};

const char *bit_rep[16] = {
    [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
    [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
    [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
    [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
};

const char *exit_codes[4] = {
	[0] = "RAM overflow", [1] = "Stack overflow", [2] = "Undefined error", [3] = "Normal",
};



unsigned char *map_file(const char *address, off_t *res, int *fp)
{
	int fd = open(address, O_RDONLY, (mode_t)0600);
    
    if (fd == -1)
    {
    	// Error opening
        *res = -1;
        return NULL;
    }        
    
    struct stat fileInfo = {0};
    
    if (fstat(fd, &fileInfo) == -1)
    {
    	// Error getting size
        *res = -2;
        close(fd);
        return NULL;
    }
    
    if (fileInfo.st_size == 0)
    {
    	// Empty file
        *res = -3;
        close(fd);
        return NULL;
    }
        
    unsigned char *map = mmap(0, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
    {
    	// Error mapping the file
    	*res = -4;
        close(fd);
    }

    *fp = fd;
    *res = fileInfo.st_size;
    return map;
}

int unmap_file(unsigned char *map, int fd, off_t sz)
{
	if (munmap(map, sz) == -1)
    {
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

void print_byte(unsigned char byte)
{
    printf("%s%s", bit_rep[byte >> 4], bit_rep[byte & 0x0F]);
}

void print_instruction(unsigned char opcode){
	if(opcode > 7){
		return;
	}
	printf("    %s ", opcodes[opcode]);
	return;
}

unsigned char read_bits(unsigned char *map, int rd_sz, int *pos, int *err)
{
	if(*pos < rd_sz-1 || rd_sz > 8) {
		*err = 1;
		return 0;
	}

	unsigned char acc = 0;
	unsigned int page = *pos/8;
	unsigned char bit_mask = 1<<(7-*pos%8);

	for(int i = 0; i<rd_sz; ++i)
	{
		if(!bit_mask)
		{
			bit_mask = 1;
			if(page==0){
				*err = -1;
				return 0;
			}
			page--;
		}
		acc |= ((map[page]&bit_mask)>0)<<i;
		bit_mask<<=1;
	}
	*pos -= rd_sz;
	return acc;
}


void print_args(unsigned char argcode, unsigned char argv)
{
	switch(argcode)
	{
		case CNST:
		printf(" %s %d", argcodes[argcode], argv);
		break;
		case RGSTR:
		printf(" %s %s%d", argcodes[argcode], "0x0", argv);
		break;
		case SYMBL:
		printf(" %s %c   ", argcodes[argcode], stack_symbols[argv]);
		break;
		case PNTR:
		printf(" %s %s%c%s", argcodes[argcode], "[", stack_symbols[argv], "]");
		break;
	}
}


void write_val(unsigned char* RAM, unsigned char sp, unsigned char *regs, unsigned char argcode, unsigned char argval, unsigned char value)
{
	switch(argcode)
	{
		case SYMBL:
		RAM[sp + argval] = value;
		break;
		case PNTR:
		RAM[RAM[sp + argval]] = value;
		break;
		case RGSTR:
		regs[argval] = value;
		break;
	}
}

unsigned char read_val(unsigned char* RAM, unsigned char sp, unsigned char *regs, unsigned char argcode, unsigned char argval)
{
	switch(argcode)
	{
		case SYMBL:
		return RAM[sp + argval];
		break;
		case PNTR:
		return RAM[RAM[sp + argval]];
		break;
		case RGSTR:
		return regs[argval];
		break;
		case CNST:
		return argval;
		break;
	}
}


int main(int argc, char const *argv[])
{

	unsigned char RAM[1<<8];
	memset(RAM, 0, sizeof(unsigned char)*(1<<8));


    ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////  READING FILE AND LOADING CONTENTS INTO MEMORY  ///////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////


	if(argc == 1)
	{
		printf("No arguments provided\n");
		return -1;
	}

	const char *address = argv[1];
	int fd;
	off_t res;
	unsigned char *map = map_file(address, &res, &fd);

	switch(res)
	{
		case -1:
		printf("Error opening file\n");
		break;
		case -2:
		printf("Error getting file size\n");
		break;
		case -3:
		printf("Empty file\n");
		break;
		case -4:
		printf("Error creating mapping\n");
		break;
		default:
		break;
	}

	//////////////////////////////////////////////////////////////////////////////////
    ///////////////          DISPLAYING THE MACHINE CODE          ////////////////////
    //////////////////////////////////////////////////////////////////////////////////

	printf("\nMachine code at %s :\n", address);
    for(int i = 0; i<res; ++i)
    {
    	print_byte(map[i]);
    }

    printf("\n\n");
    

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////   PARSING THE ASSEMBLY CODE AND LOADING IT TO VIRTUAL RAM   ////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////

	printf("Loading parsed instructions into virtual RAM...\n");
    
    int func_size = 0;
    int seg_end = res*8 - 1;
    int seg_ptr = seg_end;
    int RAM_ptr = (1<<8) -1;
    int err;
	unsigned char curr_op, arg1, arg2, arg1v, arg2v;

	int line_number;

    while(err!=-1)
    {
	    func_size = read_bits(map, 5, &seg_ptr, &err);
		if(func_size <= 0)
		{
			break;
		}

	    line_number = func_size;
	    for(int i = 0; i < func_size; ++i)
	    {
	    	if(RAM_ptr <= 8)
	    	{
	    		// RAM overflow
	    		printf("L %d : Overflow has occured. code size exceeds %d bytes\n", line_number, 248);
	    		err = -1;
	    		break;
	    	}
	    	curr_op = read_bits(map, 3, &seg_ptr, &err);
	    	line_number--;
	    	switch(curr_op)
	    	{
	    		// MOV
	    		case 0:
	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		if(!arg1){
	    			printf("L %d : Illegal arg at position 1 of type : %s given to op of type : MOV (expected address or pointer type)\n", line_number, argcodes[arg1]);
	    			err = -1;
	    		}
	    		switch(arg1)
	    		{
	    			case 1:
	    			arg1v = read_bits(map, 3, &seg_ptr, &err);
	    			break;
	    			case 2:
	    			arg1v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    			case 3:
	    			arg1v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    		}

	    		arg2 = read_bits(map, 2, &seg_ptr, &err);
	    		switch(arg2)
	    		{
	    			case 0:
	    			arg2v = read_bits(map, 8, &seg_ptr, &err);
	    			break;
	    			case 1:
	    			arg2v = read_bits(map, 3, &seg_ptr, &err);
	    			break;
	    			case 2:
	    			arg2v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    			case 3:
	    			arg2v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    		}

	    		RAM[RAM_ptr - 4] = curr_op;
	    		RAM[RAM_ptr - 3] = arg1;
	    		RAM[RAM_ptr - 2] = arg1v;
	    		RAM[RAM_ptr - 1] = arg2;
	    		RAM[RAM_ptr] = arg2v;
	    		RAM_ptr-=5;

	    		break;

	    		// CAL
	    		case 1:

	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg1 != 0)
	    		{
	    			printf("L %d : Illegal arg of type %s given to op of type : CAL (expected value type)\n", line_number, argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 8, &seg_ptr, &err);

	    		if(arg1v > 7)
	    		{
	    			printf("L %d : Label of function call exceeds maximum value (7)\n", line_number);
	    			err = -1;
	    		}

	    		RAM[RAM_ptr-2] = curr_op;
	    		RAM[RAM_ptr-1] = arg1;
	    		RAM[RAM_ptr] = arg1v;
	   			RAM_ptr -= 3;

	    		break;

	    		// RET
	    		case 2:
	    		RAM[RAM_ptr] = curr_op;
	    		RAM_ptr-=1;
	    		break;

	    		// REF
	    		case 3:

	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg1 != 2)
	    		{
	    			printf("L %d : Illegal arg at position 1 of type %s given to op of type : REF (expected SYMBL)\n", line_number, argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 5, &seg_ptr, &err);

	    		arg2 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg2 == 0)
	    		{
	    			printf("L %d : Illegal arg at position 2 of type %s given to op of type : REF (expected address or pointer type)\n", line_number, argcodes[arg2]);
	    			err = -1;
	    		}
	    		switch(arg2)
	    		{
	    			case 1:
	    			arg2v = read_bits(map, 3, &seg_ptr, &err);
	    			break;
	    			case 2:
	    			arg2v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    			case 3:
	    			arg2v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    		}

	    		RAM[RAM_ptr - 4] = curr_op;
	    		RAM[RAM_ptr - 3] = arg1;
	    		RAM[RAM_ptr - 2] = arg1v;
	    		RAM[RAM_ptr - 1] = arg2;
	    		RAM[RAM_ptr] = arg2v;
	    		RAM_ptr-=5;

	    		break;

	    		// ADD
	    		case 4:

	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg1 != 1)
	    		{
	    			printf("L %d : Illegal arg at position 1 of type %s given to op of type : ADD (expected register address)\n", line_number, argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 3, &seg_ptr, &err);

	    		arg2 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg2 != 1)
	    		{
	    			printf("L %d : Illegal arg at position 2 of type %s given to op of type : ADD (expected register address)\n", line_number, argcodes[arg2]);
	    			err = -1;
	    		}
	    		arg2v = read_bits(map, 3, &seg_ptr, &err);

	    		RAM[RAM_ptr - 4] = curr_op;
	    		RAM[RAM_ptr - 3] = arg1;
	    		RAM[RAM_ptr - 2] = arg1v;
	    		RAM[RAM_ptr - 1] = arg2;
	    		RAM[RAM_ptr] = arg2v;

	    		RAM_ptr-=5;

	    		break;

	    		// PRINT
	    		case 5:
	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		switch(arg1)
	    		{
	    			case 0:
	    			arg1v = read_bits(map, 8, &seg_ptr, &err);
	    			break;
	    			case 1:
	    			arg1v = read_bits(map, 3, &seg_ptr, &err);
	    			break;
	    			case 2:
	    			arg1v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    			case 3:
	    			arg1v = read_bits(map, 5, &seg_ptr, &err);
	    			break;
	    		}

	    		RAM[RAM_ptr-2] = curr_op;
	    		RAM[RAM_ptr-1] = arg1;
	    		RAM[RAM_ptr] = arg1v;
	    		RAM_ptr -= 3;

	    		break;

	    		// NOT
	    		case 6:

	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg1 != 1)
	    		{
	    			printf("L %d : Illegal arg of type %s given to op of type : NOT (expected register address)", line_number, argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 3, &seg_ptr, &err);

	    		RAM[RAM_ptr-2] = curr_op;
	    		RAM[RAM_ptr-1] = arg1;
	    		RAM[RAM_ptr] = arg1v;

	    		break;

	    		// EQU
	    		case 7:

	    		arg1 = read_bits(map, 3, &seg_ptr, &err);
	    		if(arg1 != 1)
	    		{
	    			printf("L %d : Illegal arg of type %s given to op of type : EQU (expected register address)", line_number, argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 3, &seg_ptr, &err);

	    		RAM[RAM_ptr-2] = curr_op;
	    		RAM[RAM_ptr-1] = arg1;
	    		RAM[RAM_ptr] = arg1v;
	    		RAM_ptr-=3;

	    		break;

	    		default:
	    		break;
	    	}
	    }
    	arg1 = read_bits(map, 3, &seg_ptr, &err);
    	if(!RAM[arg1])
		{
			RAM[arg1] = RAM_ptr + 1;
		}
		else 
		{
			err = -1;
			printf("L %d : Function with label '%d' already encountered. Ensure functions labels are unique\n", line_number, arg1);
			break;
		}
    	arg1 = 0;
	}

	if(!RAM[0])
	{
		err = -1;
		printf("No entry point defined for the program. One function labelled '0' is necessary\n");
	}

	unmap_file(map, fd, res);
	RAM_ptr++;

    if(err < 0)
    {
    	printf("Fatal errors occured during parse. Check log messages for more info.\n");
    	return -1;
    }


    //////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////       LOOKING FOR UNITIALIZED VARIABLES, FUNCTIONS      //////////////////////////
    ///////////////       AND NAMING STACK SYMBOLS PROPERLY                 //////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////

   	printf("Performing semantic checks...\n");

    unsigned char symbol_counter = 0;

    // CODE SEGMENT , STACK SEGMENT, ADDRESS SEGMENT
    unsigned char CS = RAM_ptr, SS = 8, RS = 0;

    err = 0;
    curr_op = 0;
    arg1 = 0;
    arg2 = 0;
    arg1v = 0;
    arg2v = 0;

    unsigned char stack_symbol_buffer[1<<5];

    for(int i = 0; i<8; ++i)
    {
    	if(!RAM[i])
    	{
    		continue;
    	}
    	RAM_ptr = RAM[i];

    	// resetting stack symbol buffer
    	for(int i = 0; i<32; ++i)
    	{
    		stack_symbol_buffer[i] = NO_SYMBOL;
    	}

    	while(1)
    	{
	    	switch(RAM[RAM_ptr])
	    	{
	    		// MOV
	    		case 0:

	    		curr_op = RAM[RAM_ptr];
	    		arg1 = RAM[RAM_ptr+1];
	    		arg1v = RAM[RAM_ptr+2];
	    		arg2 = RAM[RAM_ptr+3];
	    		arg2v = RAM[RAM_ptr+4];

	    		// CHECKS
	    		// 1. Source should be intialized if it is a symbol
	    		if((arg2 == SYMBL || arg2 == PNTR) && stack_symbol_buffer[arg2v] == NO_SYMBOL)
	    		{
	    			err = -1;
	    			printf("Attempt to provide uninitialized SYMBL as source in MOV\n");
	    		}
	    		else if((arg2 == SYMBL || arg2 == PNTR) && stack_symbol_buffer[arg2v] != NO_SYMBOL)
	    		{
	    			RAM[RAM_ptr+4] = stack_symbol_buffer[arg2v];
	    		}

	    		if((arg1 == SYMBL || arg1 == PNTR) && stack_symbol_buffer[arg1v] == NO_SYMBOL)
	    		{
	    			stack_symbol_buffer[arg1v] = symbol_counter;
	    			RAM[RAM_ptr+2] = symbol_counter;
	    			symbol_counter++;
	    		}
	    		else if((arg1 == SYMBL || arg1 == PNTR) && stack_symbol_buffer[arg1v] != NO_SYMBOL)
	    		{
	    			RAM[RAM_ptr+2] = stack_symbol_buffer[arg1v];
	    		}

	    		RAM_ptr+=5;
	    		break;

	    		// CAL
	    		case 1:

	    		curr_op = RAM[RAM_ptr];
	    		arg1 = RAM[RAM_ptr+1];
	    		arg1v = RAM[RAM_ptr+2];

	    		// CHECKS
	    		// 1. Function label should exist in memory
	    		if(!RAM[arg1v])
	    		{
	    			err = -1;
	    			printf("Attempt to invoke CAL on a non-existent function label (null function pointer)\n");
	    		}

	    		RAM_ptr+=3;
	    		break;

	    		// RET
	    		case 2:
	    		// Nothing to check
	    		break;

	    		// REF
	    		case 3:

	    		curr_op = RAM[RAM_ptr];
	    		arg1 = RAM[RAM_ptr+1];
	    		arg1v = RAM[RAM_ptr+2];
	    		arg2 = RAM[RAM_ptr+3];
	    		arg2v = RAM[RAM_ptr+4];

	    		// CHECKS
	    		// 1. Source stack symbol should be initialized
	    		if(arg2 == SYMBL && stack_symbol_buffer[arg2v] == NO_SYMBOL)
	    		{
	    			err = -1;
	    			printf("Attempt to extract reference to uninitialized SYMBL in REF\n");
	    		}
	    		else if(arg2 == SYMBL && stack_symbol_buffer[arg2v] != NO_SYMBOL)
	    		{
	    			RAM[RAM_ptr+4] = stack_symbol_buffer[arg2v];
	    		}

	    		if((arg1 == SYMBL || arg1 == PNTR) && stack_symbol_buffer[arg1v] == NO_SYMBOL)
	    		{
	    			stack_symbol_buffer[arg1v] = symbol_counter;
	    			RAM[RAM_ptr+2] = symbol_counter;
	    			symbol_counter++;
	    		}
	    		else if((arg1 == SYMBL || arg1 == PNTR) && stack_symbol_buffer[arg1v] != NO_SYMBOL)
	    		{
	    			RAM[RAM_ptr+2] = stack_symbol_buffer[arg1v];
	    		}

	    		RAM_ptr += 5;
	    		break;

	    		// ADD
	    		case 4:
	    		// Nothing to check
	    		RAM_ptr += 5;
	    		break;

	    		// PRINT
	    		case 5:
	    		// Nothing to check
	    		RAM_ptr += 3;
	    		break;

	    		// NOT
	    		case 6:
	    		// Nothing to check
	    		RAM_ptr += 3;
	    		break;

	    		// EQU
	    		case 7:
	    		// Nothing to check
	    		RAM_ptr += 3;
	    		break;
	    	}
	    	// RET
    		if(RAM[RAM_ptr] == 2)
    		{
    			break;
    		}
	    }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////    DISPLAYING THE MACHINE CODE AS ASSEMBLY CODE    ///////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////

    printf("Disassembling machine code...\nDisassembled code:\n");
    int i = 0;
    for(int j = 0; j<8; ++j)
    {
    	i = RAM[j];
    	if(!i)
    	{
    		continue;
    	}
    	if(j == 0)
    	{
    		printf("\nENTRY POINT (main)\n");
    	}
    	else 
    	{
    		printf("\n");
    	}
    	printf("FUNC %d at %d\n", j, i);

	    while(1){
	    	print_instruction(RAM[i]);
	    	if(RAM[i] == 2)
	    	{
	    		printf("\n");
	    		break;
	    	}
	    	switch(RAM[i]){
	    		// MOV
	    		case 0:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	print_args(RAM[i+3], RAM[i+4]);
		    	printf("\n");    		
		    	i += 5;
	    		break; 

	    		// CAL
	    		case 1:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	printf("\n");
		    	i += 3;
	    		break;

	    		// RET
	    		case 2:
	    		break;

	    		// REF
	    		case 3:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	print_args(RAM[i+3], RAM[i+4]);
		    	printf("\n");
		    	i += 5;
	    		break;

	    		// ADD
	    		case 4:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	print_args(RAM[i+3], RAM[i+4]);
		    	printf("\n");    		
		    	i += 5;
	    		break; 

	    		// PRINT
	    		case 5:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	printf("\n");
		    	i += 3;
	    		break;

	    		// NOT
	    		case 6:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	printf("\n");
		    	i += 3;
	    		break;

	    		// NOT
	    		case 7:
		    	print_args(RAM[i+1], RAM[i+2]);
		    	printf("\n");
		    	i += 3;
	    		break;
	    	}
	    }
	}


    /////////////////////////////////////////////////////////////////////////////////////////////
    /////////////// OPTIMIZING THE MACHINE CODE TO USE LESS STACK SPACE /////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////


    /*

		COMPLICATED AND PROBABLY UNNECESSARY. KEEP IT AS A PROJECT WITH A 
		MORE ADVANCED INSTRUCTION SET FOR LATER MAYBE?

    */



    /////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////    RUNNING THE MACHINE CODE ON A VIRTUAL MACHINE    /////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////

    printf("\nInitializing virtual environment...\n");

    // initializing registers
    // 0x07 : instruction pointer
    // 0x06 : current stack size
    // 0x05 : current stack pointer
    unsigned char reg_bank[8] = {0, 0, 0, 0, 0, SS, 0, RAM[0]};
    curr_op = 0;
    arg1 = 0;
    arg2 = 0;
    arg1v = 0;
    arg2v = 0;

    printf("Virtual machine running\n\n");

    int terminated = 0;
    while(!terminated)
    {
    	curr_op = RAM[reg_bank[PC]];
    	if(reg_bank[SP] + reg_bank[SZ] >= CS)
    	{
    		terminated = -1;
    		break;
    	}
    	switch(curr_op)
    	{
    		case MOV:
    		printf("MOVING\n");
    		arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		arg2 = RAM[reg_bank[PC]+3];
    		arg2v = RAM[reg_bank[PC]+4];
    		write_val(RAM, reg_bank[SP], reg_bank, arg1, arg1v, read_val(RAM, reg_bank[SP], reg_bank, arg2, arg2v));
    		reg_bank[PC] += 5;
    		break;

    		case CAL:
    		printf("CALLING\n");
    		arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		reg_bank[PC] += 3;
    		RAM[reg_bank[SP] + reg_bank[SZ]] = reg_bank[SZ];
    		RAM[reg_bank[SP] + reg_bank[SZ] + 1] = reg_bank[PC];
    		reg_bank[PC] = RAM[arg1v];
    		reg_bank[SP] += reg_bank[SZ] + 2;
    		break;

    		case RET:
    		printf("RETURNING\n");
    		if(reg_bank[SP] == 8)
    		{
    			terminated = 1;
    			break;
    		}
    		reg_bank[SZ] = RAM[reg_bank[SP] - 2];
    		reg_bank[PC] = RAM[reg_bank[SP] - 1];
    		reg_bank[SP] = reg_bank[SP] - 2 - RAM[reg_bank[SP] - 2];
    		break;

    		case REF:
    		printf("CREATING REFERENCE\n");
    		arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		arg2 = RAM[reg_bank[PC]+3];
    		arg2v = RAM[reg_bank[PC]+4];
    		RAM[reg_bank[SP] + arg1v] = reg_bank[SP] + arg2v;
    		reg_bank[PC] += 5;
    		break;

    		case ADD:
    		printf("ADDING\n");
			arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		arg2 = RAM[reg_bank[PC]+3];
    		arg2v = RAM[reg_bank[PC]+4];
    		reg_bank[arg1v] += reg_bank[arg2v];
    		printf("RESULT : %d\n", reg_bank[arg1v]);
    		reg_bank[PC] += 5;
    		break;

    		case PRINT:
    		arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		printf("STDOUT : %d\n", read_val(RAM, reg_bank[SP], reg_bank, arg1, arg1v));
    		break;

    		case NOT:
    		printf("NEGATING\n");
    		arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		reg_bank[arg1v] = ~reg_bank[arg1v];
    		reg_bank[PC] += 3;
    		break;

    		case EQU:
    		printf("COMPARING NULLITY\n");
    		arg1 = RAM[reg_bank[PC]+1];
    		arg1v = RAM[reg_bank[PC]+2];
    		reg_bank[arg1v] = !reg_bank[arg1v];
    		reg_bank[PC] += 3;
    		break;

    	}
    }
    printf("\nVirtual machine terminated. Exit code : %d (%s)\n", terminated, exit_codes[terminated+2]);

	return 0;
}
