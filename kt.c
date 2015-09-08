#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <curl/curl.h>
#include "kt.h"

/*************************/
/* Print error then exit */
/*************************/
void errorExit(const char *msg1, const char *msg2){

	fprintf(stderr, "%s: %s\n", msg1, msg2);
	exit(1);
}

/****************************************************************/
/* Malloc then test for success. Makes code a bit more readable */
/****************************************************************/
void* malloct(size_t bytes){
	
	void *ptr = malloc(bytes);
	
	if(NULL == ptr)
		errorExit("Fatal Error", "Cannot malloc memory");

	return ptr;
}

/**************************************************/
/* Simple binary to lower case hex converter util */
/**************************************************/
void digest2Hex(const unsigned char *digest, int len, char *hex){

	int i;
	for(i=0; i<len; i++){
		sprintf(hex + (i*2), "%02x", digest[i]);
	}
}

/*******************************************************************************/
/* Convert string to lower case hex encoded hash. Caller frees returned buffer */
/*******************************************************************************/
char* string2HexSHA256(const char *s){

	unsigned char hash[32];

	if(!SHA256(s, strlen(s), hash))
		errorExit("OpenSSL library error", "SHA256 returned NULL");
	
	char *hex=(char*)malloct(65);

	digest2Hex(hash, 32, hex);

	return hex;
}

/*****************************************************************************************/
/* Convert string and key (with length len) to binary hash. Caller frees returned buffer */
/*****************************************************************************************/
unsigned char* string2HMACSHA256(const char *s, const unsigned char *key, int len){

	unsigned char* hash=(unsigned char*)malloct(32);

	if(NULL == HMAC(EVP_sha256(), key, len, s, strlen(s), hash, NULL))
		errorExit("OpenSSL library error", "HMAC returned NULL");

	return hash;
}

/*************************************************************************************************/
/* Simple base64 encoder per https://en.wikipedia.org/wiki/Base64 . Caller frees returned buffer */
/*************************************************************************************************/
char* base64Encode(const unsigned char *data, int len){

	const static char *lookupTable="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	
	int padding = 0;

	if(len % 3 > 0)
		padding = 3 - (len % 3);
	
	int len64 = 4 * (len + padding) / 3;
	char *data64 = (char*) malloct(len64 + 1);
	
	int in=0;
	int out=0;
	
	while(in < len){
		
		uint32_t byte1 = 0;
		uint32_t byte2 = 0;
		uint32_t byte3 = 0;
		uint32_t threeBytes = 0;
		
		byte1 = data[in++];
		 
		if(in < len)
			byte2 = data[in++];

		if(in < len)
			byte3 = data[in++];		
		
		threeBytes = (byte1 << 16) + (byte2 << 8) + byte3;
		
		data64[out++] = lookupTable[(threeBytes >> 18) & 0x3F];
		data64[out++] = lookupTable[(threeBytes >> 12) & 0x3F];
		data64[out++] = lookupTable[(threeBytes >>  6) & 0x3F];
		data64[out++] = lookupTable[(threeBytes >>  0) & 0x3F];
	}
	
	data64[out] = '\0';
	
	if(padding >= 1)
		data64[out-1] = '=';
		
	if(padding == 2)
		data64[out-2] = '=';
	
	return data64;
}

/*************************************************************************************************************************************/
/* Creates JSON payload per http://docs.aws.amazon.com/kinesis/latest/APIReference/API_PutRecord.html . Caller frees returned buffer */
/*************************************************************************************************************************************/
char* makePutRecordPayload(const unsigned char *data, int len, const char *streamName, const char *partitionKey){

	static const char *template =
		"{"	
		"\"StreamName\":\"%s\","
		"\"PartitionKey\":\"%s\","
		"\"Data\":\"%s\""
		"}";
	
	char *encData=base64Encode(data, len);

	char *payload=(char*)malloct(strlen(template)+strlen(streamName)+strlen(partitionKey)+strlen(encData) + 1);

	sprintf(
		payload,
		template,
		streamName,
		partitionKey,
		encData
		);

	free(encData);

	return payload;
}

/******************************************************************************************************************************************/
/* Creates JSON payload per http://docs.aws.amazon.com/kinesis/latest/APIReference/API_DescribeStream.html . Caller frees returned buffer */
/******************************************************************************************************************************************/
char* makeDescribeStreamPayload(const char *streamName){

	static const char *template =
		"{"	
		"\"StreamName\":\"%s\""
		"}";

	char *payload=(char*)malloct(strlen(template)+strlen(streamName) + 1);

	sprintf(
		payload,
		template,
		streamName
		);

	return payload;
}

