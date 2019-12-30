/************* cd_ls_pwd.c file **************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007

change_dir(char *pathname)
{
  int ino;
  MINODE *mip = NULL;

  //check whether the path is entered or path is root
  if (strlen(pathname) == 0 || strcmp(pathname, "/") == 0){
    iput(running->cwd);
    running->cwd = root;
    return 0;
  }

  else{
    ino = getino(pathname);
    if(ino == 0){
      printf("%s does not exist.\n", pathname);
      return -1;
    }
    //get MINODE from ino
    mip = iget(dev,ino);

    //check if the MINODE is a DIR
    if(!(S_ISDIR(mip->INODE.i_mode))){
      printf("%s is not a directory\n",pathname);
      return -1;
    }

    //release the old cwd
    iput(running->cwd);

    //change the new cwd to mip
    running->cwd = mip;
  }
  
}


int list_file(char *pathname)
{
  char temp[256];
  int ino;

  //No pathname entered display contents of cwd
  if(strlen(pathname) == 0)
  {
      strcpy(pathname, ".");
      ino = running->cwd->ino;
  }
  else
  {
     strcpy(temp, pathname);
     ino = getino(temp);
  }
  MINODE *mip = iget(dev, ino);

  if(S_ISDIR(mip->INODE.i_mode))
  {
      ls_dir(pathname);
  }
  else
  {
      ls_file(ino, pathname);
  }
  iput(mip);
}


int ls_dir(char *dirname)
{
int ino = getino(dirname);
    MINODE * mip = iget(dev, ino);
    char buf[BLKSIZE];
    for(int i = 0; i < 12 && mip->INODE.i_block[i]; i++)
    {
        get_block(dev, mip->INODE.i_block[i], buf);
        char * cp = buf;
        DIR * dp = (DIR*)buf;

        while(cp < BLKSIZE + buf)
        {
            char temp[256];
            strncpy(temp, dp->name, dp->name_len);    //copy name
            temp[dp->name_len] = 0;
            ls_file(dp->inode, temp);
            cp += dp->rec_len;          //move cp and dp to the next entry
            dp = (DIR*)cp;
        }
    }
    iput(mip);
}

int ls_file(int ino, char *filename)
{
   MINODE * mip = iget(dev, ino);
    const char * t1 = "xwrxwrxwr-------";
    const char * t2 = "----------------";
    if(S_ISREG(mip->INODE.i_mode))
        putchar('-');
    else if(S_ISDIR(mip->INODE.i_mode))
        putchar('d');
    else if(S_ISLNK(mip->INODE.i_mode))
        putchar('l');
    for(int i = 8; i >= 0; i--)
        putchar(((mip->INODE.i_mode) & (1 << i)) ? t1[i] : t2[i]);

    time_t t = (time_t)(mip->INODE.i_ctime);
    char temp[256];
    strcpy(temp, ctime(&t))[strlen(temp) - 1] = '\0';

    printf(" %4d  %4d  %4d  %4d   %s\t%s", (int)(mip->INODE.i_links_count), (int)(mip->INODE.i_gid), (int)(mip->INODE.i_uid), (int)(mip->INODE.i_size),
            temp, filename);

    if(S_ISLNK(mip->INODE.i_mode))
        printf(" -> %s", (char*)(mip->INODE.i_block));

    putchar('\n');

    iput(mip);   
}


int pwd(MINODE *wd)
{
  if(wd == root)
    printf("/");
  else
    rpwd(wd);
  putchar('\n');
}

int rpwd(MINODE *wd)
{
  char myname[256], *cp, dbuf[BLKSIZE];
  int myino, parentino;
  MINODE *pip;
  DIR *dp;

  if(wd == root) return 0;

  myino = search(wd, ".");
  parentino = search(wd, "..");

  //get parent MINODE
  pip = iget(dev, parentino);

  get_block(fd, pip->INODE.i_block[0], dbuf);

  dp = (DIR*)dbuf;
  cp = dbuf;

  while (cp < dbuf + BLKSIZE){
    //get directory name save to myname
    strncpy(myname, dp->name, dp->name_len);
    myname[dp->name_len] = 0;

    if(dp->inode == myino)
      break;
    
    //move to next directory
    cp += dp->rec_len;
    dp = (DIR*)cp;
  }
  rpwd(pip);
  printf("/%s", myname);


}



