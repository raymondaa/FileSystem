/*********** rmdir.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

int isEmpty(MINODE *mip)
{
	char buf[1024];
	INODE *ip = &mip->INODE;
	char *cp;
	char name[64];
	DIR *dp;

	//link count greater than 2 has files
	if(ip->i_links_count > 2)
	{
		return 1;
	}
	else if(ip->i_links_count == 2)
	{
		//link count of 2 could still have files, check data blocks
		if(ip->i_block[1])
		{
			get_block(dev, ip->i_block[1], buf);

			cp = buf;
			dp = (DIR*)buf;

			//step through DIR entries
			while(cp < buf + 1024)
			{
				strncpy(name, dp->name, dp->name_len);
				name[dp->name_len] = 0;

				if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
				{
					//not empty!
					return 1;
				}
			}
		}
	}
	else
		return 0;//is empty, return 0
}

int rm_child(MINODE *parent, char *name)
{
    int i;
	INODE *pip = &parent->INODE;
	DIR *dp;
	DIR *prev_dp;
	DIR *last_dp;
	char buf[1024];
	char *cp;
	char temp[64];
	char *last_cp;
	int start, end;

	printf("going to remove %s\n", name);
	printf("parent size is %d\n", pip->i_size);

	//iterate through blocks
	//this finds the child 
	for(i = 0; i < 12 ; i++)
	{
		if(pip->i_block[i] == 0)
			return;

		get_block(dev, pip->i_block[i], buf);
		cp = buf;
		dp = (DIR*)buf;

		printf("dp at %s\n", dp->name);

		while(cp < buf + 1024)
		{
			strncpy(temp, dp->name, dp->name_len);
			temp[dp->name_len] = 0;

			printf("dp is at %s\n", temp);
			//check if child is found
			if(!strcmp(temp, name))
			{
				printf("child found!\n");
				if(cp == buf && cp + dp->rec_len == buf + 1024)
				{
					//it's the first and only entry, need to delete entire block
					free(buf);
					bdalloc(dev, pip->i_block[i]);

					pip->i_size -= 1024;

					//shift blocks left
					while(pip->i_block[i + 1] && i + 1 < 12)
					{
						i++;
						get_block(dev, pip->i_block[i], buf);
						put_block(dev, pip->i_block[i - 1], buf);
					}
				}
				else if(cp + dp->rec_len == buf + 1024)
				{
					//just have to remove the last entry
					printf("removing last entry\n");
					prev_dp->rec_len += dp->rec_len;
					put_block(dev, pip->i_block[i], buf);
				}
				else
				{
					//in the middle of two entries
					printf("Before dp is %s\n", dp->name);

					last_dp = (DIR*)buf;
					last_cp = buf;

					//step into last entry
					while(last_cp + last_dp->rec_len < buf + BLKSIZE)
					{
						printf("last_dp at %s\n", last_dp->name);
						last_cp += last_dp->rec_len;
						last_dp = (DIR*)last_cp;
					}

					printf("%s and %s\n", dp->name, last_dp->name);
					//add deleted rec_len to last entry
					last_dp->rec_len += dp->rec_len;

					start = cp + dp->rec_len;
					end = buf + 1024;

					memmove(cp, start, end - start);//built in function. move memory left by length of entry

					put_block(dev, pip->i_block[i], buf);

				}

				parent->dirty = 1;
				iput(parent);
				return 0;
			}//end of child found

			prev_dp = dp;     //move to next entry
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}
	}

	return 0;
}


int rmdir()
{
    int ino, i, pino;
    MINODE *mip, *pmip;
    INODE  *pip;
    char temp[256], child[256], temp2[256], parent[256];

    strcpy(temp, pathname);
    strcpy(child, basename(temp));
    strcpy(temp2, pathname);
    strcpy(parent, dirname(temp2));
    ino = getino(pathname);
    mip = iget(dev, ino);

    //check user id or super user
    if(running->uid != 0 && running->uid != pip->i_uid){
        printf("Cannot remove, not the owner of file or SUPER user.\n");
        return -1;
    }

	//check if MINODE exists
    if(!mip){
        printf("mip does not exist.\n");
        return -1;
    }

	//Checks if the desired directory to be deleted is empty
    if(isEmpty(mip)){
        printf("The desired directory to be removed is not empty.\n");
        return -1;
	}

	
	// check if the directory is being referenced by other files, Not BUSY
	if(mip->refCount == 1){
        printf("mip is being used by another file, cannot be opened\n");
        return -1;
    }

	//Check path given is a directory
    if(!S_ISDIR(mip->INODE.i_mode)){
        printf("%s is not directory.\n", pathname);
        return -1;
    }

    //deallocate blocks
    for(i = 0; i < 12; i++)
    {
        if(mip->INODE.i_block[i] == 0)
            continue;
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }
    idalloc(mip->dev, mip->ino);
    iput(mip);

    //get parent DIR's ino and minode
    if(strcmp(parent, ".") == 0 || strcmp(parent, "/") == 0)
    {
        if(pathname[0] == '.') pino = running->cwd->ino;
        else pino = root->ino;
    }
    else pino = getino(parent);
    pmip = iget(dev, pino);

    rm_child(pmip, child);

    pmip->INODE.i_links_count--;
    pmip->INODE.i_atime = time(0L);
    pmip->INODE.i_mtime = time(0L);
    pmip->dirty = 1;

    iput(pmip);
}