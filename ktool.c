#include "kt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void printUsageThenExit(){	
	printf(
		"Usage:\n"
		"  ktool -L -k aws_key -i aws_key_id -r region -e endpoint [-t session_token]\n"
		"  ktool -D -k aws_key -i aws_key_id -r region -e endpoint [-t session_token]\n"
		"        -s stream_name\n"
		"  ktool -P -k aws_key -i aws_key_id -r region -e endpoint [-t session_token]\n"
		"        -s stream_name -p partition_key [-f filename] [-x text]\n\n"
		"  List Kinesis streams, describe a Kinesis stream or put data onto a Kinesis\n"
		"  stream from file and/or text on the command line. Provide a session_token\n"
		"  if using temporary AWS credentials. Specify a single -f or -x option to\n"
		"  make ktool to use the single record action 'PutRecord' otherwise\n"
		"  'PutRecords' will be used.\n\n"
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

	char *key=NULL, *keyId=NULL, *sessionToken=NULL, *region=NULL, *endpoint=NULL, *streamName=NULL;
	char action=0;
	int opt;
	
	char *filenames[255], *strings[255], *partitionKeys[255];
	int filenameCount=0, stringCount=0, partitionKeyCount=0;
	
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
				filenames[filenameCount++] = optarg;
				break;
			case 'x':
				strings[stringCount++] = optarg;
				break;
			case 'p':
				partitionKeys[partitionKeyCount++] = optarg;
				break;

			default:
				printUsageThenExit();
		}
	}

	/* ensure we have the right parameters for all actions */
	if(key == NULL || keyId == NULL || region == NULL || endpoint == NULL)
		printUsageThenExit();

	/* test parameters for describe */
	if(action == 'D' && streamName == NULL)
		printUsageThenExit();
	
	/* test parameters for PutRecord(s) */
	if(action == 'P' && (streamName == NULL || partitionKeyCount == 0 || filenameCount + stringCount == 0))
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

	/* single record from command line*/
	if(action == 'P' && stringCount == 1 && filenameCount == 0)
		retcode = ktPutRecord(ctx, streamName, partitionKeys[0], strings[0], strlen(strings[0]), &respHeader, &respBody, errorMsg);

	/* single record from file*/
	if(action == 'P' && stringCount == 0 && filenameCount == 1){
		int len;
		unsigned char *data = readFile(filenames[0], &len);
		retcode = ktPutRecord(ctx, streamName, partitionKeys[0], data, len, &respHeader, &respBody, errorMsg);
		free(data);
	}

	/* multiple records */
	if(action == 'P' && stringCount + filenameCount > 1){

		/* create data structures to pass to ktPutRecords */
		int recordCount = stringCount + filenameCount;
		char **partitionKeyArray = malloc(recordCount * sizeof(char*));
		unsigned char **dataArray = malloc(recordCount * sizeof(unsigned char*));
		int *lenArray = malloc(recordCount * sizeof(int));
		
		int i;
		/* add command line blobs */
		for(i=0;i<stringCount;i++){
			
			dataArray[i]=strings[i];
			lenArray[i]=strlen(strings[i]);
		}
		
		/* add file records */
		for(i=0;i<filenameCount;i++){
			
			int len;
			dataArray[i+stringCount]=readFile(filenames[i], &len);
			lenArray[i+stringCount]=len;
		}
		
		/* add partition keys */
		for(i=0;i<recordCount;i++){
			
			/* reuse the last partition key if there aren't enough */
			if(i<partitionKeyCount)
				partitionKeyArray[i]=partitionKeys[i];
			else
				partitionKeyArray[i]=partitionKeys[partitionKeyCount-1];
		}
		
		retcode = ktPutRecords(ctx, streamName, recordCount, partitionKeyArray, dataArray, lenArray, &respHeader, &respBody, errorMsg);
		
		/* free memory allocated for file records */
		for(i=stringCount; i<recordCount; i++)
			free(dataArray[i]);
		
		/* cleanup */
		free(partitionKeyArray);
		free(dataArray);
		free(lenArray);
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


