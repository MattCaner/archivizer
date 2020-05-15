#include"archivizer.h"

void pack(const char* archivePath, const char* directoryPath){

	//time control for the user's information:
	clock_t start;
	clock_t end;
	double cpu_time;
	start = clock();

	//creating archive file:
	int archiveFD = open(archivePath,O_RDWR | O_CREAT,0666);
	
	//the 'proper' call:
	packDirectory(archiveFD,directoryPath,directoryPath);

	//after the packing was successfull:
	printf("Packed successfully.\n");
	//setting privileges to the archive:
	mode_t mod;
	mod = S_IRWXU;
	chmod(archivePath,mod);
	close(archiveFD);

	//printing time:
	end = clock();
	cpu_time = ((double)(end-start)) / CLOCKS_PER_SEC;
	printf("Performed in %f seconds.\n",cpu_time);

	return;
}

void unpack(const char* archivePath, const char* directoryPath){
	
	//time control for the user's information:
	clock_t start;
	clock_t end;
	double cpu_time;
	start = clock();

	//opening the archive:
	int archiveFD = open(archivePath,O_RDONLY);
	//doesn't exist?
	if(archiveFD<=0){
		perror("The archive file cannot be opened! Does it exist?");
		return;
	}

	//the 'proper' call
	unpackDirectory(archiveFD,directoryPath,directoryPath);
	printf("Unpacked successfully.\n");
	close(archiveFD);

	//printing time:
	end = clock();
	cpu_time = ((double)(end-start)) / CLOCKS_PER_SEC;
	printf("Performed in %f seconds.\n",cpu_time);

	return;
}

void packDirectory(int archiveFD, const char* path, const char* name){

	printf("Packing directory %s\n",path);

	int entity_count = 0;

	struct finfo dirinfo;
	dirinfo.content_size = 0;
	dirinfo.type = 1;
	dirinfo.name_size = strlen(name);

	struct stat dfs;
	stat(path,&dfs);
	
	DIR* d = opendir(path);
	//doesn't exist?
	if(d==NULL){
		perror("The directory set for being archived cannot be opened! Does it exist or do you have permission to read it?");
		return;
	}
	struct dirent* entry;
	entry = readdir(d);

	//writing metadata about the directory:
	void* dirdatabuffer = malloc(sizeof(struct finfo));
	memcpy(dirdatabuffer,(void*)&dirinfo,sizeof(struct finfo));
	write(archiveFD,dirdatabuffer,sizeof(struct finfo));
	write(archiveFD,(void*)name,strlen(name)*sizeof(char));


	free(dirdatabuffer);
	
	while(entry!=NULL){

		entity_count++;

		if(entry->d_type==DT_REG){

			//if the file is a regular file, put it into the archive byte by byte,
			//but first, its metadata needs to be preserved

			char* buffer = calloc(sizeof(char),1024);
			strcpy(buffer,path);
			strcat(buffer,"/");
			strcat(buffer,entry->d_name);
			int fileFD = open(buffer,O_RDONLY);
			if(fileFD<=0){
				perror("Error: cannot open a file!");
			}
			printf("Packing file %s...",buffer);
			long size = lseek(fileFD,0,SEEK_END);
			lseek(fileFD,0,SEEK_SET);
			free(buffer);

			char* name_buffer = malloc(sizeof(char)*1024);
			strcpy(name_buffer,entry->d_name);

			struct stat fs;
			fstat(fileFD,&fs);

			void* data_buffer = malloc(1024);
			//long blocks = size/buffer_size;
			//int bytes_read;
			int status;
			struct finfo* fi = malloc(sizeof(struct finfo));
			fi->content_size = size;
			fi->type = 0;
			fi->name_size = strlen(name_buffer);
			fi->permissions = fs.st_mode;
			//void* data_buffer = malloc(sizeof(struct finfo));
			memcpy(data_buffer,(void*)fi,sizeof(struct finfo));
			status = write(archiveFD,data_buffer,sizeof(struct finfo));
			if(status!=sizeof(struct finfo)) perror("Error writing data!\n");

			memcpy(data_buffer,name_buffer,sizeof(char)*fi->name_size);

			memcpy(data_buffer,entry->d_name,sizeof(struct finfo));
			status = write(archiveFD,data_buffer,sizeof(char)*fi->name_size);
			if(status!=sizeof(char)*fi->name_size) perror("Error writing data!\n");

			//the 'proper' copying begins here:
			long archive_size = lseek(archiveFD,0,SEEK_END);
			lseek(archiveFD,0,SEEK_SET);

			//printf("archive size, calc: %ld\n",archive_size);

			if(ftruncate(archiveFD,archive_size+size) == -1) perror("truncating error");

			void* mmaped_origin;
			void* mmaped_destination;

			mmaped_origin = mmap(NULL,size,PROT_READ,MAP_SHARED,fileFD,0);
			mmaped_destination = mmap(NULL,archive_size+size,PROT_WRITE|PROT_READ,MAP_SHARED,archiveFD,0);
			
			void* relocated_dest = (char*)mmaped_destination + archive_size;

			memcpy(relocated_dest,mmaped_origin,size);
			//printf("went here\n");

			munmap(mmaped_origin,size);
			munmap(mmaped_destination,archive_size+size);

			printf(" packed %ld bytes.\n",size);

			free(data_buffer);
			free(fi);
			close(fileFD);
			free(name_buffer);

			lseek(archiveFD,0,SEEK_END);

		}
		else if(entry->d_type == DT_DIR){
			//if the file is a directory, the function is called recursively:
			if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")){
				char* buffer = malloc(sizeof(char)*1024);
				strcpy(buffer,path);
				strcat(buffer,"/");
				strcat(buffer,entry->d_name);
				packDirectory(archiveFD,buffer,entry->d_name);
				free(buffer);
			}
		}
		entry = readdir(d);
	}

	//placing the information that this is the end of the directory
	dirinfo.content_size = entity_count;
	dirinfo.type = 2;
	dirinfo.name_size = 0;

	dirdatabuffer = malloc(sizeof(struct finfo));

	memcpy(dirdatabuffer,(void*)&dirinfo,sizeof(struct finfo));
	write(archiveFD,dirdatabuffer,sizeof(struct finfo));
	free(dirdatabuffer);
	closedir(d);

}


