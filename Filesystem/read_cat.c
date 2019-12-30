/*********** read_cat.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256], pathname2[256];

int read_file()
{
    int fd = atoi(pathname);
    int nbytes = atoi(pathname2);
    char buf[nbytes + 1];

    //check if OFT entry is valid and is either R|RW = 0|2
    if (running->fd[fd] == 0 || (running->fd[fd]->mode != 0 && running->fd[fd]->mode != 2)){
        printf("file %s is not opened for R or RW mode", pathname);
        return -1;
    }
    return (myread(fd, buf, nbytes));
}

int myread(int fd, char buf[], int nbytes)
{
    MINODE *mip = running->fd[fd]->mptr;
    OFT *oftp = running->fd[fd];
    int count = 0;
    int avil = mip->INODE.i_size - oftp->offset;          // number of bytes still available in file.
    char *cq = buf;                               // cq points at buf[ ]
    int blk;
    char ibuf[256];

    while (nbytes > 0 && avil > 0){

        //Compute LOGICAL BLOCK number lbk and startByte in that block from offset;

        int lbk = oftp->offset / BLKSIZE;            //current byte position within logical block
        int startByte = oftp->offset % BLKSIZE;      //start byte for reading

        // I only show how to read DIRECT BLOCKS. YOU do INDIRECT and D_INDIRECT
        
        if (lbk < 12){                     // lbk is a direct block
            blk = mip->INODE.i_block[lbk]; // map LOGICAL lbk to PHYSICAL blk
        }
        else if (lbk >= 12 && lbk < 268){ // 256+12 
            //  indirect blocks 
            get_block(mip->dev, mip->INODE.i_block[12], ibuf); //indirect blocks located in 12th iblock
            blk = ibuf[lbk-12];   //subtract the 12 from first if statement
        }
        else{ 
            //  double indirect blocks mailman's algorithm
            get_block(mip->dev, mip->INODE.i_block[13] , ibuf); //get to indirect blocks from double indirect
            get_block(mip->dev, ibuf[(lbk-268) / 256] , ibuf); //get to the indirect block
            blk = ibuf[(lbk-268) % 256]; //give the byte address
        } 

        /* get the data block into readbuf[BLKSIZE] */
        char readbuf[BLKSIZE];
        get_block(mip->dev, blk, readbuf);

        /* copy from startByte to buf[ ], at most remain bytes in this block */
        char *cp = readbuf + startByte;      // cp points at the start byte in readbuf
        int remain = BLKSIZE - startByte;   // number of bytes remain in readbuf[]

        if(nbytes < remain)
            remain = nbytes;
        
        memcpy(cq, cp, remain);
        oftp->offset += remain;
        count += remain;
        avil -= remain;
        nbytes -= remain;
        /* while (remain > 0){
            *cq++ = *cp++;             // copy byte from readbuf[] into buf[]
             oftp->offset++;           // advance offset 
             count++;                  // inc count as number of bytes read
             avil--; nbytes--;  remain--;
             if (nbytes <= 0 || avil <= 0) 
                 break;
       } */
 
       // if one data block is not enough, loop back to OUTER while for more ...

   }
   //printf("myread: read %d char from file descriptor %d\n", count, fd);  
   return count;   // count is the actual number of bytes read

}

void cat()
{
    char mybuf[1024];
    int n, i;

    //make sure file is open for read
    strcpy(pathname2, "0");
    int fd = open_file();

    

    while (n = myread(fd, mybuf, 1024)){
        
        //buffer terminated by null
        mybuf[n] = 0;

        i = 0;
        while(mybuf[i]){
            putchar(mybuf[i]);
            if (mybuf[i] == '\n')     // check for '\n'
                putchar('\r');
            i++;
        }
    }
    close_file(fd);
}

