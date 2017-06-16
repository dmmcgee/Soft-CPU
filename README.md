# soft_cpu
Soft processor in C

Basic soft processor with 16-bit registers that addresses 1 MiB of memory. Current version takes 3 command line arguments (the instructions) as follows: `./softcpu command dest src.`

For all commands, dest is always the destination for a value, and src is the source of that value.

Registers accessible by outside processes: `A` and `B`
Format of memory address arguments: Hexidecimal, including the hex prefix `0x`.

Command instructions and usage below:
 *  `r`: Raw set. Takes an integer given in src and assigns it to a register (A or B) given in dest.
 *  `m`: Move between registers. Takes the value contained within the register given in src and moves it to the register given in dest. The value remains in the src register unchanged.
 *  `l`: Load to register. Gets the value at the memory address given in src and loads it into the register given in dest. 
 *  `s`: Store from register. Takes the value in the register given in src and stores it in the memory address given in dest.

Processor syncs RAM memory to disk following one operation.

