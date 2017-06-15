/**
 * Basic soft processor with 16-bit registers that addresses 1 MiB of memory. Current version
 * takes 3 command line arguments (the instructions) as follows: ./softcpu command dest src. 
 * For all commands, dest is always the destination for a value, and src is the source of that value.
 * 
 * Registers accessible by outside processes: A and B
 * Format of memory address arguments: Hexidecimal, including the hex prefix 0x.
 * 
 * Command instructions (within quotes) and usage below:
 *  "r": Raw set. Takes an integer given in src and assigns it to a register (A or B) given in dest.
 *  "m": Move between registers. Takes the value contained within the register given in src and moves it to the 
 *       register given in dest. The value remains in the src register unchanged.
 *  "l": Load to register. Gets the value at the memory address given in src and loads it into the register
 *       given in dest. 
 *  "s": Store from register. Takes the value in the register given in src and stores it in the memory address given 
 *       in dest.
 * 
 * Processor syncs RAM memory to disk following one operation.
 * 
**/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <err.h>
#include <sysexits.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>

int ftruncate(int fd, off_t length);

// make argv global
char** gargv;

// main memory size
#define ram_bytes 1048576

// define register type with either full 16 bits or high and low 8 bits
typedef union reg {
    struct __attribute__((packed)) {
        uint8_t low;
        uint8_t high;
    } high_low;
    uint16_t full;
} reg_t;

// define general registers
struct general {
    reg_t A;                                        // general register
    reg_t B;                                        // general register
    reg_t I1;                                       // first instruction arg (command)
    reg_t I2;                                       // second instruction arg unless address
    reg_t I3;                                       // third instruction arg unless address
} reg_gen;

// define registers for address handling 
struct address {
    reg_t D1;                                        // first 16 bits of mem address
    reg_t D2;                                        // low bits of a mem address greater than 16 bits
} reg_addr;

// move value from one register to another (copying value)
void reg_move(struct general* regs_move)
{
    // load rest of instruction into intruction regs
    regs_move->I2.high_low.low = *gargv[2];
    regs_move->I3.high_low.low = *gargv[3];
    
    // copy value from reg B into A
    if (regs_move->I2.high_low.low == 'A' && regs_move->I3.high_low.low == 'B')
    {
        regs_move->A.full = regs_move->B.full;
    }
    
    // copy value from reg A into B
    else if (regs_move->I2.high_low.low == 'B' && regs_move->I3.high_low.low == 'A')
    {
        regs_move->B.full = regs_move->A.full;
    }
    
    else
    {
        errx(EX_USAGE, "Invalid instructions");
    }
}

// set register value to a raw value given by instruction
void raw_set(struct general* reg_set)
{
    reg_set->I2.full = *gargv[2];
    sscanf (gargv[3], "%hi" SCNu16, &reg_set->I3.full);
    
    if (reg_set->I2.high_low.low == 'A')
    {
        reg_set->A.full = reg_set->I3.full;
    }
    
    else if (reg_set->I2.high_low.low == 'B')
    {
        reg_set->B.full = reg_set->I3.full;
    }
    
    else
    {
        errx(EX_USAGE, "Invalid instrcutions");
    }
}

// load value into a register from a memory address
void load_to_reg(struct general* regs_load, struct address* regs_addr, uint16_t* ram_mem) 
{
    regs_load->I2.high_low.low = *gargv[2];
    
    // allows bitshift for concatenating mem addresses greater than 16 bits
    int shift = strlen(gargv[3]) - 6;
    if (shift < 0)
    {
        shift = 0;
    }
    
    // load highest 16 bits into memory reg
    sscanf(&gargv[3][2], "%4hX", &regs_addr->D1.full);
    
    // load low bits of addresses greater than 16 bits  
    if(gargv[3][6])
    {
        sscanf(&gargv[3][6], "%2hhX", &regs_addr->D2.high_low.low);
    }
    
    if (regs_load->I2.high_low.low == 'A')
    {
        // index into mapped RAM with concatenated address
        regs_load->A.full = ram_mem[ (regs_addr->D1.full << (shift * 4) ) |
                                      regs_addr->D2.high_low.low ];
    }
    
    else if (regs_load->I2.high_low.low == 'B')
    {
        regs_load->B.full = ram_mem[ (regs_addr->D1.full << (shift * 4) ) |
                                      regs_addr->D2.high_low.low ];
    }
    
    else
    {
        errx(EX_USAGE, "Invalid instructions");
    }
} 

// store a value from a register into a given memory address
void store_from_reg(struct general* regs_store, struct address* regs_addr, uint16_t* ram_mem) 
{
    regs_store->I3.high_low.low = gargv[3][0];
    
    int shift = strlen(gargv[2]) - 6;
    if (shift < 0)
    {
        shift = 0;
    }
    
    sscanf(&gargv[2][2], "%4hX", &regs_addr->D1.full);
    
    if(gargv[2][6])
    {
        sscanf(&gargv[2][6], "%2hhX", &regs_addr->D2.high_low.low);
    }
    
    if (regs_store->I3.high_low.low == 'A')
    {
        ram_mem[ (regs_addr->D1.full << (shift * 4) ) | regs_addr->D2.high_low.low ]
            = regs_store->A.full;
    }
    
    else if (regs_store->I3.high_low.low == 'B')
    {
        ram_mem[ (regs_addr->D1.full << (shift * 4) ) | regs_addr->D2.high_low.low ]
            = regs_store->B.full;
    }
    
    else
    {
        errx(EX_USAGE, "Invalid instructions");
    }
}

int main(int argc, char** argv)
{
    gargv = argv;
    if (argc != 4)
    {
        errx(EX_USAGE, "Usage: ./softcpu command dest src");
    }
    
    // open ram file or create one if not already existing and mmap RAM
    int ramD = open("ramMap.txt", O_RDWR | O_CREAT | O_EXCL, 0666);
    if (ramD == -1 || errno == EEXIST)
    {
        ramD = open("ramMap.txt", O_RDWR, 0666);
        if(ramD == -1)
        {
            errx(EX_OSERR, "RAM could not be initialized");
        }
    }
    
    // initialize file
    ftruncate(ramD, ram_bytes);
    
    // mmap file
    uint16_t* ram = mmap(0, ram_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ramD, 0);
    if (ram == (void *)-1)
    {
        errx(EX_OSERR, "RAM could not be initialized");
    }
    
    ram[0x1234] = 64123;
    
    // load first instruction
    reg_gen.I1.full = *argv[1];
    
    // instruction parsing
    switch (reg_gen.I1.full) {
        case 'r':
            raw_set(&reg_gen);
            break;
        case 'm':
            reg_move(&reg_gen);
            break;
        case 'l':
            load_to_reg(&reg_gen, &reg_addr, ram);
            break;
        case 's':
            store_from_reg(&reg_gen, &reg_addr, ram);
            break;
        default:
            errx(EX_USAGE, "Invalid instructions");
            break;
    }
    
    // flush to disk
    int diskSync = msync(ram, ram_bytes, MS_SYNC);
    if (diskSync != 0)
    {
        errx(EX_NOINPUT, "System state could not be synced to disk");
    }
    
    // unmap memory and close RAM file
    munmap(ram, ram_bytes);
    close(ramD);
}