/***************************************************************************************************************************************/
/* Creates JSON payload per http://docs.aws.amazon.com/kinesis/latest/APIReference/API_ListStreams.html . Caller frees returned buffer */
/***************************************************************************************************************************************/
char* makeListStreamsPayload(){

	static const char *template = "{}";

	char *payload=(char*)malloct(strlen(template) + 1);

	sprintf(
		payload,
		template
		);

	return payload;
}

/*************************************************************************************************************************************************/
/* Creates Canonical Request per http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html . Caller frees returned buffer */
/*************************************************************************************************************************************************/
char* makeCanonicalRequest(const char *host, const char *longDate, const char *payload){

	static const char *template =
		"POST\n"
		"/\n"
		"\n"
		"content-type:application/x-amz-json-1.1\n"
		"host:%s\n"
		"x-amz-date:%s\n"
		"\n"
		"content-type;host;x-amz-date\n"
		"%s";

	char *hash = string2HexSHA256(payload);

	char *creq=(char*)malloct(strlen(template)+strlen(host)+strlen(longDate)+strlen(hash) + 1);

	sprintf(
		creq,
		template,
		host,
		longDate,
		hash	
		);

	free(hash);

	return creq;
}

/*******************************************************************************************************************************************/
/* Creates String to Sign per http://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html . Caller frees returned buffer */
/*******************************************************************************************************************************************/
char* makeStringToSign(const char *longDate, const char *shortDate, const char *region, const char *service, const char *canonicalRequest){

	static const char *template =
		"AWS4-HMAC-SHA256\n"
		"%s\n"
		"%s/%s/%s/aws4_request\n"
		"%s";

	char *hash = string2HexSHA256(canonicalRequest);

	char *str=(char*)malloct(strlen(template)+strlen(longDate)+strlen(shortDate)+strlen(service)+strlen(region)+strlen(hash) + 1);

	sprintf(
		str,
		template,
		longDate,
		shortDate,
		region,
		service,
		hash	
		);

	free(hash);

	return str;
}

/**************************************************************************************************************************************/
/* Calculate signature per http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html . Caller frees returned buffer */
/**************************************************************************************************************************************/
char* makeSignature(const char *key, const char *shortDate, const char *region, const char *service, const char *stringToSign){

	unsigned char *kSecret, *kDate, *kRegion, *kService, *kSigning, *sig;

	kSecret=(unsigned char*)malloct(strlen(key) + 5);
	sprintf(kSecret, "AWS4%s", key);
	kDate=string2HMACSHA256(shortDate, kSecret, strlen(kSecret));
	kRegion=string2HMACSHA256(region, kDate, 32);
	kService=string2HMACSHA256(service, kRegion, 32);
	kSigning=string2HMACSHA256("aws4_request", kService, 32);
	sig=string2HMACSHA256(stringToSign, kSigning, 32);

	char *hex = (char*)malloct(65);
	digest2Hex(sig, 32, hex);

	free(kSecret);
	free(kDate);
	free(kRegion);
	free(kService);
	free(kSigning);
	free(sig);
	
	return hex;
}

/*****************************************************************************************************************************************************/
/* Create Authentication Header per http://docs.aws.amazon.com/general/latest/gr/sigv4-add-signature-to-request.html . Caller frees returned buffer  */
/*****************************************************************************************************************************************************/
char* makeAuthHeader(const char *key, const char *keyId, const char *longDate, const char *shortDate, const char *region,  const char *endpoint, const char *payload){

	static const char *service = "kinesis";
	
	static const char *template =
		"Authorization: AWS4-HMAC-SHA256 "
		"Credential=%s/%s/%s/%s/aws4_request, "
		"SignedHeaders=content-type;host;x-amz-date, "
		"Signature=%s";

	/* canonical request */
	char *creq = makeCanonicalRequest(endpoint, longDate, payload);
	
	/* string to sign */
	char *stringTosign = makeStringToSign(longDate, shortDate, region, service, creq);
	
	/* signature */
	char *sig = makeSignature(key, shortDate, region, service, stringTosign);

	char* header=(char*)malloct(strlen(template)+strlen(keyId)+strlen(shortDate)+strlen(region)+strlen(service)+strlen(sig) + 1);

	sprintf(
		header,
		template,
		keyId,
		shortDate,
		region,
		service,
		sig	
		);

	free(creq);
	free(stringTosign);
	free(sig);
	
	return header;
}

/*********************************************************************************/
/* Prints null terminated short and long form UTC dates in user supplied buffer. */
/* longDate and shortDate buffers should be 17 and 9 chars respectively.         */
/*********************************************************************************/
void makeDateStrings(char *longDate, char *shortDate){

	time_t time_;
	struct tm tm_;

	time(&time_);
	gmtime_r(&time_, &tm_);
	strftime(longDate, 17, "%Y%m%dT%H%M%SZ", &tm_);
	strftime(shortDate, 9, "%Y%m%d", &tm_);
}

