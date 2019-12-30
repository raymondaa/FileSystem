/*********** open_close_lseek.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256], pathname2[256];


int open_file()
{
    int mode, ino, i;
    MINODE *mip;
    OFT *oftp;
    
    //convert string pathname2 into an int mode
    mode = atoi(pathname2);

    //check if mode is valid   0|1|2|3 = R|W|RW|APPEND
    if (mode < 0 || mode >3){
        printf("Invalid mode entered. Only 0,1,2,3 are valid.\n");
        return -1;
    }

    //get inumber of pathname
    if(pathname[0] == '/')
        dev = root->dev;
    else
        dev = running->cwd->dev;
    
    ino = getino(pathname);

    //check if the file to be opened exists, if not create a new one
    if (!ino){
        creat_file();
        ino = getino(pathname);
    }

    mip = iget(dev, ino);

    //verify that the file is REG type
    if(!S_ISREG(mip->INODE.i_mode)){
        printf("Cannot open %s it is not a REG file.\n", pathname);
        iput(mip);
        return -1;
    }

    //verify permissions of file
    if (mip->INODE.i_uid != running->uid){
        printf("Cannot open %s incorrect permission.\n",pathname);
        iput(mip);
        return -1;
    }

    //check to see if the file is already opened with incompatible type
    for (i = 0; running->fd[i]; i++)
    {
        if(running->fd[i]->mode != 0){
            printf("File %s is opened for another type.\n" ,pathname);
            iput(mip);
            return -1;
        }
    }

    //allocate OFT and fill in values
    running->fd[i] = (OFT*)malloc(sizeof(OFT));
    oftp = running->fd[i];
    oftp->mode = mode;
    oftp->refCount = 1;
    oftp->mptr = mip;

    //set offset based on mode
    switch(mode){
        case 0: 
            oftp->offset = 0;             // R: offset = 0;
            break;
        case 1: 
            truncate(mip);                // W: truncate file to 0 size
            oftp->offset = 0;
            break;
        case 2: 
            oftp->offset = 0;             // RW: do NOT truncate file
            break;
        case 3: 
            oftp->offset = mip->INODE.i_size;      //APPEND MODE
            break;
        default: 
            printf("invalid mode.\n");
            return -1;
    }

    if(mode == 0){
        mip->INODE.i_atime = time(0L);
    }
    else{
        mip->INODE.i_atime = time(0L);
        mip->INODE.i_mtime = time(0L);
    }

    return i; //return fd
}

int close_file(int fd)
{
    MINODE *mip;

    //check if fd is within bounds and running->fd[fd] is pointing at valid OFT
    if(!(0 <= fd && fd < 10) || running->fd[fd] == 0){
        printf("Invalid file descriptor.\n");
        return -1;
    }
    OFT *oftp = running->fd[fd];
    running->fd[fd] = 0;        // set the entry to FREE
    oftp->refCount--;
    if (oftp->refCount > 0){
        return 0;
    }
    mip = oftp->mptr;
    iput(mip);

    return 0;
}


int mylseek(int fd, int position)
{
    int originalPosition = running->fd[fd]->offset;
    OFT *oftp = running->fd[fd];

    //check if position is within the bounds of the file
    if(position < 0 || position > oftp->mptr->INODE.i_size){
        printf("Invalid position entered.\n");
        return -1;
    }
    //change offset of OFT entry to position
    oftp->offset = position;
    return originalPosition;
}

int pfd()
{
    int i;
    OFT *oftp;
    printf("\nfd     mode     offset     INODE\n");
    printf("----     ----     -----     -------");

    //run through OFT for all current open files
    for (i = 0; running->fd[i]; i++)
    {
        oftp = running->fd[i];

        if (oftp->refCount == 0)
        {
            return -1;
        }

        printf(" %d      ", i);

        switch(oftp->mode)
        {
            case 0:
                printf("READ\t");
                break;
            case 1:
                printf("WRITE\t");
                break;
            case 2:
                printf("RW\t");
                break;
            case 3:
                printf("AP\t");
                break;
            default:
                printf("N/A\t");
                break;
        }
        printf("%d\t%d\n", oftp->offset, oftp->mptr->ino);
    }
    printf("---------------------------------------\n");
    return 0;
}