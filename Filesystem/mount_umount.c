/*********** mount_umount.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern MOUNT mounttable[NMTABLE]; 

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256], pathname2[256];


int mount()
{
    int i, fd, ino;
    char buf[BLKSIZE];
    MINODE *mip;
    MOUNT *mtptr;
    //No parameters provided, display current mounted FS
    if (strcmp(pathname, "") == 0){
        printf("Filesystems currently mounted: \n");
        for(i = 0; i < NMTABLE; i++){
            if (mounttable[i].dev > 0){
                printf("%s is mounted on %s\n", mounttable[i].devName, mounttable[i].mntName);
            }
        }
        return 0;
    }

    //check if filesystem is already mounted
    for (i = 0; i < NMTABLE; i++){
        //if FS is mounted, mount fails
        if ((mounttable[i].dev > 0) && (strcmp(mounttable[i].devName, pathname) == 0)){
            printf("The filesytem is already mounted.\n");
            return -1;
        }
    }

    //Allocate a free mount table entry
    for (i = 0; i < NMTABLE; i++){
        if (mounttable[i].dev == 0){        //entry in mountTable is free
            mtptr = &(mounttable[i]);
        }
    }

    //open FS for RW and make fd = DEV
    strcpy(pathname2, "2");    // 2 is RW mode
    fd = open_file();
    if (fd == -1){
        printf("Could not open the FS.\n");
        return -1;
    }
    dev = fd;

    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    /* verify it's an ext2 file system *****************/
    if (sp->s_magic != 0xEF53){
        printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
        exit(1);
    }     

    //find mount points ino and get its MINODE
    ino = getino(pathname);
    mip = iget(dev, ino);

    //check if mount point is a DIR
    if(!S_ISDIR(mip->INODE.i_mode))
    {
        printf("The specified FS is not a directory.\n");
        return -1;
    }

    if (running->cwd->dev == mip->dev){
        printf("The mount point is busy.\n");
        return -1;
    }

    //Record new DEV in the mount table entry
    mtptr->dev = dev;
    strcpy(mtptr->devName, pathname);
    mtptr->ninodes = sp->s_inodes_count;
    mtptr->nblocks = sp->s_blocks_count;
    mtptr->mntDirPtr->mounted = 1;
    mtptr->mntDirPtr = iget(dev, 2);     // 2 is the root inode

    return 0;
}

int umount(char *filesys)
{
    int i;
    MOUNT *umnt = 0;
    MINODE *mip;

    //check if a FS has been entered
    if (strcmp(filesys, "") == 0)
    {
        printf("No FS entered to unmount.\n");
        return -1;
    }

    //check if the FS for unmounting is mounted
    for (i = 1; i < NMTABLE; i++){
        if(umnt->dev == minode[i].dev && strcmp(mounttable[i].mntName, filesys) == 0){
            umnt = &mounttable[i];
            break;
        }
    }

    if (umnt == 0){
        printf("%s is not mounted.\n", filesys);
        return -1;
    }

    //check if FS is still active in the mounted filesystem
    // someone's CWD or opened files are still there cannot umount
    // must check all minode[].dev
    for (i = 1; i < NMINODE; i++){
        if(umnt->dev == minode[i].dev && minode[i].ino != 2){
            printf("%s is busy and cannot be umounted.\n");
            return -1;
        }
    }

    //find the mount point's INODE reset to "not mounted" the iput() minode
    mip = umnt->mntDirPtr;
    mip->mounted = 0;
    iput(mip);
    close(umnt->dev);
    umnt->dev = 0;

    return 0;
    


}