void unpackDirectory(int archiveFD, const char* path, const char* name){

	//create dicrectory:
	int dirval = mkdir(path,S_IRWXU | O_CREAT);
	if(dirval!=0){
		perror("Error creating a directory! Check the quota and writing permissions!");
	}
	printf("Unpacking directory: %s\n",path);

	//the checker checks the metadata type of the file.
	int checker = 0;

	while(checker<2){

		char* namesBuffer1 = calloc(sizeof(char),1024);
		char* namesBuffer2 = calloc(sizeof(char),1024);

		//retrieving metadata:
		struct finfo metadata;
		int read_bytes = read(archiveFD,(void*)&metadata,sizeof(struct finfo));
		checker = metadata.type;
		if(read_bytes==0) {
			//nothing more to read:
			printf("End of the archive reached\n");
			free(namesBuffer1);
			free(namesBuffer2);
			break;
		}

		//unpacking:
		switch(checker){
			case 0:
				read(archiveFD,(void*)namesBuffer2,(metadata.name_size)*sizeof(char));
				strcpy(namesBuffer1,path);
				strcat(namesBuffer1,"/");
				strcat(namesBuffer1,namesBuffer2);

				printf("Unpacking file %s...",namesBuffer1);

				int fileFD = open(namesBuffer1,O_RDWR|O_CREAT,0666);
				if(fileFD<=0){
					perror("Error unpacking a file! Check the quota and writing permissions!");
					exit(-1);
				}

				long size = metadata.content_size;

				//unpacking a file:

				long archive_position = lseek(archiveFD,0,SEEK_CUR);
				long archive_size = lseek(archiveFD,0,SEEK_END);
				//setting archiveFD to beginning:
				lseek(archiveFD,0,SEEK_SET);

				if(ftruncate(fileFD,size) == -1) perror("truncating error");

				void* mmaped_origin;
				void* mmaped_destination;

				mmaped_origin = mmap(NULL,archive_size,PROT_READ,MAP_SHARED,archiveFD,0);
				mmaped_destination = mmap(NULL,size,PROT_WRITE|PROT_READ,MAP_SHARED,fileFD,0);
				
				void* relocated_origin = (char*)mmaped_origin + archive_position;

				memcpy(mmaped_destination,relocated_origin,size);

				munmap(mmaped_origin,size);
				munmap(mmaped_destination,archive_size+size);

				lseek(archiveFD,archive_position+size,SEEK_SET);
				chmod(namesBuffer1,metadata.permissions);
				close(fileFD);

				printf(" unpacked %ld bytes of data\n",size);		
			break;
			case 1:
				//unpacking a directory:
				read(archiveFD,(void*)namesBuffer2,metadata.name_size*sizeof(char));
				strcpy(namesBuffer1,path);
				strcat(namesBuffer1,"/");
				strcat(namesBuffer1,namesBuffer2);
				unpackDirectory(archiveFD,namesBuffer1,namesBuffer2);

			break;
			case 2:
				printf("Directory unpacked.\n");
				free(namesBuffer1);
				free(namesBuffer2);
				return;
			break;
			default:
				perror("Error - unrecognized metadata type! Is the file a correct archive file?");
				free(namesBuffer1);
				free(namesBuffer2);
				return;
			break;
		}
		free(namesBuffer1);
		free(namesBuffer2);
	}

}
