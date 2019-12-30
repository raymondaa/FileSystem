/****************************************************************************
*                   main.c file                                             *
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h>

#include "type.h"
#include "util.c"
#include "cd_ls_pwd.c"
#include "mkdir_creat.c"
#include "rmdir.c"
#include "link_unlink_symlink.c"
#include "open_close_lseek.c"
#include "read_cat.c"
#include "write_cp.c"
#include "mount_umount.c"

MINODE minode[NMINODE];
MINODE *root;
PROC   proc[NPROC], *running;
MOUNT mounttable[NMTABLE];

char   gpath[256]; // global for tokenized components
char   *name[64];  // assume at most 64 components in pathname
int    n;          // number of component strings

int    fd, dev;
int    nblocks, ninodes, bmap, imap, inode_start;
char   line[256], cmd[32], pathname[256], pathname2[256];


char *disk = "mydisk";

int init()
{
   int i,j; 
   for (i=0; i<NMINODE; i++) // initialize all minodes as FREE
      minode[i].refCount = 0;
   for (i=0; i<NMTABLE; i++) // initialize mtable entries as FREE
      mounttable[i].dev = 0; 
   for (i=0; i<NPROC; i++){ // initialize PROCs
      proc[i].status = READY; // reday to run
      proc[i].pid = i; // pid = 0 to NPROC-1
      proc[i].uid = i; // P0 is a superuser process
      for (j=0; j<NFD; j++) 
         proc[i].fd[j] = 0; // all file descriptors are NULL
      proc[i].next = &proc[i+1];
   } 
   proc[NPROC-1].next = &proc[0]; // circular list
   running = &proc[0]; // P0 runs first
}


int mount_root()
{
   int i;
   MOUNT *mp;
   SUPER *sp;
   GD *gp;
   char buf[BLKSIZE];

   dev = open(disk, O_RDWR);
   if (dev < 0){ 
      printf("panic : can’t open root device\n");
      exit(1); 
   }
   /* get super block of rootdev */
   get_block(dev, 1, buf);
   sp = (SUPER *)buf;
   /* check magic number */
   if (sp->s_magic != 0xEF53){
      printf("super magic=%x : %s is not an EXT2 filesys\n", sp->s_magic, disk);
      exit(0);
   } 
   // fill mount table mtable[0] with rootdev information
   mp = &mounttable[0]; // use mtable[0]
   mp->dev = dev;
   // copy super block info into mtable[0]
   ninodes = mp->ninodes = sp->s_inodes_count;
   nblocks = mp->nblocks = sp->s_blocks_count;
   strcpy(mp->devName, disk);
   strcpy(mp->mntName, "/");
   get_block(dev, 2, buf);
   gp = (GD *)buf;
   bmap = mp->bmap = gp->bg_block_bitmap; 
   imap = mp->imap = gp->bg_inode_bitmap; 
   inode_start = mp->iblock = gp->bg_inode_table;
   printf("bmap=%d imap=%d iblock=%d\n", bmap, imap, inode_start);

   // call iget(), which inc minode’s refCount 
   root = iget(dev, 2); // get root inode
   mp->mntDirPtr = root; // double link
   root->mntptr = mp;
   // set proc CWDs
   for (i=0; i<NPROC; i++) // set proc’s CWD
   proc[i].cwd = iget(dev, 2); // each inc refCount by 1
   printf("mount : %s mounted on / \n", disk); 
   return 0;
}

int main(int argc, char *argv[ ])
{

  init();
  mount_root();

  //printf("hit a key to continue : "); getchar();
  while(1){
    printf("input command : [ls|cd|pwd|quit|mkdir|creat|rmdir|link|unlink|symlink\nopen|close|lseek|read|write|cat|cp|mount|umount] ");
    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;
    if (line[0]==0)
      continue;
    pathname[0] = 0;
    cmd[0] = 0;
    
    sscanf(line, "%s %s %s", cmd, pathname, pathname2);
    printf("cmd=%s pathname=%s\n", cmd, pathname);

    if (strcmp(cmd, "ls")==0)
       list_file(pathname);
    if (strcmp(cmd, "cd")==0)
       change_dir(pathname);
    if (strcmp(cmd, "pwd")==0)
       pwd(running->cwd);
    if (strcmp(cmd, "mkdir")==0)
       make_dir();
    if (strcmp(cmd, "creat")==0)
       creat_file();
    if (strcmp(cmd, "rmdir")==0)
       rmdir();
       if (strcmp(cmd, "link")==0)
       link();
    if (strcmp(cmd, "unlink")==0)
       unlink();
    if (strcmp(cmd, "symlink")==0)
       symlink();
    if (strcmp(cmd, "open")==0)
       open_file();
    if (strcmp(cmd, "close")==0)
       //close_file();
    if (strcmp(cmd, "lseek")==0)
       //lseek();
    if (strcmp(cmd, "read")==0)
       read_file();
    if (strcmp(cmd, "cat")==0)
       cat();
    if (strcmp(cmd, "write")==0)
       write_file();
    if (strcmp(cmd, "cp")==0)
       cp();
    if (strcmp(cmd, "mount")==0)
       mount();
    if (strcmp(cmd, "umount")==0)
       umount(pathname);
    if (strcmp(cmd, "quit")==0)
       quit();
  }
  return 0;
}
 
 //Write all modified MINODES back to disk
int quit()
{
  int i;
  MINODE *mip;
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount && mip->dirty){
      mip->refCount = 1;
      iput(mip);
    }
  }
  exit(0);
}
