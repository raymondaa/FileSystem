/*********** util.c file ****************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

//reads a disk block into a buf
int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

//writes a disk block into a buf
int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}   

//tokenizes the pathname into component strings
int tokenize(char *pathname)
{
  // tokenize pathname in GLOBAL gpath[]; pointer by name[i]; n tokens
  char *token;
  int i = 0;
  strcpy(gpath, pathname);
  for(token = strtok(gpath, "/"), n = 0; token; token = strtok(NULL, "/"), i++, n++)
  {
    name[i] = (char *)malloc(64 *sizeof(char));
    strcpy(name[i], token);
  }
  name[i] = NULL;
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, disp;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];

    if (mip->refCount && mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
  
  // a new MINODE must be allocated
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
      //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino to buf    
       blk  = (ino-1) / 8 + inode_start;
       disp = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d disp=%d\n", ino, blk, disp);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + disp;
       // copy INODE to mp->INODE
       mip->INODE = *ip;

       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

// Releases the MINODE pointed to by mip makes refCount=0
int iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return;

 mip->refCount--;
 
 if (mip->refCount > 0) return;
 if (!mip->dirty)       return;
 
 /* write back */
 //printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 

 block =  ((mip->ino - 1) / 8) + inode_start;
 offset =  (mip->ino - 1) % 8;

 /* first get the block containing this inode */
 get_block(mip->dev, block, buf);

 ip = (INODE *)buf + offset;
 *ip = mip->INODE;

 put_block(mip->dev, block, buf);

} 

//search for the tokenized strings within successive directories
int search(MINODE *mip, char *name)
{
  printf("i_number         rec_len       name_len        name\n");
    char sbuf[BLKSIZE], temp[256];
    int i;
    DIR* dp;
    char* cp;
    for(i = 0; i < 12; i++)
    {
        if(mip->INODE.i_block[i] == 0)
            return 0;
        get_block(mip->dev, mip->INODE.i_block[i], sbuf);
        dp = (DIR*)sbuf;
        cp = sbuf;


        while(cp < sbuf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%8d       %8d      %8d          %s\n",
                dp->inode, dp->rec_len, dp->name_len, temp);
            if(strcmp(name, temp) == 0){
                printf("Found: %s: inumber = %d\n", name, dp->inode);
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (DIR*)cp;
        }
    }
    
    return 0;
}

// returns the inumber of INODE from the specified pathname
int getino(char *pathname)
{
  int i, ino, blk, disp;
  INODE *ip;
  MINODE *mip;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return 2;

  if (pathname[0]=='/')
    mip = iget(dev, 2);
  else
    mip = iget(running->cwd->dev, running->cwd->ino);
  tokenize(pathname);

  for (i=0; i<n; i++){
      printf("===========================================\n");
      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);
      mip = iget(dev, ino);
   }
   return ino;
}


int tst_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  if (buf[i] & (1 << j))
     return 1;
  return 0;
}

int set_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] |= (1 << j);
}

int clr_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] &= ~(1 << j);
}


int decFreeInodes(int dev)
{
  char buf[BLKSIZE];
  // dec free inodes count by 1 in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buf);
}

int ialloc(int dev)  // allocate an inode number
{
  int  i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       put_block(dev, imap, buf);
       decFreeInodes(dev);
       return i+1;
    }
  }
  return 0;
}

// WRITE YOUR OWN int balloc(dev) function, which returns a FREE disk block number
int balloc(int dev)  // allocate an block number
{
  int  i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, bmap, buf);

  for (i=0; i < nblocks; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       put_block(dev, bmap, buf);
       //decFreeInodes(dev);
       return i+1;
    }
  }
  return 0;
}

int incFreeInodes(int dev)
{
  char buf[BLKSIZE];

  // inc free inodes count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

idalloc(int dev, int ino)  // deallocate an ino number
{
  int i;  
  char buf[BLKSIZE];

  if (ino > ninodes){
    printf("inumber %d out of range\n", ino);
    return;
  }

  // get inode bitmap block
  get_block(dev, imap, buf);
  clr_bit(buf, ino-1);

  // write buf back
  put_block(dev, imap, buf);

  // update free inode count in SUPER and GD
  incFreeInodes(dev);
}

int bdalloc(int dev, int blk) // deallocate a blk number
{
  int i;  
  char buf[BLKSIZE];

  if (blk > nblocks){
    printf("inumber %d out of range\n", blk);
    return;
  }

  // get inode bitmap block
  get_block(dev, bmap, buf);
  clr_bit(buf, blk-1);

  // write buf back
  put_block(dev, bmap, buf);

  // update free inode count in SUPER and GD
  incFreeInodes(dev);
}

void truncate(MINODE * mip)
{
    for(int i = 0; i < 12 && mip->INODE.i_block[i]; i++)
        bdalloc(mip->dev, mip->INODE.i_block[i]);

    int buf[256];
    if(mip->INODE.i_block[12] != 0)
    {
        get_block(mip->dev, mip->INODE.i_block[12], (char*)buf);
        for(int i = 0; i < 256 && buf[i] != 0; i++)
            bdalloc(mip->dev, buf[i]);
    }

    if(mip->INODE.i_block[13] != 0)
    {
        get_block(mip->dev, mip->INODE.i_block[13], (char*)buf);  
        for(int i = 0; i < 256 && buf[i] != 0; i++)
        {
            int buf2[256];
            get_block(mip->dev, buf[i], (char*)buf2);
            for(int j = 0; j < 256 && buf2[j] != 0; j++)
            {
                bdalloc(mip->dev, buf2[j]);
            }

            bdalloc(mip->dev, buf[i]);
        }
    }
}

void zero_block(int dev, int blk)
{
  char buf[BLKSIZE];
  memset(buf, 0, BLKSIZE);
  put_block(dev, blk, buf);
}