import json
import base64
import boto3
import logging
import os
from botocore.exceptions import ClientError

logging.basicConfig(level=logging.INFO)

def lambda_handler(event, context):
    method = event['requestContext']['http']['method']
    if method == 'POST':
        try:
            bucket_name = os.environ['BUCKET_NAME']
            file_name = os.environ['FILE_NAME']
        except:
            return {'statusCode': 500, 'body': "Unable to load required environment configuration"}

        try:
            image_bytes = base64.b64decode(event['body'])
            print(f"base64 decoded image: {image_bytes}")
            image_bytes = b"\xFF" + image_bytes
            print(f"base64 decoded image with edited header: {image_bytes}")
            rekognition = boto3.client('rekognition')
            response = rekognition.compare_faces(
                SourceImage={
                    'S3Object': {
                        'Bucket': bucket_name,
                        'Name': file_name
                    }
                },
                TargetImage={
                    'Bytes': image_bytes
                }
            )
            print("Rekognition response received")
            print(f"Response: {response}")
            matches = response["FaceMatches"]
            if len(matches) > 0:
                similarity = matches[0]['Similarity']
                responseJson = {
                    'access': 'approved',
                    'similarity': similarity
                }
                return {'statusCode': 200, 'headers': {'Content-Type': 'application/json'}, 'body': json.dumps(responseJson)}
            else:
                responseJson = {
                    'access': 'denied'
                }
                return {'statusCode': 401, 'headers': {'Content-Type': 'application/json'}, 'body': json.dumps(responseJson)}
        except ClientError as err:
            if err.response['Error']['Code'] == 'InvalidParameterException':
                return {'statusCode': 422, 'body': 'Image must be of a face'}
            else:
                logging.error(f"Execution failed due to error: {err}")
                return {'statusCode': 500, 'body': 'Internal Server Error'}
        except Exception as err:
            logging.error(f"Execution failed due to error: {err}")
            return {'statusCode': 500, 'body': 'Internal Server Error'}
        
