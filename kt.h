#ifndef KT_H
#define KT_H

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************/
/* AWSContext objects store static credentials and stream information.             */
/* Use ktMakeAWSContext and ktFreeAWSContext to create and destroy.                */
/* sessionToken is only required for temporary credentials, otherwise set to NULL. */
/***********************************************************************************/

typedef struct{
	char *key;
	char *keyId;
	char *sessionToken;
	char *region;
	char *endpoint;
	char *url;
}AWSContext;

AWSContext* ktMakeAWSContext(const char *key, const char *keyId, const char *sessionToken, const char *region, const char *endpoint);
void ktFreeAWSContext(AWSContext* ctx);

/************************************************************************************************************/
/* kt* functions below map to the similarly named actions in the AWS Kinesis API.                           */
/* Functions return 0 if a transport level error has occurred, in which case errorMsg will contain details. */
/* Otherwise functions return HTTP status code - 200 indicates success. Any output will be in respBody.     */
/* For non 200 HTTP status codes, respBody will contain error messages.                                     */
/* respHeader, respBody, errorMsg can be set to NULL if the respective data is not required.                */
/* Only the first MAX_HTTP_RESPONSE_SIZE chars are saved in respHeader and respBody.                        */
/* errorMsg should be at least 256 characters long if set.                                                  */
/************************************************************************************************************/

#define MAX_HTTP_RESPONSE_SIZE 512
typedef struct{
	char text[MAX_HTTP_RESPONSE_SIZE];
	int len;
}httpResponse;

int ktListStreams(const AWSContext *ctx, httpResponse *respHeader, httpResponse *respBody, char *errorMsg);
int ktDescribeStream(const AWSContext *ctx, const char *streamName, httpResponse *respHeader, httpResponse *respBody, char *errorMsg);
/* set partitionKey to NULL if not required */
int ktPutRecord(const AWSContext *ctx, const char *streamName, const char *partitionKey, const unsigned char *data, int len, httpResponse *respHeader, httpResponse *respBody, char *errorMsg);

#ifdef __cplusplus
}
#endif

#endif //KT_H
