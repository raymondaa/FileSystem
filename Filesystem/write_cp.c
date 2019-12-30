/*********** write_cp.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256], pathname2[256];

int write_file()
{
    int fd = atoi(pathname);

    //check if file is open
    if (running->fd[fd]->refCount == 0){
        printf("The file is not opened.\n");
        return -1;
    }

    //check if mode is in read mode (0)
    if (running->fd[fd]->mode == 0){
        printf("File is not open for WR, RW, or APPEND mode.\n");
        return -1;
    }

    mywrite(fd, pathname2, strlen(pathname2));
}

int mywrite(int fd, char buf[], int nbytes)
{
    MINODE *mip = running->fd[fd]->mptr;
    OFT *oftp = running->fd[fd];
    char *cq = buf;


    while (nbytes > 0 ){

     //compute LOGICAL BLOCK (lbk) and the startByte in that lbk:

          int lbk       = oftp->offset / BLKSIZE;
          int startByte = oftp->offset % BLKSIZE;
          int blk;

    // I only show how to write DIRECT data blocks, you figure out how to 
    // write indirect and double-indirect blocks.

     if (lbk < 12){                         // direct block
        if (mip->INODE.i_block[lbk] == 0){   // if no data block yet

           mip->INODE.i_block[lbk] = balloc(mip->dev);// MUST ALLOCATE a block
        }
        blk = mip->INODE.i_block[lbk];      // blk should be a disk block now
     }
     else if (lbk >= 12 && lbk < 268){ // INDIRECT blocks 
              // HELP INFO:
              if (mip->INODE.i_block[12] == 0){
                  mip->INODE.i_block[12] = balloc(mip->dev);        //allocate block
                  zero_block(mip->dev, mip->INODE.i_block[12]);   //zero out the block on disk !!!!
              }
              int ibuf[256];                             //get i_block[12] into an int ibuf[256];
              get_block(mip->dev, mip->INODE.i_block[12], (char*)ibuf);
              blk = ibuf[lbk - 12];
              if (blk==0){
                 balloc(mip->dev);           //allocate a disk block;
                 put_block(mip->dev, mip->INODE.i_block[12], (char*)ibuf);  //record it in i_block[12];
              }
     }
     else{
            // double indirect blocks */

            //mailman's algorithm
            int block = (lbk - 268) / 256;   //268 = 256 + 12
            int offset = (lbk -268) % 256;
            int ibuf[256];
            if (mip->INODE.i_block[13] == 0){
                mip->INODE.i_block[13] = balloc(mip->dev);  //allocate block
                zero_block(mip->dev, mip->INODE.i_block[13]); //zero out the block
            }
            get_block(mip->dev, mip->INODE.i_block[13], (char*)ibuf); // first indirection
            
            if(ibuf[block] == 0){
                ibuf[block] = balloc(mip->dev);      //allocate a block for block
                zero_block(mip->dev, ibuf[block]);
                put_block(mip->dev, mip->INODE.i_block[13], (char*)ibuf);
            }
            int ibuf2[256];
            get_block(mip->dev, ibuf[block], (char*)ibuf2);   //second indirection
            if(ibuf2[offset] == 0){
                ibuf2[offset] = balloc(mip->dev);            //allocate a block for the offset
                zero_block(mip->dev, ibuf2[offset]);
                put_block(mip->dev, ibuf[block], (char*)ibuf2);
            }
            blk = ibuf2[offset]; //found the physical block for the double indirect blocks
     }

     /* all cases come to here : write to the data block */
     char wbuf[BLKSIZE];
     zero_block(mip->dev, blk);
     get_block(mip->dev, blk, wbuf);   // read disk block into wbuf[ ]  
     char *cp = wbuf + startByte;      // cp points at startByte in wbuf[]
     int remain = BLKSIZE - startByte;     // number of BYTEs remain in this block
     
     if(nbytes < remain)
        remain = nbytes;
     memcpy(cp, cq, remain);
     cq += remain;
     oftp->offset += remain;
     nbytes -= remain;
     mip->INODE.i_size += remain;
     put_block(mip->dev, blk, wbuf);

     /* while (remain > 0){               // write as much as remain allows  
           *cp++ = *cq++;              // cq points at buf[ ]
           nbytes--; remain--;         // dec counts
           oftp->offset++;             // advance offset
           if (offset > INODE.i_size)  // especially for RW|APPEND mode
               mip->INODE.i_size++;    // inc file size (if offset > fileSize)
           if (nbytes <= 0) break;     // if already nbytes, break
     }
     put_block(mip->dev, blk, wbuf);   // write wbuf[ ] to disk
     
     // loop back to outer while to write more .... until nbytes are written */
  }

  mip->dirty = 1;       // mark mip dirty for iput() 
  //printf("wrote %d char into file descriptor fd=%d\n", nbytes, fd);           
  return nbytes;
}

int cp()
{
    int n;
    char buf[1024];
    char temp1[256];
    char temp2[256];
    strcpy(temp1, pathname);
    strcpy(temp2, pathname2);
    strcpy(pathname2, "0");
    int fd = open_file();
    strcpy(pathname, temp2);
    strcpy(pathname2, "1");
    int gd = open_file();

    while(n = myread(fd, buf, BLKSIZE)){
        mywrite(gd, buf, n);
    }
    close_file(fd);
    close_file(gd);
}