/**************************************************/
/* See comments in header file for kt* functions  */
/**************************************************/
AWSContext* ktMakeAWSContext(const char *key, const char *keyId, const char *sessionToken, const char *region, const char *endpoint){

	AWSContext* ctx = malloct(sizeof(AWSContext));
	
	ctx->key = malloct(strlen(key)+1);
	strcpy(ctx->key, key);
	ctx->keyId = malloct(strlen(keyId)+1);
	strcpy(ctx->keyId, keyId);
	
	ctx->sessionToken = NULL;
	if(sessionToken != NULL){
		ctx->sessionToken = malloct(strlen(sessionToken)+1);
		strcpy(ctx->sessionToken, sessionToken);
	}
	
	ctx->region = malloct(strlen(region)+1);
	strcpy(ctx->region, region);
	ctx->endpoint = malloct(strlen(endpoint)+1);
	strcpy(ctx->endpoint, endpoint);
	ctx->url = malloct(strlen(endpoint) + 10);
	sprintf(ctx->url, "https://%s", endpoint);
	
	return ctx;
}

/**************************************************/
/* See comments in header file for kt* functions  */
/**************************************************/
void ktFreeAWSContext(AWSContext* ctx){
	
	free(ctx->key);
	free(ctx->keyId);
	free(ctx->sessionToken);
	free(ctx->region);
	free(ctx->endpoint);
	free(ctx->url);
	free(ctx);
}

/****************************************************************************************************************/
/* AWSHeaders wraps headers required for HTTP post. Use makeAWSHeaders to construct and freeAWSHeaders to free. */
/****************************************************************************************************************/
typedef struct{
	char *authorization;
	char *contentType;
	char *expect;
	char *xAMZSecurityToken;
	char *xAMZTarget;
	char *xAMZDate;
}AWSHeaders;

/****************************************************************************************************************************************************/
/* AWSHeaders constructor. Makes all headers need for a successful POST. sessionToken only required for temporary credentials otherwise set to NULL */
/****************************************************************************************************************************************************/
AWSHeaders* makeAWSHeaders(const char *authHeader, const char *sessionToken, const char *target, const char *longDate){

	AWSHeaders* headers = malloct(sizeof(AWSHeaders));

	headers->authorization = malloct(strlen(authHeader)+1);
	strcpy(headers->authorization, authHeader);
	
	headers->contentType = "Content-Type: application/x-amz-json-1.1";
	headers->expect = "Expect:";

	headers->xAMZSecurityToken = NULL;
	if(sessionToken){
		headers->xAMZSecurityToken = (char*) malloct(strlen(sessionToken) + 25);
		sprintf(headers->xAMZSecurityToken, "x-amz-security-token: %s", sessionToken);
	}
	
	headers->xAMZTarget = (char*) malloct(strlen(target) + 25);
	sprintf(headers->xAMZTarget, "x-amz-target: %s", target);
	
	headers->xAMZDate = (char*) malloct(strlen(longDate) + 25);
	sprintf(headers->xAMZDate, "x-amz-date: %s", longDate);
	
	return headers;
};

/*************************/
/* AWSHeaders destructor */
/*************************/
void freeAWSHeaders(AWSHeaders* headers){
	
	free(headers->authorization);
	free(headers->xAMZSecurityToken);
	free(headers->xAMZTarget);
	free(headers->xAMZDate);
	free(headers);
}

/**************************************************************************************/
/* Curl specific callback function to process response header and response body data. */
/* First MAX_CURL_RESPONSE_DATA_SIZE maximum chars are saved.                         */
/**************************************************************************************/
static size_t curlResponseCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	httpResponse *response = (httpResponse*)userp;

	int realSize = size*nmemb;
	int spaceRemaining = MAX_HTTP_RESPONSE_SIZE -1 - response->len;
	int copySize = 0;
	
	if(realSize <= spaceRemaining)
		copySize=realSize;
	else
		copySize=spaceRemaining;
	
	/* copy and null terminate */
	memcpy(&(response->text[response->len]), contents, copySize);
	response->len+=copySize;
	response->text[(response->len)]='\0';
	
	return realSize;
}

