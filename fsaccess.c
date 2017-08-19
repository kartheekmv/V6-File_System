/***************************************/

//CS5348 Project-2: V6-Based File System
//Implementation of Modified Unix Version-6

//Execution Steps:
//1. Compile using: g++ -o fsaccess fsaccess.c
//2. Run using: ./fsacess <nameofV6filesystem>

//Available Commands:
//initfs
//cpin
//cpout
//mkdir
//rm
//q
//cd
//ls
//help

/***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

//Set of User Defined Values
#define sizeofblk 512 //Set BLOCK SIZE
#define sizeofinode 32 //SET inode SIZE

//Structure for super block
struct super_block {
	unsigned short isize;
	unsigned short fsize;
	unsigned short nfree;
	unsigned short free[100];
	unsigned short ninode;
	unsigned short inode[100];
	char flock;
	char ilock;
	char fmod;
	unsigned short time[2];
};

//Structure for inode
struct inode{
	unsigned short flags;
	char nlinks;
	char uid;
	char gid;
	char size0;
	unsigned short size1;
	unsigned short addr[8];
	unsigned short atime[2];
	unsigned short mtime[2];
};

//Support structure to increase size to 32MB
struct size25{
	unsigned short lowbyte;
	unsigned char highbyte;
	unsigned char extrabit;
};

int fd = -1;
struct super_block super_block = {0, 0, 0, {0}, 0, {0}, 0, 0, 0, {0}};
int curdirinode = 1;
char dirpath[256] = {0};

//Flags combinations
#define inodealloc 0100000
#define filetype2bit 060000
#define size25MAX_VALUE 0177777777
#define directory 040000
#define filemode S_IRUSR | S_IWUSR | S_IXUSR
#define largefile 010000
#define size25bit 01000

//Convert Data size to long
unsigned long size25conv(struct size25 val) {
	unsigned long res = 0;

	res = val.lowbyte;
	res += val.highbyte * 0200000;
	res += val.extrabit * 0100000000;

	return res;
}

//Convert data size from long to inode format
void size25revconv(struct size25* res, unsigned long val) {
	unsigned short temp;

	res->lowbyte = val;
	temp = val / 0200000;
	res->highbyte = temp;
	res->extrabit = temp / 0400;
}

int max(int a, int b) {
	return a > b ? a : b;
}

//function to read input command(choose from available commands)
char* scaninput() {
	int size = 1;
	int index = -1;
	char* str = (char*)malloc(size * sizeof(char));
	char c;

	while(1) {
		c = getchar();

		index++;

		if(index >= size) {
			size++;
			str = (char*)realloc(str, size * sizeof(char));
		}

		if(c != '\n') {
			str[index] = c;
		}
		else {
			str[index] = 0;
			break;
		}
	}

	return str;
}

//process input command arguments
char** arguments(char* str) {
	int size = 2;
	int index = 1;
	char** args = (char**)malloc(size * sizeof(char*));
	int pointer = 0;

	args[0] = &str[0];

	while(str[pointer] != 0) {
		if(str[pointer] == ' ') {
			size++;
			args = (char**)realloc(args, size * sizeof(char*));
			str[pointer] = 0;
			args[index] = &str[pointer + 1];
			index++;
		}

		pointer++;
	}

	args[index] = NULL;

	return args;
}

//Convert to integers
int parseInt(char* str) {
	int pointer = 0;
	int val = 0;

	while(str[pointer] != 0) {
		if(str[pointer] >= '0' && str[pointer] <= '9') {
			val = 10 * val + (str[pointer] - '0');
			pointer++;
		}
		else {
			return -1;
		}
	}

	return val;
}

int namecheck(char* str) {
	int pointer = 0;

	while(str[pointer] != 0) {
		if(str[pointer] == 34 || str[pointer] == 42 || str[pointer] == 47 || str[pointer] == 60 || str[pointer] == 58 || str[pointer] == 62 || str[pointer] == 63 || str[pointer] == 124) 
		{
			return -1;
		}

		pointer++;
	}

	if(pointer > 14) {
		return -1;
	}

	return 0;
}

//function to load superblock
int loadsuper_block() {
	
	lseek(fd, sizeofblk, SEEK_SET);
	read(fd, &super_block, sizeof(struct super_block));

	return 0;
}

//function to save changes to superblock
int savesuper_block() {
	time_t t;
	time(&t);
	super_block.time[0] = t % 0200000;
	super_block.time[1] = t / 0200000;

	lseek(fd, sizeofblk, SEEK_SET);
	write(fd, &super_block, sizeof(struct super_block));

	return 0;
}


int writeblk(int blkid) {
	if(blkid >= super_block.fsize) {
		return -1;
	}

	if(super_block.nfree == 100) {
		int offset = blkid * sizeofblk;

		lseek(fd, offset, SEEK_SET);
		write(fd, &super_block.nfree, 2);
		offset += 2;

		lseek(fd, offset, SEEK_SET);
		write(fd, super_block.free, 2 * super_block.nfree);

		super_block.free[0] = blkid;
		super_block.nfree = 1;
	}
	else {
		super_block.free[super_block.nfree] = blkid;
		super_block.nfree++;
	}

	
	savesuper_block();

	return 0;
}

//Function to set free blocks
int setfreeblocks(int startpos, int endpos) {
	super_block.free[0] = 0;
	super_block.nfree = 1;

	savesuper_block();

	int pointer;

	for(pointer = startpos; pointer <= endpos; pointer++) {
		writeblk(pointer);
	}

	return 0;
}

//initialize inodes
int setinodelist(int endpos) {
	char empty[sizeofblk] = {0};

	int offset = 2 * sizeofblk;
	int pointer;

	for(pointer = 2; pointer <= endpos; pointer++) {
		lseek(fd, offset, SEEK_SET);
		write(fd, empty, sizeofblk);
		offset += sizeofblk;
	}

	return 0;
}

//Function to fill contents of i-node
int fillinodes() {
	if(super_block.ninode == 100) {
		return 0;
	}

	int offset = 2 * sizeofblk;
	unsigned short flags;
	int iNum;
	int maxNum = super_block.isize * 16;

	for(iNum = 1; iNum <= maxNum; iNum++) {
		lseek(fd, offset, SEEK_SET);
		read(fd, &flags, 2);

		if(!(flags & inodealloc)) {
			super_block.inode[super_block.ninode] = iNum;
			super_block.ninode++;

			if(super_block.ninode == 100) {
				break;
			}
		}

		offset += sizeofinode;
	}

	if(super_block.ninode == 0) {
		return -1;
	}

	savesuper_block();

	return 0;
}


int readblk() {
	super_block.nfree--;

	if(super_block.free[super_block.nfree] == 0) {
		return -1;
	}

	int blkid;

	if(super_block.nfree == 0) {
		blkid = super_block.free[0];

		int offset = blkid * sizeofblk;

		lseek(fd, offset, SEEK_SET);
		read(fd, &super_block.nfree, 2);
		offset += 2;

		lseek(fd, offset, SEEK_SET);
		read(fd, &super_block.free, 2 * super_block.nfree);
	}
	else {
		blkid = super_block.free[super_block.nfree];
	}

	savesuper_block();

	char empty[sizeofblk] = {0};

	lseek(fd, blkid * sizeofblk, SEEK_SET);
	write(fd, empty, sizeofblk);

	return blkid;
}

//get the block address of the indirect block
int getblkaddr(int blkid, int num) {
	unsigned short res;

	lseek(fd, (blkid * sizeofblk) + (num * 2), SEEK_SET);
	read(fd, &res, 2);

	return res;
}

//set the block address of the indirect block
int setblkaddr(int blkid, int address, int num) {
	unsigned short buffer = (unsigned short)address;

	lseek(fd, (blkid * sizeofblk) + (num * 2), SEEK_SET);
	write(fd, &buffer, 2);

	return 0;
}

//write the V6 file
int writefile(int id_inode, void* buff, size_t size, int offset) {
	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	//load inode from the file
	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	unsigned long i_size;
	struct size25 size25_size;

	size25_size.lowbyte = inode.size1;
	size25_size.highbyte = inode.size0;
	size25_size.extrabit = (inode.flags & size25bit) ? 1 : 0;

	i_size = size25conv(size25_size);

	int blkid;

	if(offset + size > size25MAX_VALUE) {
		return -1;
	}

	
	if(!(inode.flags & largefile) && ((offset + size) / 512 > 7)) {
		blkid = readblk();
		if(blkid == -1) {
			//No free blocks
			return -1;
		}

		lseek(fd, blkid * sizeofblk, SEEK_SET);
		write(fd, inode.addr, 16);

		int i;
		for(i = 1; i < 8; i++) {
			inode.addr[i] = 0;
		}

		inode.addr[0] = blkid;

		inode.flags |= largefile;
	}

	int dataOffset;
	int countSize;
	int stepSize;
	int b, b1;
	int pointer;
	char* buffer = (char*)buff;
	int indirectblkid_1, indirectblkid_2;

	
	if(inode.flags & largefile) {
		countSize = 0;
		pointer = 0;

		while(countSize < size) {
			b = (offset + countSize) / 512;
			b1 = b / 256;

			if(b1 < 7) {
				indirectblkid_1 = inode.addr[b1];
				if(indirectblkid_1 == 0) {
					indirectblkid_1 = readblk();
					if(indirectblkid_1 == -1) {return -1;}
					inode.addr[b1] = indirectblkid_1;
				}

				blkid = getblkaddr(indirectblkid_1, b % 256);
				if(blkid == 0) {
					blkid = readblk();
					if(blkid == -1) {return -1;}
					setblkaddr(indirectblkid_1, blkid, b % 256);
				}
			}
			else {
				b1 = b1 - 7;

				indirectblkid_2 = inode.addr[7];
				if(indirectblkid_2 == 0) {
					indirectblkid_2 = readblk();
					if(indirectblkid_2 == -1) {return -1;}
					inode.addr[7] = indirectblkid_2;
				}

				indirectblkid_1 = getblkaddr(indirectblkid_2, b1);
				if(indirectblkid_1 == 0) {
					indirectblkid_1 = readblk();
					if(indirectblkid_1 == -1) {return -1;}
					setblkaddr(indirectblkid_2, indirectblkid_1, b1);
				}

				blkid = getblkaddr(indirectblkid_1, b % 256);
				if(blkid == 0) {
					blkid = readblk();
					if(blkid == -1) {return -1;}
					setblkaddr(indirectblkid_1, blkid, b % 256);
				}
			}

			dataOffset = blkid * sizeofblk + ((offset + countSize) % 512);
			stepSize = 512 - ((offset + countSize) % 512);
			countSize += stepSize;
			if(countSize > size) {
				stepSize -= countSize - size;
			}

			lseek(fd, dataOffset, SEEK_SET);
			write(fd, &buffer[pointer], stepSize);

			pointer += stepSize;
		}
	}
	
	else {
		countSize = 0;
		pointer = 0;

		while(countSize < size) {
			b = (offset + countSize) / 512;

			blkid = inode.addr[b];
			if(blkid == 0) {
				blkid = readblk();
				if(blkid == -1) {return -1;}
				inode.addr[b] = blkid;
			}

			dataOffset = blkid * sizeofblk + ((offset + countSize) % 512);
			stepSize = 512 - ((offset + countSize) % 512);
			countSize += stepSize;
			if(countSize > size) {
				stepSize -= countSize - size;
			}

			lseek(fd, dataOffset, SEEK_SET);
			write(fd, &buffer[pointer], stepSize);

			pointer += stepSize;
		}
	}

	if(i_size < offset + size) {
		i_size = offset + size;
		size25revconv(&size25_size, i_size);

		inode.size1 = size25_size.lowbyte;
		inode.size0 = size25_size.highbyte;
		inode.flags |= size25_size.extrabit ? size25bit : 0;
	}

	
	lseek(fd, inodeOffset, SEEK_SET);
	write(fd, &inode, sizeofinode);

	return 0;
}

//set the root directory
int setrootdir() {
	int offset = 2 * sizeofblk;

	struct inode inode = {0,0,0,0,0,0,{0},{0},{0}};

	inode.flags = 0140777;

	lseek(fd, offset, SEEK_SET);
	write(fd, &inode, sizeofinode);

	unsigned short id_inode = 1;

	//write the first and second element's id_inode
	writefile(1, &id_inode, 2, 0);
	writefile(1, &id_inode, 2, 16);
	//write the first and second element's name
	char str[14] = {0};
	sprintf(str, ".");
	writefile(1, str, 14, 2);
	sprintf(str, "..");
	writefile(1, str, 14, 18);

	return 0;
}

int writeinode(int id_inode) {
	char empty[sizeofinode] = {0};
	int offset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	lseek(fd, offset, SEEK_SET);
	write(fd, empty, sizeofinode);

	if(super_block.ninode < 100) {
		super_block.inode[super_block.ninode] = id_inode;
		super_block.ninode++;
	}

	
	savesuper_block();

	return 0;
}

int readinode() {
	if(super_block.ninode == 0 && fillinodes() == -1) {
		return -1;
	}

	int id_inode;

	super_block.ninode--;
	id_inode = super_block.inode[super_block.ninode];

	
	savesuper_block();

	return id_inode;
}

//Read V6 file
int readfile(int id_inode, void* buff, size_t size, int offset) {
	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	//load inode from the file
	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	unsigned long i_size;
	struct size25 size25_size;

	size25_size.lowbyte = inode.size1;
	size25_size.highbyte = inode.size0;
	size25_size.extrabit = (inode.flags & size25bit) ? 1 : 0;

	i_size = size25conv(size25_size);

	int blkid;
	char emptyFlag;
	int dataOffset;
	int countSize;
	int stepSize;
	int b, b1;
	int pointer;
	char* buffer = (char*)buff;
	int indirectblkid_1, indirectblkid_2;

	if(inode.flags & largefile) {
		emptyFlag = 0;
		countSize = 0;
		pointer = 0;

		while(countSize < size) {
			b = (offset + countSize) / 512;
			b1 = b / 256;

			if(b1 < 7) {
				indirectblkid_1 = inode.addr[b1];
				if(indirectblkid_1 == 0) {
					emptyFlag = 1;
				}
				else {
					blkid = getblkaddr(indirectblkid_1, b % 256);
					if(blkid == 0) {
						emptyFlag = 1;
					}
				}
			}
			else {
				b1 = b1 - 7;

				indirectblkid_2 = inode.addr[7];
				if(indirectblkid_2 == 0) {
					emptyFlag = 1;
				}
				else {
					indirectblkid_1 = getblkaddr(indirectblkid_2, b1);
					if(indirectblkid_1 == 0) {
						emptyFlag = 1;
					}
					else {
						blkid = getblkaddr(indirectblkid_1, b % 256);
						if(blkid == 0) {
							emptyFlag = 1;
						}
					}
				}
			}

			dataOffset = blkid * sizeofblk + ((offset + countSize) % 512);
			stepSize = 512 - ((offset + countSize) % 512);
			countSize += stepSize;
			if(countSize > size) {
				stepSize -= countSize - size;
			}

			if(emptyFlag) {
				int i;
				int endpos = stepSize + pointer;
				for(i = pointer; i < endpos; i++) {
					buffer[i] = 0;
				}
			}
			else {
				lseek(fd, dataOffset, SEEK_SET);
				read(fd, &buffer[pointer], stepSize);
			}

			pointer += stepSize;
		}
	}
	//read data from small file
	else {
		emptyFlag = 0;
		countSize = 0;
		pointer = 0;

		while(countSize < size) {
			b = (offset + countSize) / 512;

			blkid = inode.addr[b];
			if(blkid == 0) {
				emptyFlag = 1;
			}

			dataOffset = blkid * sizeofblk + ((offset + countSize) % 512);
			stepSize = 512 - ((offset + countSize) % 512);
			countSize += stepSize;
			if(countSize > size) {
				stepSize -= countSize - size;
			}

			if(emptyFlag) {
				int i;
				int endpos = stepSize + pointer;
				for(i = pointer; i < endpos; i++) {
					buffer[i] = 0;
				}
			}
			else {
				lseek(fd, dataOffset, SEEK_SET);
				read(fd, &buffer[pointer], stepSize);
			}

			pointer += stepSize;
		}
	}

	return 0;
}

//Find file by file name
int findfilename(int id_inode, char* str) {
	if(str[0] == 0 || id_inode < 1) {
		return -1;
	}

	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	if(!(inode.flags & 040000) || (inode.flags & 020000)) {
		return -1;
	}

	unsigned long i_size;
	struct size25 size25_size;

	size25_size.lowbyte = inode.size1;
	size25_size.highbyte = inode.size0;
	size25_size.extrabit = (inode.flags & size25bit) ? 1 : 0;

	i_size = size25conv(size25_size);

	char name_string[15] = {0};
	int pointer;
	char findFlag = 0;

	for(pointer = 2; pointer < i_size; pointer += 16) {
		readfile(id_inode, name_string, 14, pointer);
		if(strcmp(str,name_string) == 0) {
			findFlag = 1;
			break;
		}
	}

	if(!findFlag) {
		return -1;
	}

	unsigned short resid_inode;
	pointer -= 2;

	readfile(id_inode, &resid_inode, 2, pointer);

	return resid_inode;
}

//Find file directory
int findfiledir(char* str) {
	if(str[0] == 0) {
		return -1;
	}

	int id_inode;
	int pointer;

	if(str[0] == '/') {
		id_inode = 1;
		pointer = 1;
	}
	else {
		id_inode = curdirinode;
		pointer = 0;
	}

	char name_string[15] = {0};
	int p = 0;

	while(str[pointer] != 0 && p <= 14) {
		if(str[pointer] == '/') {
			name_string[p] = 0;
			id_inode = findfilename(id_inode, name_string);
			if(id_inode == -1) {
				return -1;
			}
			p = 0;
		}
		else {
			name_string[p] = str[pointer];
			p++;
		}

		pointer++;
	}

	if(p > 14) {
		return -1;
	}

	if(p != 0) {
		name_string[p] = 0;
		id_inode = findfilename(id_inode, name_string);
	}

	return id_inode;
}

int getsize(int id_inode) {
	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	//loading the file
	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	struct size25 size25_size;

	size25_size.lowbyte = inode.size1;
	size25_size.highbyte = inode.size0;
	size25_size.extrabit = (inode.flags & size25bit) ? 1 : 0;

	return size25conv(size25_size);
}

int getname(int id_inode, char* name) {
	if(id_inode == 1) {
		sprintf(name, "/");
		return 0;
	}

	unsigned short id_parent;

	readfile(id_inode, &id_parent, 2, 16);

	int size = getsize(id_parent);
	unsigned short temp;
	unsigned short pointer;

	for(pointer = 0; pointer < size; pointer += 16) {
		readfile(id_parent, &temp, 2, pointer);

		if(temp == id_inode) {
			break;
		}
	}

	readfile(id_parent, name, 14, pointer + 2);

	return 0;
}

//Update the current path
int getpath(int id_inode, char* path) {
	if(id_inode == 1) {
		sprintf(path, "/");
		return 0;
	}

	unsigned short id_parent;

	readfile(id_inode, &id_parent, 2, 16);

	getpath(id_parent, path);

	char name[15] = {0};

	getname(id_inode, name);

	if(id_parent != 1) {
		strcat(path, "/");
	}

	strcat(path, name);

	return 0;
}

//Change directory in V6 file system
int changeV6dir(char* str) {
	int id_inode;

	id_inode = findfiledir(str);

	if(id_inode == -1) {
		return -1;
	}

	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	//loading the file
	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	//checking for directory
	if(!(inode.flags & 040000) || (inode.flags & 020000)) {
		return -1;
	}

	curdirinode = id_inode;

	//update path to display
	getpath(curdirinode, dirpath);

	return 0;
}

//Create a new directory in file system
int dircreate(int id_inode, char* str) {
	if(str[0] == 0) {
		return -1;
	}

	if(namecheck(str) == -1) {
		return -1;
	}

	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	//loading the file
	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	
	if(!(inode.flags & 040000) || (inode.flags & 020000)) {
		return -1;
	}

	unsigned long i_size;
	struct size25 size25_size;

	size25_size.lowbyte = inode.size1;
	size25_size.highbyte = inode.size0;
	size25_size.extrabit = (inode.flags & size25bit) ? 1 : 0;

	i_size = size25conv(size25_size);

	char name_string[15] = {0};
	int pointer;
	char findFlag = 0;

	for(pointer = 2; pointer < i_size; pointer += 16) {
		readfile(id_inode, name_string, 14, pointer);
		if(strcmp(str,name_string) == 0) {
			findFlag = 1;
			break;
		}
	}

	if(findFlag) {
		return -1;
	}

	unsigned short newid_inode;

	newid_inode = readinode();

	if(newid_inode == -1) {
		return -1;
	}

	int offset = 2 * sizeofblk + (newid_inode - 1) * sizeofinode;
	struct inode newinode = {0,0,0,0,0,0,{0},{0},{0}};

	newinode.flags = 0140777;

	lseek(fd, offset, SEEK_SET);
	write(fd, &newinode, sizeofinode);

	writefile(newid_inode, &newid_inode, 2, 0);
	writefile(newid_inode, &id_inode, 2, 16);
	
	char dirStr[14] = {0};
	sprintf(dirStr, ".");
	writefile(newid_inode, dirStr, 14, 2);
	sprintf(dirStr, "..");
	writefile(newid_inode, dirStr, 14, 18);

	unsigned short temp;

	for(pointer = 0; pointer < i_size; pointer += 16) {
		readfile(id_inode, &temp, 2, pointer);
		if(temp == 0) {
			break;
		}
	}

	writefile(id_inode, &newid_inode, 2, pointer);
	sprintf(dirStr, "%s", str);
	writefile(id_inode, dirStr, 14, pointer + 2);

	return newid_inode;
}

//Creates a newfile
int filecreate(int id_inode, char* str) {
	if(str[0] == 0) {
		return -1;
	}

	if(namecheck(str) == -1) {
		return -1;
	}

	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;


	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	
	if(!(inode.flags & 040000) || (inode.flags & 020000)) {
		return -1;
	}

	unsigned long i_size;
	struct size25 size25_size;

	size25_size.lowbyte = inode.size1;
	size25_size.highbyte = inode.size0;
	size25_size.extrabit = (inode.flags & size25bit) ? 1 : 0;

	i_size = size25conv(size25_size);

	char name_string[15] = {0};
	int pointer;
	char findFlag = 0;

	for(pointer = 2; pointer < i_size; pointer += 16) {
		readfile(id_inode, name_string, 14, pointer);
		if(strcmp(str,name_string) == 0) {
			findFlag = 1;
			break;
		}
	}

	if(findFlag) {
		return -1;
	}

	unsigned short newid_inode;

	newid_inode = readinode();

	if(newid_inode == -1) {
		return -1;
	}

	int offset = 2 * sizeofblk + (newid_inode - 1) * sizeofinode;
	struct inode newinode = {0,0,0,0,0,0,{0},{0},{0}};

	newinode.flags = 0100777;

	lseek(fd, offset, SEEK_SET);
	write(fd, &newinode, sizeofinode);

	unsigned short temp;

	for(pointer = 0; pointer < i_size; pointer += 16) {
		readfile(id_inode, &temp, 2, pointer);
		if(temp == 0) {
			break;
		}
	}

	writefile(id_inode, &newid_inode, 2, pointer);
	sprintf(name_string, "%s", str);
	writefile(id_inode, name_string, 14, pointer + 2);

	return newid_inode;
}

//Removes V6 file
int rmV6file(int id_inode) {
	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	//load inode from the file
	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	unsigned long i_size = getsize(id_inode);

	int i, k;
	int countSize = 0;
	unsigned short blkid, indirectblkid;

	if((inode.flags & largefile) == largefile) {
		for(i = 0; i < 263; i++) {
			if(i < 7) {
				indirectblkid = inode.addr[i];
			}
			else if(inode.addr[7] != 0) {
				indirectblkid = getblkaddr(inode.addr[7], i - 7);
			}
			else {
				writeinode(id_inode);
				return 0;
			}

			if(indirectblkid != 0) {
				for(k = 0; k < 256; k++) {
					blkid = getblkaddr(indirectblkid, k);
					if(blkid != 0) {
						writeblk(blkid);
					}

					countSize += 512;

					if(countSize >= i_size) {
						if(inode.addr[7] != 0) {
							writeblk(inode.addr[7]);
						}
						writeblk(indirectblkid);
						writeinode(id_inode);
						return 0;
					}
				}

				writeblk(indirectblkid);
			}
			else {
				countSize += 131072;

				if(countSize >= i_size) {
					if(inode.addr[7] != 0) {
						writeblk(inode.addr[7]);
					}
					writeinode(id_inode);
					return 0;
				}
			}
		}
	}
	else {
		for(i = 0; i < 8; i++) {
			blkid = inode.addr[i];
			if(blkid != 0) {
				writeblk(blkid);
			}
		}
	}

	writeinode(id_inode);
	return 0;
}

//Determine filetype using flag combinations
int gettype(int id_inode) {
	struct inode inode;
	int inodeOffset = 2 * sizeofblk + (id_inode - 1) * sizeofinode;

	lseek(fd, inodeOffset, SEEK_SET);
	read(fd, &inode, sizeofinode);

	return inode.flags & filetype2bit;
}

int loadf(char* args[]) {
	if(fd != -1) {
		close(fd);
		fd = -1;
	}

	fd = open(args[1], O_CREAT | O_RDWR, filemode);
	
	if(fd != -1) {
		//load super block from the file
		loadsuper_block();
	}

	return fd;
}

//"initfs" Command
int initializefs(char* args[]) {
	if(fd == -1) {
		printf("no loaded file\n");
		return -1;
	}

	//Check this
	if(args[1] == NULL || args[2] == NULL) {
		printf("cannot find parameters\n");
		return -1;
	}

	int numOfBlock = parseInt(args[1]);
	int numOfinode = parseInt(args[2]);

	if(numOfBlock == 0 || numOfinode == 0 || numOfinode > numOfBlock * 16) {
		printf("incorrect parameters\n");
		return -1;
	}

	
	char emptyBlock[sizeofblk] = {0};
	lseek(fd, (numOfBlock - 1) * sizeofblk, SEEK_SET); //Go to last block
	write(fd, emptyBlock, sizeofblk); //Make the last block contents to zero

	//initialize super block
	super_block.isize = numOfinode / 16;
	super_block.isize += (super_block.isize * 16) >= numOfinode ? 0 : 1;
	super_block.fsize = numOfBlock;
	super_block.nfree = 0;
	super_block.ninode = 0;
	super_block.flock = 0;
	super_block.ilock = 0;
	super_block.fmod = 1;

	savesuper_block();

	setfreeblocks(super_block.isize + 2, super_block.fsize - 1);

	setinodelist(super_block.isize + 1);

	setrootdir();

	fillinodes();

	printf("File system initialization successful\n");

	return 0;
}

//"cpin" Command
int copyin(char* args[]) {
	if(args[1] == NULL) {
		printf("cannot find parameter\n");
		return -1;
	}

	int externalfd = -1;

	externalfd = open(args[1], O_RDONLY, filemode);

	if(externalfd == -1) {
		printf("fail to find external file\n");
		return -1;
	}

	int id_inode;

	id_inode = filecreate(curdirinode, args[2]);

	if(id_inode == -1) {
		printf("fail to create V6 file\n");
		return -1;
	}

	int size;
	size = lseek(externalfd, 0, SEEK_END);
	int countSize = 0;
	int stepSize = 512;
	char buffer[512] = {0};

	while(size > countSize) {
		if(stepSize + countSize > size) {
			stepSize = size - countSize;
		}

		lseek(externalfd, countSize, SEEK_SET);
		read(externalfd, buffer, stepSize);
		writefile(id_inode, buffer, stepSize, countSize);

		countSize += stepSize;
	}

	close(externalfd);

	printf("File copy successful\n");

	return 0;
}

//"cpout" Command
int copyout(char* args[]) {
	if(args[1] == NULL) {
		printf("cannot find parameter\n");
		return -1;
	}

	int id_inode;

	id_inode = findfilename(curdirinode, args[1]);

	if(id_inode == -1) {
		printf("fail to find V6 file\n");
		return -1;
	}

	int externalfd = -1;

	char name_string[20] = {0};

	sprintf(name_string, "%s", args[2]);

	externalfd = creat(name_string, filemode);

	if(externalfd == -1) {
		printf("fail to create external file\n");
		return -1;
	}

	int size;
	size = getsize(id_inode);
	int countSize = 0;
	int stepSize = 512;
	char buffer[512] = {0};

	while(size > countSize) {
		if(stepSize + countSize > size) {
			stepSize = size - countSize;
		}

		readfile(id_inode, buffer, stepSize, countSize);
		lseek(externalfd, countSize, SEEK_SET);
		write(externalfd, buffer, stepSize);

		countSize += stepSize;
	}

	close(externalfd);

	printf("File exported to external system\n");

	return 0;
}

//"mkdir" Command
int makedir(char* args[]) {
	if(args[1] != NULL) {
		if(dircreate(curdirinode, args[1]) == -1) {
			printf("fail to create directory\n");
		}
	}
	else {
		printf("cannot find parameter\n");
		return -1;
	}
}
int list();
//"rm" command
int rmfile(char* args[]) {
	if(args[1] == NULL) {
		printf("fail to find argument\n");
		return -1;
	}

	char* str = args[1];
	int i_size = getsize(curdirinode);
	char name_string[15] = {0};
	int pointer;
	char findFlag = 0;

	for(pointer = 2; pointer < i_size; pointer += 16) {
		readfile(curdirinode, name_string, 14, pointer);
		if(strcmp(str,name_string) == 0) {
			findFlag = 1;
			break;
		}
	}

	if(!findFlag) {
		printf("fail to find the file or directory\n");
		return -1;
	}

	unsigned short resid_inode;
	pointer -= 2;

	readfile(curdirinode, &resid_inode, 2, pointer);
	char empty[16] = {0};
	
	if(gettype(resid_inode) == directory) {
		printf("Cannot Remove directory\n");
	}
	else {
		writefile(curdirinode, empty, 16, pointer);
		rmV6file(resid_inode);
	}

	return 0;
}

//Helper function for quit command
int closef(char* args[]) {
	close(fd);

	fd = -1;

	return 0;
}

//"q" command
int quit(char* args[]) {
	closef(args);
	exit(0);
}

//"cd" command
int changedir(char* args[]) {
	if(args[1] != NULL) {
		if(changeV6dir(args[1]) == -1) {
			printf("wrong directory\n");
		}
	}
	else {
		printf("cannot find parameter\n");
		return -1;
	}
}

//"ls" command
int list() {
	int size;

	size = getsize(curdirinode);

	char name_string[15] = {0};
	unsigned short id_inode;
	int pointer;

	for(pointer = 0; pointer < size; pointer += 16) {
		readfile(curdirinode, &id_inode, 2, pointer);

		if(id_inode == 0) {
			continue;
		}

		readfile(curdirinode, name_string, 14, pointer + 2);
		printf("%d\t%s\n", id_inode, name_string);
	}

	return 0;
}

//"help command"
int help(){

	printf("List of available commands:\n");
    printf("initfs <Number of blocks> <Number of i-nodes>: Initialize the file system\n");
    printf("cpin <externalfile> <v6-file>: Copy an externalfile into file system\n");
    printf("cpout <v6-file> <externalfile>: Copy V6 File into external system\n");
    printf("mkdir <v6-directory>: Create a new directory\n");
    printf("rm <v6-file>: Remove a file from file system\n");
    printf("q: save all changes and exit\n");
    printf("help: Displays available commands\n");

    return 0;
}

//"Main" Subroutine
int main(int args, char* argv[]) {
	char* inp;
	char** arg;
	printf("V6 Based File System \n");

	int ld = loadf(argv);
	int temp = 0;

	if(args > 2) {
		printf("Too Many Arguments\n");
		return -1;
	}
	else if(ld == -1){
		printf("Unable to load provided argument\n"); 
	}

	dirpath[0] = '/';

    while(1) {
    	printf(">~%s$: ", dirpath);//Constantly displays the current path
    	inp = scaninput();
    	arg = arguments(inp);


    	if(strcmp("initfs",arg[0]) == 0)
    	{
    		temp = initializefs(arg);
    	}
    	else if(strcmp("cpin",arg[0]) == 0)
    	{
    		temp = copyin(arg);
    	}
    	else if(strcmp("cpout",arg[0]) == 0)
    	{
    		temp = copyout(arg);
    	}
    	else if(strcmp("mkdir",arg[0]) == 0)
    	{
    		temp = makedir(arg);
    	}
    	else if(strcmp("rm",arg[0]) == 0)
    	{
    		temp = rmfile(arg);
    	}
    	else if(strcmp("q",arg[0]) == 0)
    	{
    		temp = quit(arg);
    	}
    	else if(strcmp("cd",arg[0]) == 0)
    	{
    		temp = changedir(arg);
    	}
    	else if(strcmp("ls",arg[0]) == 0)
    	{
    		temp = list();
    	}
    	else if(strcmp("help",arg[0]) == 0)
    	{
    		
    		temp = help();
    	}
    	else
    	{
    		printf("Invalid command\n");
    		temp = help();
    	}

    }

    return 0;
}