#kinesis-c-api

###About
Tiny footprint, thread safe C API for posting data to AWS Kinesis, perfect for embedded and other resource constrained devices. Uses permanent or temporary AWS credentials. Includes ktool, a command line tool built using the C API. The library originally implemented the `PutRecord` action only, however `ListStreams` and `DescribeStream` were added to demonstrate extensibility (See Extending below). Turns out these are useful to have for testing connectivity / credential validity.

###Dependencies
[OpenSSL](https://www.openssl.org/) for the two hash functions required to calculate AWS Signature version 4 (`SHA256` and `HMAC-SHA256`) and [libcurl](http://curl.haxx.se/libcurl/) for HTTPS transport layer. All Curl specific code is isolated in functions `curlDoPost` and `curlResponseCallback` in case this needs to be replaced.
Headers and libraries for both packages should be available on your *nix platform as libssl-dev and libcurl-dev or similar.

###Documentation
See kt.h.

###Example
```C
/* Make a context object. Set sessionToken for temporary credentials, otherwise set to NULL */
AWSContext* ctx = ktMakeAWSContext("AWSKEY", "AWSKEYID", "AWSSESSIONTOKEN",  "us-east-1", "kinesis.us-east-1.amazonaws.com");
	
/* create response and error buffers if required */
httpResponse respHeader, respBody;
char errorMsg[256];

/* Post some data */
char *data = "my-data-blob";
int retcode = ktPutRecord(ctx, "my-test-kinesis-stream", "partition-key", data, strlen(data), &respHeader, &respBody, errorMsg);

/* interpret result */
if(retcode == 0)
	printf("%s\n", errorMsg);
else if(retcode == 200)
	printf("%s\n", respBody.text);
else
	printf("%s\n%s\n", respHeader.text, respBody.text);
	
/* cleanup */
ktFreeAWSContext(ctx);
```
See ktool.c for a simple command line tool built using the API.

###Ktool examples
```sh
$ # list streams
$ ktool -L -i "FAKE-AWS-KEYID" -k "FAKE-AWS-KEY" -r "us-east-1" -e "kinesis.us-east-1.amazonaws.com"
$ # describe stream "my-test-kinesis-stream"
$ ktool -D -i "FAKE-AWS-KEYID" -k "FAKE-AWS-KEY" -r "us-east-1" -e "kinesis.us-east-1.amazonaws.com" -s "my-test-kinesis-stream"
$ # put file "filename" on stream "my-test-kinesis-stream"
$ ktool -P -i "FAKE-AWS-KEYID" -k "FAKE-AWS-KEY" -r "us-east-1" -e "kinesis.us-east-1.amazonaws.com" -s "my-test-kinesis-stream" -p "partition-key" -f filename
$ # put "a blob of text" on stream "my-test-kinesis-stream" without a partition key
$ ktool -P -i "FAKE-AWS-KEYID" -k "FAKE-AWS-KEY" -r "us-east-1" -e "kinesis.us-east-1.amazonaws.com" -s "my-test-kinesis-stream" -x "a blob of text"
```

###Extending
`ListStreams`, `DescribeStream`, `PutRecord` are currently implemented. To implement `NewAction`, code the relevant `ktNewAction` and `makeNewActionPayload` functions using existing function pairs as a guide.

###Notes
For multi threaded use, curl requires `curl_global_init(CURL_GLOBAL_DEFAULT)` to be called before any other threads are created.