/*****************************************************************************************************************************/
/* Curl specific HTTP post routine.                                                                                          */
/* Set respHeader, respBody, errorMsg to NULL to ignore response header, response body and curl error messages respectively. */
/* If supplied, errorMsg must have minimum size CURL_ERROR_SIZE.                                                             */
/* Maximum MAX_CURL_RESPONSE_DATA_SIZE chars will be saved in respHeader and respBody.                                       */
/* Returns 0 for a curl level error (see errorMsg for details) otherwise HTTP status code. 200 indicates success.            */
/*****************************************************************************************************************************/
int curlDoPost(const char *url, const AWSHeaders *headers, const char *payload, httpResponse *respHeader, httpResponse *respBody, char *errorMsg){
	
	/* curl init */
	CURL *curl = curl_easy_init();
	if(!curl)
		errorExit("Fatal curl error", "Cannot initialize curl");

	/* uncomment for verbose */
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		
	/* set url */
	curl_easy_setopt(curl, CURLOPT_URL, url);
 
 	/* set timeout */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

	/* (too) permissive SSL options */
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* set headers */
	struct curl_slist *list = NULL;
	list = curl_slist_append(list, headers->authorization);
	list = curl_slist_append(list, headers->contentType);
	list = curl_slist_append(list, headers->expect);
	if(headers->xAMZSecurityToken) /* only included for temporary credentials */
		list = curl_slist_append(list, headers->xAMZSecurityToken);
	list = curl_slist_append(list, headers->xAMZTarget);
	list = curl_slist_append(list, headers->xAMZDate);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	
	/* set post data */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
	
	/* set error message buffer if required */
	if(errorMsg)
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorMsg);
	
	/* set callbacks if we want to save response header or body */
	if(respHeader){
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlResponseCallback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)respHeader);
		respHeader->len=0; /* empty buffer prior to call */
	}

	if(respBody){
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlResponseCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)respBody);
		respBody->len=0; /* empty buffer prior to call */
	}
 
	long retcode = 0;
	/* Perform request, on success set retcode to HTTP status code*/
	if(CURLE_OK == curl_easy_perform(curl)){
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode);
	}
	
	/* Curl cleanup */
	curl_slist_free_all(list);
	curl_easy_cleanup(curl);
	
	return retcode;
}

/**************************************************/
/* See comments in header file for kt* functions  */
/**************************************************/
int ktPutRecord(const AWSContext *ctx, const char *streamName, const char *partitionKey, const unsigned char *data, int len, httpResponse *respHeader, httpResponse *respBody, char *errorMsg){
	
	static const char *target = "Kinesis_20131202.PutRecord";
	
	/* make date strings */
	char longDate[17], shortDate[9];
	makeDateStrings(longDate, shortDate);

	/* make payload */
	char *payload = makePutRecordPayload(data, len, streamName, partitionKey);
	
	/* make Authorization header */
	char *authHeader = makeAuthHeader(ctx->key, ctx->keyId, longDate, shortDate, ctx->region, ctx->endpoint, payload);
	
	/* make all headers */
	AWSHeaders *headers = makeAWSHeaders(authHeader, ctx->sessionToken, target, longDate);
		
	/* do the post */
	int retcode = curlDoPost(ctx->url, headers, payload, respHeader, respBody, errorMsg);
	
	/* cleanup */
	free(payload);
	free(authHeader);
	freeAWSHeaders(headers);
	
	return retcode;
}

/**************************************************/
/* See comments in header file for kt* functions  */
/**************************************************/
int ktDescribeStream(const AWSContext *ctx, const char *streamName, httpResponse *respHeader, httpResponse *respBody, char *errorMsg){

	static const char *target = "Kinesis_20131202.DescribeStream";
	
	/* make date strings */
	char longDate[17], shortDate[9];
	makeDateStrings(longDate, shortDate);

	/* make payload */
	char *payload = makeDescribeStreamPayload(streamName);
	
	/* make Authorization header */
	char *authHeader = makeAuthHeader(ctx->key, ctx->keyId, longDate, shortDate, ctx->region, ctx->endpoint, payload);
	
	/* make all headers */
	AWSHeaders *headers = makeAWSHeaders(authHeader, ctx->sessionToken, target, longDate);
		
	/* do the post */
	int retcode = curlDoPost(ctx->url, headers, payload, respHeader, respBody, errorMsg);
	
	/* cleanup */
	free(payload);
	free(authHeader);
	freeAWSHeaders(headers);
	
	return retcode;
}

/**************************************************/
/* See comments in header file for kt* functions  */
/**************************************************/
int ktListStreams(const AWSContext *ctx, httpResponse *respHeader, httpResponse *respBody, char *errorMsg){

	static const char *target = "Kinesis_20131202.ListStreams";
	
	/* make date strings */
	char longDate[17], shortDate[9];
	makeDateStrings(longDate, shortDate);

	/* make payload */
	char *payload = makeListStreamsPayload();
	
	/* make Authorization header */
	char *authHeader = makeAuthHeader(ctx->key, ctx->keyId, longDate, shortDate, ctx->region, ctx->endpoint, payload);
	
	/* make all headers */
	AWSHeaders *headers = makeAWSHeaders(authHeader, ctx->sessionToken, target, longDate);
		
	/* do the post */
	int retcode = curlDoPost(ctx->url, headers, payload, respHeader, respBody, errorMsg);
	
	/* cleanup */
	free(payload);
	free(authHeader);
	freeAWSHeaders(headers);
	
	return retcode;
}

