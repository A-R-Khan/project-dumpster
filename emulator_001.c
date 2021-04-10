#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#define NO_FUNCTION 1<<4
#define NO_SYMBOL 1<<6

typedef uint8_t byte;

const char *opcodes[8] = {
	[0] = "MOV  ", [1] = "CAL  ", [2] = "RET  ", [3] = "REF  ",
	[4] = "ADD  ", [5] = "PRINT", [6] = "NOT  ", [7] = "EQU  ",
};

const char *argcodes[4] = {
	[0] = "CONST", [1] = "RGSTR", [2] = "SYMBL", [3] = "PNTR",
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
        exit(EXIT_FAILURE);
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

int print_instruction(unsigned char opcode){
	if(opcode > 7){
		return 0;
	}
	printf("%s ", opcodes[opcode]);
	return 1;
}

unsigned char read_bits(unsigned char *map, int rd_sz, int *pos, int *err)
{
	if(*pos < rd_sz-1 || rd_sz > 8) {
		*err = -1;
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

int main(int argc, char const *argv[])
{
	unsigned char RAM[1<<8];
	for(int i =0; i<8; ++i)
	{
		RAM[i] = NO_FUNCTION;
	}
	for(int i = 8; i<(1<<8); ++i) 
	{
		RAM[i] = 0;
	}

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
		printf("Error opening the file\n");
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

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////   PARSING THE ASSEMBLY CODE AND LOADING IT TO VIRTUAL RAM   ////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////
    
    int func_size = 0;
    int seg_end = res*8 - 1;
    int seg_ptr = seg_end;
    int RAM_ptr = (1<<8) -1;
    int err;

    while(err!=-1)
    {
	    func_size = read_bits(map, 5, &seg_ptr, &err);
		if(func_size <= 0)
		{
			break;
		}

	    unsigned char curr_op, arg1, arg2, arg1v, arg2v;
	    for(int i = 0; i < func_size; ++i)
	    {
	    	curr_op = read_bits(map, 3, &seg_ptr, &err);
	    	switch(curr_op)
	    	{
	    		// MOV
	    		case 0:
	    		arg1 = read_bits(map, 2, &seg_ptr, &err);
	    		if(!arg1){
	    			printf("Illegal arg at position 1 of type : %s given to op of type : MOV (expected address or pointer type)\n", argcodes[arg1]);
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
	    			printf("Illegal arg of type %s given to op of type : CAL (expected value type)\n", argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 8, &seg_ptr, &err);

	    		if(arg1v > 7)
	    		{
	    			printf("Label of function call exceeds maximum value (7)\n");
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
	    			printf("Illegal arg at position 1 of type %s given to op of type : REF (expected SYMBL)\n", argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 5, &seg_ptr, &err);

	    		arg2 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg2 == 0)
	    		{
	    			printf("Illegal arg at position 2 of type %s given to op of type : REF (expected address or pointer type)\n", argcodes[arg2]);
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
	    			printf("Illegal arg at position 1 of type %s given to op of type : ADD (expected register address)\n", argcodes[arg1]);
	    			err = -1;
	    		}
	    		arg1v = read_bits(map, 3, &seg_ptr, &err);

	    		arg2 = read_bits(map, 2, &seg_ptr, &err);
	    		if(arg2 != 1)
	    		{
	    			printf("Illegal arg at position 2 of type %s given to op of type : ADD (expected register address)\n", argcodes[arg2]);
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
	    			printf("Illegal arg of type %s given to op of type : NOT (expected register address)", argcodes[arg1]);
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
	    			printf("Illegal arg of type %s given to op of type : EQU (expected register address)", argcodes[arg1]);
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
	    if(err == -1)
	    {
	    	break;
	    }
    	arg1 = read_bits(map, 3, &seg_ptr, &err);
    	RAM[arg1] = RAM_ptr + 1;
    	arg1 = 0;
	}


	unmap_file(map, fd, res);
	RAM_ptr++;

    if(err == -1)
    {
    	printf("Fatal errors occured during parse. Check log messages for more info.\n");
    	return -1;
    }


    //////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////       LOOKING FOR UNITIALIZED VARIABLES, FUNCTIONS      //////////////////////////
    ///////////////       AND ALSO NAMING STACK SYMBOLS PROPERLY            //////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////


    unsigned char stack_symbol_buff[32];
    for(int i = 0; i<32; ++i)
    {
    	stack_symbol_buff[i] = NO_SYMBOL;
    }

    unsigned char function_label_buff[8];
    for(int i = 0; i<8; ++i)
    {
    	stack_symbol_buff[i] = NO_FUNCTION;
    }

    unsigned char func_counter = 0, symbol_counter = 0;
    unsigned char CS = RAM_ptr, SS = 8, RS = 0;

    while(RAM_ptr<(1<<8)) 
    {
    	switch(RAM[RAM_ptr])
    	{
    		// MOV
    		case 0:
    		break;

    		// CAL
    		case 1:
    		break;

    		// RET
    		case 2:
    		func_counter++;
    		for(int i = 0; i<32; ++i) 
    		{
    			stack_symbol_buff[i] = NO_SYMBOL;
    		}
    		break;

    		// REF
    		case 3:
    		break;

    		// ADD
    		case 4:
    		break;

    		// PRINT
    		case 5:
    		break;

    		// NOT
    		case 6:
    		break;
    		case 7:

    		// EQU
    		break;
    	}
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////    DISPLAYING THE MACHINE CODE AS ASSEMBLY CODE    ///////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////

    int i = CS;
    while(i<(1<<8)){
    	switch(RAM[i]){
    		case 0:
    		printf("%s %s %d %s %d\n", opcodes[0], argcodes[RAM[i+1]], RAM[i+2], argcodes[RAM[i+3]], RAM[i+4]);
    		i += 5;
    		break;
    		case 1:
    		printf("%s %s %d\n", opcodes[1], argcodes[RAM[i+1]], RAM[i+2]);
    		i += 3;
    		break;
    		case 2:
    		printf("%s\n", opcodes[2]);
    		i++;
    		break;
    		case 3:
	    	printf("%s %s %d %s %d\n", opcodes[3], argcodes[RAM[i+1]], RAM[i+2], argcodes[RAM[i+3]], RAM[i+4]);
    		break;
    		case 4:
    		printf("%s %s %d %s %d\n", opcodes[4], argcodes[RAM[i+1]], RAM[i+2], argcodes[RAM[i+3]], RAM[i+4]);
    		i += 5;
    		break;
    		case 6:
      		printf("%s %s %d\n", opcodes[6], argcodes[RAM[i+1]], RAM[i+2]);
      		i += 3;
    		break;
    		case 7:
    		printf("%s %s %d\n", opcodes[7], argcodes[RAM[i+1]], RAM[i+2]);
    		i += 3;
    		break;
    	}
    }


    /////////////////////////////////////////////////////////////////////////////////////////////
    /////////////// OPTIMIZING THE MACHINE CODE TO USE LESS STACK SPACE /////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////


    /*

		OPTIONAL AND PROBABLY UNNECESSARY. KEEP IT AS A PROJECT WITH A 
		MORE ADVANCED LANGUAGE FOR LATER MAYBE?

    */



    /////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////    RUNNING THE MACHINE CODE ON A VIRTUAL MACHINE    /////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////




	return 0;
}