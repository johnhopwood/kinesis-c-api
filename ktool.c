#include "kt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void printUsageThenExit(){	
	printf(
		"Usage: kt -L -k aws_key -i aws_key_id -r region -e endpoint [-t session_token]\n"
		"       kt -D -k aws_key -i aws_key_id -r region -e endpoint [-t session_token]\n"
		"         -s stream_name\n"
		"       kt -P -k aws_key -i aws_key_id -r region -e endpoint [-t session_token]\n"
		"         -s stream_name -p partition_key [-f filename] [-x text]\n\n"
		"List Kinesis streams, describe a Kinesis stream or put data onto a Kinesis\n"
		"stream from either a file or as text on the command line. Specify\n"
		"a session_token if using temporary AWS credentials.\n\n"
		);
	
	exit(1);
}

/* Simple binary file to buffer util. Caller frees returned buffer */
unsigned char *readFile(const char *fname, int *len){

	/* Open file */
	FILE *file = fopen(fname, "r");
	if (!file) {
		fprintf(stderr, "Cannot open file %s\n", fname); 
		exit(1);
	}
	
	/* Get size */
	fseek(file, 0, SEEK_END);
	*len=ftell(file);
	fseek(file, 0, SEEK_SET);

	/* allocate buffer */
	unsigned char *buffer=(unsigned char *)malloc(*len);
	if (!buffer){
		fprintf(stderr, "Cannot malloc buffer\n");
		exit(1);
	}

	/* read file into buffer */
	fread(buffer, *len, 1, file);
	fclose(file);
	
	return buffer;
}

int main(int argc, char **argv){

	char *key=NULL, *keyId=NULL, *sessionToken=NULL, *region=NULL, *endpoint=NULL, *streamName=NULL, *filename=NULL, *partitionKey=NULL, *text=NULL;
	char action=0;
	int opt;
	
	/* parse command line */
	while ((opt = getopt(argc, argv,"PLDk:i:t:r:e:s:f:x:p:")) != -1){
		switch (opt){
			case 'P': /* put record */
			case 'L': /* list streams */
			case 'D': /* describe stream */
				action = opt;
				break;
			case 'k':
				key = optarg;
				break;
			case 'i':
				keyId = optarg;
				break;
			case 't':
				sessionToken = optarg;
				break;
			case 'r':
				region = optarg;
				break;
			case 'e':
				endpoint = optarg;
				break;
			case 's':
				streamName = optarg;
				break;
			case 'f':
				filename = optarg;
				break;
			case 'x':
				text = optarg;
				break;
			case 'p':
				partitionKey = optarg;
				break;

			default:
				printUsageThenExit();
		}
	}

	/* ensure we have the right parameters for the requested action */
	if(key == NULL || keyId == NULL || region == NULL || endpoint == NULL)
		printUsageThenExit();
	
	if(action == 'P' && (streamName == NULL || partitionKey == NULL || (filename == NULL && text == NULL)))
		printUsageThenExit();
	
	if(action == 'D' && streamName == NULL)
		printUsageThenExit();
	
	/* make a context object */
	AWSContext* ctx = ktMakeAWSContext(key, keyId, sessionToken, region, endpoint);
	
	/* create response and error buffers */
	httpResponse respHeader, respBody;
	char errorMsg[256];
	int retcode;
	
	/* do requested action */
	if(action == 'L')
		retcode = ktListStreams(ctx, &respHeader, &respBody, errorMsg);
	
	if(action == 'D')
		retcode = ktDescribeStream(ctx, streamName, &respHeader, &respBody, errorMsg);

	if(action == 'P'){
		
		if(text != NULL)
			/* data from command line */
			retcode = ktPutRecord(ctx, streamName, partitionKey, text, strlen(text), &respHeader, &respBody, errorMsg);
		else {
			/* data from file */
			int len;
			unsigned char *data = readFile(filename, &len);
			retcode = ktPutRecord(ctx, streamName, partitionKey, data, len, &respHeader, &respBody, errorMsg);
			free(data);
		}
	}
	
	/* interpret result */
	if(retcode == 0)
		fprintf(stderr, "%s\n", errorMsg);
	else if(retcode == 200)
		fprintf(stderr, "%s\n", respBody.text);
	else
		fprintf(stderr, "%s\n%s\n", respHeader.text, respBody.text);
	

	/* cleanup */
	ktFreeAWSContext(ctx);
}


