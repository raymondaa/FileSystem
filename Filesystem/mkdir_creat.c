/*********** mkdir_creat.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

int enter_name(MINODE *pip, int myino, char *myname)
{
    int i;
	INODE *parent_ip = &pip->INODE;

	char buf[1024];
	char *cp;
	DIR *dp;

	int need_len = 0, ideal = 0, remain = 0;
	int bno = 0, block_size = 1024;

	for(i = 0; i < parent_ip->i_size / BLKSIZE; i++)
	{
		if(parent_ip->i_block[i] == 0)
			break;

		bno = parent_ip->i_block[i];

        //get data block of parent directory into buf
		get_block(dev, bno, buf);

		dp = (DIR*)buf;
		cp = buf;

		//need length
		need_len = 4 * ( (8 + strlen(myname) + 3) / 4);
		printf("need len is %d\n", need_len);

		//step into last dir entry
		while(cp + dp->rec_len < buf + BLKSIZE)
		{
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}

		printf("last entry is %s\n", dp->name);
		cp = (char*)dp;

		//ideal length uses name len of last dir entry
		ideal = 4 * ( (8 + dp->name_len + 3) / 4);

        //remain = last entry's rec length - ideal length
		remain = dp->rec_len - ideal;
		printf("remain is %d\n", remain);

        //set new entry as last entry and trim the previous entry to ideal length
		if(remain >= need_len)
		{
			//set rec_len to ideal
			dp->rec_len = ideal;

            //move the new entry to the last entry
			cp += dp->rec_len;
			dp = (DIR*)cp;

			//sets the dirpointer inode to the given myino
			dp->inode = myino;
			dp->rec_len = block_size - ((u32)cp - (u32)buf);
			printf("rec len is %d\n", dp->rec_len);
			dp->name_len = strlen(myname);
			dp->file_type = EXT2_FT_DIR;
			//sets the dp name to the given name
			strcpy(dp->name, myname);

			//puts the block
			put_block(dev, bno, buf);

			return 0;
		}
	}

	printf("number is %d\n", i);

	//no space in existing data blocks, must allocate in next block
	bno = balloc(dev);
	parent_ip->i_block[i] = bno;

    //increment parent size by BLKSIZE
	parent_ip->i_size += BLKSIZE;
	pip->dirty = 1;

	get_block(dev, bno, buf);

    //Enter new entry as first entry in the new data block
	dp = (DIR*)buf;
	cp = buf;

	printf("dir name is %s\n", dp->name);

    //initialize
	dp->inode = myino;
	dp->rec_len = BLKSIZE;
	dp->name_len = strlen(myname);
	dp->file_type = EXT2_FT_DIR;
	strcpy(dp->name, myname);

    //write block back to disk
	put_block(dev, bno, buf);

	return 0;

}


int mymkdir(MINODE *pip, char *name)
{
    MINODE *mip;
    int ino, bno;

    //allocate INODE number and a block number
    ino = ialloc(dev);
    bno = balloc(dev);

    printf("ino = %d\tbno = %d\n",ino, bno);

    mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
    ip->i_uid  = running->uid;	// Owner uid 
    ip->i_gid  = running->cwd->INODE.i_gid;	// Group Id
    ip->i_size = BLKSIZE;		// Size in bytes 
    ip->i_links_count = 2;	        // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
    ip->i_blocks = 2;                	// LINUX: Blocks count in 512-byte chunks 
    ip->i_block[0] = bno;             // new DIR has one data block   
    for(int i = 1; i<15; i++){
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;
    iput(mip);

    char *cp, buf[1024];
    DIR *dp;

    //create data block for . and ..
    get_block(dev, bno, buf);

    dp = (DIR*)buf;
    cp = buf;

    //setting . for new DIR
    dp->inode = ino;
    dp->name_len = 1;
    dp->rec_len = 12;
    dp->file_type = EXT2_FT_DIR;
    dp->name[0] = '.';
    cp += dp->rec_len;
    dp =(DIR*)cp;
    //setting .. for new DIR
    dp->inode = pip->ino;
    dp->rec_len = 1012;     //. will always take up 12 at this point
    dp->name_len = 2;
    dp->file_type = EXT2_FT_DIR;
    dp->name[0] = '.';
    dp->name[1] = '.';

    put_block(dev, bno, buf);   //write back to disk

    enter_name(pip, ino, name);
    
    return 0;
}


int make_dir()
{
    MINODE *start, *pmip;
    INODE *pip;
    int pino;
    char parent[256], child[256];
    char temp1[256], temp2[256];

    //determine if path is either absolute or relative
    if (pathname[0] == "/"){
        start = root;
        dev = root->dev;
    }
    else{
        start = running->cwd;
        dev = running->cwd->dev;
    }
    strcpy(temp1, pathname);
    strcpy(temp2, pathname);

    //divide pathname into dirname and basename
    strcpy(parent, dirname(temp1));
    strcpy(child, basename(temp2));
    pino = getino(parent);
    pmip = iget(dev, pino);

    //need INODE of pmip for checks
    pip = &pmip->INODE;

    // check if pip is a DIR
    if(!S_ISDIR(pip->i_mode))
    {
        printf("The specified path is not a directory.\n");
        return -1;
    }
    //search that child does not already exist in parent directory
    if(search(pip ,child) != 0)
    {
        printf("Entry %s already exists.\n", child);
        return -1;
    }
    mymkdir(pmip, child);

    pip->i_links_count++;
    pip->i_atime = time(0L);
    pmip->dirty = 1;
}

int creat_file()
{
    MINODE *start, *pmip;
    INODE *pip;
    int pino;
    char parent[256], child[256];
    char temp1[256], temp2[256];


    if (pathname[0] == "/"){
        start = root;
        dev = root->dev;
    }
    else{
        start = running->cwd;
        dev = running->cwd->dev;
    }
    strcpy(temp1, pathname);
    strcpy(temp2, pathname);

    strcpy(parent, dirname(temp1));
    strcpy(child, basename(temp2));
    pino = getino(parent);
    pmip = iget(dev, pino);
    pip = &pmip->INODE;

    // check if pip is a DIR
    if(!S_ISDIR(pip->i_mode))
    {
        printf("The specified path is not a directory.\n");
        return -1;
    }
    //search that child does not already exist in parent directory
    if(search(pip,child) != 0)
    {
        printf("Entry %s already exists.\n", child);
        return -1;
    }
    my_creat(pmip, child);

    pip->i_atime = time(0L);
    pmip->dirty = 1;
}

int my_creat(MINODE *pip, char* name)
{
    MINODE *mip;
    int ino, bno;
    ino = ialloc(dev);
    bno = balloc(dev);

    printf("ino = %d\tbno = %d\n",ino, bno);

    mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x81A4;		// OR 0x81A4: FILE type and permissions
    ip->i_uid  = running->uid;	// Owner uid 
    ip->i_gid  = running->cwd->INODE.i_gid;	// Group Id
    ip->i_size = 0;		// Size in bytes 
    ip->i_links_count = 1;	        // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
    ip->i_blocks = 0;                             // data blocks = 0   
    for(int i = 0; i<15; i++){                //set data blocks to 0
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;
    iput(mip);


    enter_name(pip, ino, name);
    
    return ino;
}