// Matthew Rutigliano
// pzip.cpp
// Modified from Dr. Zhu's "wzip.cpp"
// February 13th, 2021

#include <iostream>
#include <fstream>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

using namespace std;

void* reader(void*);

struct read_arg {
	char* addr; 		// File Location
	char* cbuf;			// Character Buffer
	int* ibuf;			// Integer Buffer
	int* START_ARR;		// Array of job boundaries
	int* BUF_START;		// Array of buffer boundaries
	int jnum;			// Job number
};

// Matching threads to the number of available CPU cores was excessive for my method
// of creating jobs, so I stuck with the default. Increasing this number could increase
// performance, but with diminishing returns.
int JOB_COUNT = 5;

int main(int argc, char* argv[]) {
	
	// Check Arguments
	if (argc < 2) {
		cout <<  "pzip file1 [file2 ...]\n";
		exit(1);
	}
	
	// Reading Files in
	int fsize;
	char* addr;
	for (int i=1; i<argc; i++){
		
		// Getting size of file
		ifstream myfile(argv[i], ifstream::in);
		int begin = myfile.tellg();
		myfile.seekg (0, ios::end);
		int end = myfile.tellg();
		fsize = (end-begin);
		myfile.close();
		
		// Mapping file to address space
		int fd = open(argv[i], 0);
		addr = (char*) mmap(NULL, (size_t) fsize, PROT_READ, MAP_PRIVATE, fd, 0);
		close(fd);
		
		// Creating Buffers
		char* cbuf = new char[fsize*2];
		int* ibuf = new int[fsize*2];
		
		// Creating job boundaries
		int* bound = new int[JOB_COUNT];
		int* bufBound = new int[JOB_COUNT];
		for(int i=0; i<JOB_COUNT; i++){
			bound[i] = (int) floor((i+1) * fsize / JOB_COUNT);
			
			// Check that boundary isn't in the middle of character run
			while (addr[bound[i]] ==  addr[bound[i]-1])
				bound[i]--;
			
			// Add space to buffer boundaries for terminating character
			bufBound[i] = bound[i] + (i+1);
			
			// Reduce jobs and redo if overlap
			if (i > 0 && bound[i] == bound[i-1]){
				i--;
				JOB_COUNT--;
			}
		}
		
		// Create Threads
		pthread_t p[JOB_COUNT];
		read_arg args[JOB_COUNT];
		for(int i=0; i<JOB_COUNT; i++){
			args[i].addr = addr;
			args[i].cbuf = cbuf;
			args[i].ibuf = ibuf;
			args[i].START_ARR = bound;
			args[i].jnum = i;
			args[i].BUF_START = bufBound;
			
			if (pthread_create(&p[i], NULL, reader, &args[i]))
				exit(1);
		}
		
		// Waiting for threads to complete
		int ret;
		for(int i=0; i<JOB_COUNT; i++){
			pthread_join(p[i], (void**) &ret);
		}
		
		// Printing final result
		int cur = 0;
		while(cbuf[cur]){
			cout.write((char*) &ibuf[cur], sizeof(int));
			cout.write((char*) &cbuf[cur], 1);
			cur++;
		}
		for(int i=0; i<JOB_COUNT-1; i++){
			cur = bufBound[i];
			while(cbuf[cur]){
				cout.write((char*) &ibuf[cur], sizeof(int));
				cout.write((char*) &cbuf[cur], 1);
				cur++;
			}
		}
		
		munmap(addr, fsize);
		delete [] ibuf;
		delete [] cbuf;
		delete [] bound;
		delete [] bufBound;
	}
	return 0;
}

void* reader(void* arg){
	read_arg* args = (read_arg*) arg;
	char c, last;
	int count, cur, start;
	
	// Default boundaries
	c = args->addr[0];
	cur = 0;
	start = 0;
	
	// Reads boundaries if not first job
	if (args->jnum > 0){
		cur = args->BUF_START[args->jnum-1];
		start = args->START_ARR[args->jnum-1];
		c = args->addr[start];
	}
		
	// Begin compression
	count = 0;
	for(int i=start; i<args->START_ARR[args->jnum]; i++) {
		c = args->addr[i];
		if (count && c != last) {
			args->cbuf[cur] = last;
			args->ibuf[cur] = count;
			cur++;
			count = 0;
		}
		last = c;
		count++;
	}
	if (count) {
		args->cbuf[cur] = last;
		args->ibuf[cur] = count;
		cur++;
	}
	args->cbuf[cur] = '\0';
	
	return (void*) 0;
}