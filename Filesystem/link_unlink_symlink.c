/*********** link_unlink_symlink.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256], pathname2[256];


int link()
{
    int ino, pino;
    char parent[256], child[256], temp[256], temp2[256];
    MINODE *mip, *pmip;

    ino = getino(pathname);

    //check if file exists
    if(!ino){
        printf("File does not exist to link\n");
        return -1;
    }

    mip = iget(dev, ino);

    //check that file is not a DIR
    if(S_ISDIR(mip->INODE.i_mode)){
        printf("Cannot link a directory.\n");
        return -1;
    }

    strcpy(temp, pathname2);
    strcpy(parent, dirname(temp));

    pino = getino(parent);

    // check if parent directory exists
    if(!pino){
        printf("The parent directory for the file does not exist.\n");
        return -1;
    }

    pmip = iget(dev, pino);

    //check that parent is a directory valid to put file in
    if(!S_ISDIR(pmip->INODE.i_mode)){
        printf("Destination is not a directory.\n");
        return -1;
    }
    strcpy(temp2, pathname2);
    strcpy(child, basename(temp2));

    //check if file exists within parent directory
    if(search(pmip, child)){
        printf("Destination file already exists.\n");
        return -1;
    }

    enter_name(pmip, ino, child);

    mip->INODE.i_links_count++;
    mip->dirty = 1;

    iput(mip);
    iput(pmip);
}

int unlink()
{
    int ino, pino;
    MINODE *mip, *pmip;
    char temp[256],temp2[256], parent[256], child[256];

    ino = getino(pathname);

    //check if file exists
    if(!ino){
        printf("could not find file for unlinking.\n");
        return -1;
    }

    mip = iget(dev, ino);

    // check if it is a file
    if(S_ISDIR(mip->INODE.i_mode)){
        printf("Cannot remove a directory with unlink.\n");
        return -1;
    }

    //decrease link count by 1
    mip->INODE.i_links_count--;

    if(mip->INODE.i_links_count == 0){
        truncate(mip);  //deallocate all blocks in INODE
        idalloc(dev, ino);   //deallocate INODE
    }
    else{
        mip->dirty = 1;
    }

    strcpy(temp, pathname);
    strcpy(parent, dirname(temp));
    pino = getino(parent);
    pmip = iget(dev, pino);
    strcpy(temp2, pathname);
    strcpy(child, basename(temp2));
    rm_child(pmip, child);

    iput(pmip);
    iput(mip);
}

int symlink()
{
	int ino, i, link_ino, pino;
	char temp[256], temp2[256], tempOld[256], parent[256], child[256];
	char oldName[256];

	MINODE *mip;
	MINODE *pmip;
	MINODE *link_mip;

	INODE *link_ip;

	strcpy(tempOld, pathname);
	strcpy(oldName, basename(tempOld));

	//get inode of old file
	ino = getino(pathname);
	mip = iget(dev, ino);

    //make sure pathname is less than 60. (84 bytes - 24 unused bytes after INODE i_block[]
	if(strlen(pathname) > 60)
	{
		printf("name too long.\n");
		return -1;
	}

    //check if old file exists
	if(!mip)
	{
		printf("%s does not exist\n", pathname);
		return -1;
	}

	//get parent and child of old file pathname
	strcpy(temp, pathname2);
	strcpy(parent, dirname(temp));
    strcpy(temp2, pathname);
	strcpy(child, basename(pathname2));

	printf("parent is %s,  child is %s\n", parent, child);

	pino = getino(parent);
	pmip = iget(dev, pino);

	if(!pmip)
	{
		printf("can't get parent mip.\n");
		return -1;
	}

	if(!S_ISDIR(pmip->INODE.i_mode))
	{
	 	printf("parent is not a directory.\n");
	 	return -1;
	}

	if(getino(child) > 0)
	{
		printf("%s already exists\n", child);
		return -1;
	}

	link_ino = my_creat(pmip, child);
	//gets the newfile minode to set it's variables
	link_mip = iget(dev, link_ino);
	//link_ip = &link_mip->INODE;

	//set the link mode
	link_mip->INODE.i_mode = 0120000;
    strcpy((char*)(link_mip->INODE.i_block), pathname);  //set name file is linked to
	//set the link size, which is the size of the oldfiles name
	link_mip->INODE.i_size = strlen(oldName);

	link_mip->dirty = 1;
	iput(link_mip);
	iput(mip);
    
}