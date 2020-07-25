Forked from : https://github.com/rxi/microtar

Sample usages:
- Reading file
```
mtar_t tar;
mtar_header_t h;
char *p;

/* Open archive for reading */
mtar_open(&tar, "test.tar", "r");

/* Print all file names and sizes */
while ( (mtar_read_header(&tar, &h)) != MTAR_ENULLRECORD ) {
  printf("%s (%d bytes)\n", h.name, h.size);
  mtar_next(&tar);
}

/* Load and print contents of file "test.txt" */
mtar_find(&tar, "test.txt", &h);
p = calloc(1, h.size + 1);
mtar_read_data(&tar, p, h.size);
printf("%s", p);
free(p);

/* Close archive */
mtar_close(&tar);
```
- Reading in memmory stream
```
/* struct to hold MemStream for tarball reading*/
struct MemStream{
	char* beg;
	char* buff;
};

int mem_read(mtar_t *tar, void *data, unsigned size){
	MemStream* ss = (MemStream*) tar->stream;
	memcpy(data,ss->buff,size);
	return MTAR_ESUCCESS;
}
int mem_seek(mtar_t *tar, unsigned pos){
	MemStream* ss = (MemStream*) tar->stream;
	ss->buff = ss->beg + pos;
	return MTAR_ESUCCESS;
}
int mem_close(mtar_t *tar){
	return MTAR_ESUCCESS;
}

/* Read tarball */
void readTar(std::string destination, char* source){
	mtar_t* tar = new mtar_t();
	tar->read = mem_read;
	tar->seek = mem_seek;
	tar->close = mem_close;

	MemStream* memStream = new MemStream();
	memStream->beg = source;
	memStream->buff = source;
	tar->stream = (void*)memStream;

	mtar_header_t h;
	std::cout<<"source: "<<source<<std::endl;
	while((mtar_read_header(tar, &h)) != MTAR_ENULLRECORD ) {
		std::cout << "h.name: "<<h.name<<std::endl;
		mtar_next(tar);
	}
}
```
- Writing file
 ```
 mtar_t tar;
const char *str1 = "Hello world";
const char *str2 = "Goodbye world";

/* Open archive for writing */
mtar_open(&tar, "test.tar", "w");

/* Write strings to files `test1.txt` and `test2.txt` */
mtar_write_file_header(&tar, "test1.txt", strlen(str1));
mtar_write_data(&tar, str1, strlen(str1));
mtar_write_file_header(&tar, "test2.txt", strlen(str2));
mtar_write_data(&tar, str2, strlen(str2));

/* Finalize -- this needs to be the last thing done before closing */
mtar_finalize(&tar);

/* Close archive */
mtar_close(&tar);
 ```
 
