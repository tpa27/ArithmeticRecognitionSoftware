/*
 * ArithmeticRecognitionSoftware.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: Thomas Amon and Zhi Wei Zheng
 */

#include "ARS.h"
#include <math.h>
#include <dirent.h>

void usage()
{
	printf("Usage: ./ARS IMAGE\n");
	printf("Missing image filename.\n");
}

char int_to_ops(int i)
{
	char rv;
	switch(i)
	{
		case 10:
			rv = '+';
			break;
		case 11: 
			rv = '-';
			break;
		case 12: 
			rv = 'x';
			break;
		case 13: 
			rv = '%';
			break;
		case 14: 
			rv = ')';
			break;
		case 15: 
			rv = '(';
			break;
		default:
			rv = (char)(((int)'0')+i);
			break;
	}
	return rv;
}

/**
 * Loads an image (stored as an IplImage struct) for each
 * filename provided.
 * @param numImages		The number of images to load
 * @param filenames		A list of strings of the filenames to load
 */
IplImage** loadSamples(int numSamples, int numClasses, char* dirpath, int
		image_size)
{
	IplImage** rv; // the return result
	int i, // used for looping
		 j, // used for looping
		 k; // used for indexing 
	DIR *dir;  //used to find all files
	struct dirent *ent; // a directory entry 
	char extention[4], // stores the name of the subfolder(1/, 2/, ...)
	 	  *dirname, // stores the dirpath + extension
		  fullpath[256], // stores the full path to each file (dirname + file)
		  character; //the current character in the folders
	IplImage* current;

	rv =(IplImage**)malloc(numClasses*numSamples*sizeof(IplImage*));
	k = 0;
   for(i = 0; i < numClasses; i++)	
	{
		dirname = (char*)malloc((strlen(dirpath)+4)*sizeof(char));
		character = int_to_ops(i);
		sprintf(extention, "%c/", character);
		strcpy(dirname, dirpath);
		strcat(dirname, extention);
		if ((dir = opendir (dirname)) == NULL) 
		{
			fprintf(stderr, "Could not open dir %s\n", dirname);
			return NULL;
		}
	   for(j = 0; j < numSamples+2; j++)	 //need to add 2 because . and ..
		{
			ent = readdir(dir);
			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;
			strcpy(fullpath, dirname);
			strcat(fullpath, ent->d_name);
			rv[k] = scale_image(cvLoadImage(fullpath, 0), image_size);
			if(rv[k] == 0) 
			{
				fprintf(stderr, "Error: couldn't load image %s\n", ent->d_name);
				return NULL;
			}
			k++;
		}
		closedir(dir);
	}
	
	return rv;
}

IplImage* convert_to_bw(IplImage* image)
{
	IplImage* bw_image = cvCreateImage(cvGetSize(image), IPL_DEPTH_8U, 1);
	cvThreshold(image, bw_image, 128, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
	return bw_image;
}


IplImage** OpCropper(IplImage* imgSrc, int* ImgNum)
{
	int i,			 //Iterate through column
		 min,			 //Stores starting column number of a operator/operand
		 max,			 //Stores ending column number of a operator/operand
		 minFound,	 //Acts as a bool to see if the first minimum value is set
	    count;		 //Index through the array of IplImage

	CvRect cropRect;			//The rectangle surrounding an operator/operand
	IplImage	**rv;				//Array to store a maximum of 128 IplImages

	CvMat data;  			//Stores the current column
	CvScalar maxVal,		//Stores the value of a column that is entirely white
				val;			//Stores the value of the current column

	//For each col sum, if sum < width*255 then we find the min 
	//then continue to end to search the max, if sum< width*255 then is new
	//max

	//initialize some variables
	count = 0;
	minFound = 0;
	min = 0;
	max = 0;
	rv = (IplImage**)malloc(128*sizeof(IplImage*));
	maxVal = cvRealScalar(imgSrc->height* 255);
	val = cvRealScalar(0);
	
	//Iterate through each column
	for (i=0; i< imgSrc->width; i++){
		cvGetCol(imgSrc, &data, i);
		val= cvSum(&data);

		//Set the min to the initial column containing black
		//This is where the Operator/Operand begins
		if(val.val[0] < maxVal.val[0] && !minFound){
			min = i;
			minFound = 1;
		}
		//Keeps setting the max value to the column with a black pixel
		else if(val.val[0] < maxVal.val[0] && minFound){
			max = i;			
		}
		//Once a white column is found, we have found the end of this
		//operator/operand
		else if(val.val[0] == maxVal.val[0] && minFound) 
		{
			//Set a rectangle to have the dimension of the cropping area
			if(max-min < 20)  //skip unreasonably small marks
				continue;

			cropRect = cvRect(min, 0, max-min, imgSrc->height);
			//Crop the image and put it in the array
			cvSetImageROI(imgSrc, cropRect);
			rv[count] = cvCreateImage(cvSize(cropRect.width, cropRect.height), imgSrc->depth,imgSrc->nChannels);
			cvCopy(imgSrc, rv[count], NULL);
			cvResetImageROI(imgSrc);

			//Increment the index and amount of images
			count++;	
			(*ImgNum)++;	
			//Minimum is reset to find a new operand/opeartor
			minFound = 0;
		}
	}
	return rv;
}

CvMat** createMatrices(IplImage** samples, int num_samples, int
		num_classes, int image_size)
{
	CvMat** res; // the result		
	CvMat *train_data, //matrix of the train data
   	   *train_classes, //matrix of the training classes 
		   row, // the row to place the image
      	row_header,	// header of compressed matrix	
      	*reshaped_image; // stores the compressed matrix
	int i, //used for looping
		 j, //used for looping
		 k; //used for indexing
	res = (CvMat**)malloc(2*sizeof(CvMat*));
	train_data = cvCreateMat(num_classes*num_samples, image_size*image_size, CV_32FC1);
	train_classes = cvCreateMat(num_classes*num_samples, 1, CV_32FC1);
	res[0] = train_data;
	res[1] = train_classes;

	k = 0;
   for(i = 0; i < num_classes; i++)	
	{
	   for(j = 0; j < num_samples; j++)	
		{
			//Set the value of this class
      	cvGetRow(train_classes, &row, k);
      	cvSet(&row, cvRealScalar(i), NULL);

      	//Set the data of this class
      	cvGetRow(train_data, &row, k);
      
      	IplImage* img = cvCreateImage(cvSize(image_size, image_size), IPL_DEPTH_32F, 1 );
      	//convert 8 bits image to 32 float image
      	cvConvertScale(samples[k], img, 0.0039215, 0);

      	//coverts a size x size martix to a 1 x (size*size)
      	reshaped_image = cvReshape(img, &row_header, 0, 1);
			
			//copy the compressed matrix into its row
      	cvCopy(reshaped_image, &row, NULL);
			k++;
		}
	}
	return res;
}

void findX(IplImage* imgSrc,int* min, int* max){
	int i;
	int minFound=0;
	CvMat data;
	CvScalar maxVal=cvRealScalar(imgSrc->height* 255);
	CvScalar val=cvRealScalar(0);
	//For each col sum, if sum < width*255 then we find the min 
	//then continue to end to search the max, if sum< width*255 then is new max
	for (i=0; i < imgSrc->width; i++){
		cvGetCol(imgSrc, &data, i);
		val= cvSum(&data);
		if(val.val[0] < maxVal.val[0]){
			*max= i;
			if(!minFound){
				*min= i;
				minFound= 1;
			}
		}
	}
}

void findY(IplImage* imgSrc,int* min, int* max)
{
	int i;
	int minFound=0;
	CvMat data;
	CvScalar maxVal=cvRealScalar(imgSrc->width * 250);	//250 to ignore small
																		//blemishes
	CvScalar val=cvRealScalar(0);
	//For each col sum, if sum < width*255 then we find the min 
	//then continue to end to search the max, if sum< width*250 then is new max
	for (i=0; i< imgSrc->height; i++){
		cvGetRow(imgSrc, &data, i);
		val= cvSum(&data);
		if(val.val[0] < maxVal.val[0]){
			*max=i;
			if(!minFound){
				*min= i;
				minFound= 1;
			}
		}
	}
}

CvRect findBB(IplImage* imgSrc)
{
	CvRect rv;
	int xmin, 
		 xmax, 
		 ymin,
		 ymax;

	xmin = 0;
	xmax = imgSrc->height;
	ymin = 0;
	ymax = imgSrc->width;
	findX(imgSrc, &xmin, &xmax);
	findY(imgSrc, &ymin, &ymax);
	
	rv  = cvRect(xmin, ymin, xmax-xmin, ymax-ymin);
	
	return rv;
}

IplImage* scale_image(IplImage* imgSrc, int image_size)
{
	IplImage *result,  //imgSrc with a 1 as the aspect ratio
			   *scaledResult;	//the result scaled to image_size

	CvRect bb;	//bounding box
	
	//Find bounding box
	bb = findBB(imgSrc);
	
	//Set the ROI to the bounding box
	cvSetImageROI(imgSrc, cvRect(bb.x, bb.y, bb.width, bb.height));

	//Create a new image with the height and width both as the greater of the
	//two of the bounding box	
	int size = (bb.width>bb.height)?bb.width:bb.height;
	result = cvCreateImage(cvSize( size, size ), 8, 1 );
	cvSet(result, CV_RGB(255, 255, 255), NULL); 
	
	//This will give you the x and y position so the new image will be
	//centered
	int x = (int)floor((float)(size-bb.width)/2.0f);
	int y = (int)floor((float)(size-bb.height)/2.0f);
	
	cvSetImageROI(result, cvRect(x, y, bb.width, bb.height));
	cvCopy(imgSrc, result, NULL);
	
	cvResetImageROI(imgSrc);
	cvResetImageROI(result);
	
	//Scale result
	scaledResult = cvCreateImage(cvSize(image_size, image_size), 8, 1 );
	cvResize(result, scaledResult, CV_INTER_NN);


	//Return scaled image data
	return scaledResult;
}


int topIndex = -1;							//Global variable used to index a stack
char* in2post(char* equation){
	char* copy = strdup(equation);										//Initialize a copy of the original equation
	char** equationArray = (char**)malloc(256*sizeof(char*));	//Initialize an array of strings to hold 256 strings
	char* p = strtok(copy," ");											//Tokenizer to delimit a space
	int i = 0;																	//Iterator
	int arraySize = 0;														//Keep track of the size of the array of strings
	char* postfix = (char*)malloc(128*sizeof(char));				//Initialize a variable to hold the postfix expression with suitable size
	strcpy(postfix, "");
	if(strlen(equation) == 1) {
		return equation;
	}
	//Creates an array containing the delimited
	while(p != NULL) {
		equationArray[i++] = p;
		p = strtok(NULL, " ");
		arraySize++;
	}

	char* stack[arraySize];				//Creates a stack with enough size to hold all the postfix operator/operands

	//Iterate through each operand and operator for infix to postfix conversion
	for( i = 0; i < arraySize; i++){
		//Determine if the element is an operator
		if(strcmp(equationArray[i], "+") == 0 || strcmp(equationArray[i],"-")
				== 0 || strcmp(equationArray[i], "x") == 0 || strcmp(equationArray[i], "%") == 0
			  	|| strcmp(equationArray[i], "(") == 0 || strcmp(equationArray[i], ")") == 0) {
			//Push to stack if there are no elements in the stack
			if(topIndex == -1) {
				push(equationArray[i], stack);
			}
			//If it's plus or minus, it will concatenate the last element of
			//the stack as long as the priority is the same of higher.
			else if(strcmp(equationArray[i], "(") == 0) {
				push(equationArray[i], stack);
			}	
			else if(strcmp(equationArray[i], ")") == 0) {
				while(strcmp(top(stack), "(") != 0 && topIndex != -1) {
					strcat(postfix, pop(stack));
					strcat(postfix, " ");
				}
				if(topIndex == -1)
				{
					fprintf(stderr, "Error: Invalid expression\n");
					return NULL;
				}
				pop(stack);
			}
			else if(strcmp(equationArray[i], "+") == 0 || strcmp(equationArray[i], "-") == 0){
				while(strcmp(top(stack), "+") == 0 || strcmp(top(stack), "-")
						==0 || strcmp(top(stack), "x") == 0 || strcmp(top(stack),"%") == 0 ) {
					strcat(postfix, pop(stack));
					strcat(postfix, " ");
				}
				//Pushes most recent operator to stack
				push(equationArray[i], stack);
			}
			//If it's a multiplication or division symbol, it keeps
			//concatenating the top stack elements as longa s it has the same
			//precendence.
			else if(strcmp(equationArray[i], "x") == 0 || strcmp(equationArray[i], "%") == 0) {
				while(strcmp(top(stack), "x") == 0 || strcmp(top(stack), "%") == 0 ) { 
					strcat(postfix, pop(stack));
					strcat(postfix, " ");
				}
				//Pushes most recent operator to stack
				push(equationArray[i], stack);
			}
		}
		//Concatenate to the postfix expression if element is an operand
		else {
			strcat(postfix, equationArray[i]);
			strcat(postfix, " ");
		}
	}
	//Concatenates all the remaining elements in the stack to the final
	//postfix expression
	while(topIndex != -1) {
		strcat(postfix, pop(stack));
		strcat(postfix, " ");
	}

	//Free memory used to hold copy
	free(copy);
	
	//Return the final postfix expression
	return postfix;
}

int postEval(char* postfix) {
	char* copy = strdup(postfix);											//Initialize a string to hold a copy of the postfix expression
	char** equationArray = (char**)malloc(257*sizeof(char*));	//Initialize an array of strings to hold the delimited postfix expressoin
	char* p = strtok(copy, " ");											//Tokenizes the first space
	int i = 0;																	//Iterator
	int arraySize = 0;														//Variable to keep track of the size of the array of strings

	if(strlen(postfix) == 1) {
		return atoi(postfix);
	}
	if(postfix == NULL)
		return 0;
	//Delimits the string by the spaces and set the array equal to each
	//element
	while(p != NULL) {
		equationArray[i++] = p;
		p = strtok(NULL, " ");
		arraySize++;
	}

	char* stack[arraySize*3];		//Initialize a stack with enough size to hold the delimited array
	char str[15];					//Initialize a string to for conversion from integer to string

	//Iterate through each operator and operand in the postfix array
	for(i = 0; i < arraySize; i++) {
		//Determine if the element is an operator
		if(strcmp(equationArray[i], "+") == 0 || strcmp(equationArray[i], "-")
				== 0 || strcmp(equationArray[i], "x") == 0 || strcmp(equationArray[i], "%") == 0) {
			int first = atoi(pop(stack));		//Convert the top element of the stack to an integer
			int second = atoi(pop(stack));	//Convert the secodn top element of the stack to an integer
			int result;								//Initialize an int to hold the result of the two top elements
			//Add the two numbers
			if(strcmp(equationArray[i], "+") == 0) {
				result = second + first;
			}
			//Subtract the two numbers
			else if(strcmp(equationArray[i], "-") == 0) {
				result = second - first;
			}
			//Multiply the two numbers
			else if(strcmp(equationArray[i], "x") == 0) {
				result = second*first;
			}
			//divide the two numbers
			else if(strcmp(equationArray[i], "%") == 0) { 
				result = second/first;
			}
			//Convert the result back into string 
			sprintf(str, "%d", result);

			//Initialize a new string to hold the value of the result
			char* newStr = (char*) malloc((strlen(str) + 1)*sizeof(char));	
			strcpy(newStr, str);

			//Push the result back onto the stack
			push(newStr, stack);
		}
		//It not an operator, push the element to the stack
		else {
			push(equationArray[i], stack);
		}
	}	
	//Free the memory of copy
	free(copy);

	//Return the last element of the stack, which contains the answer, as an
	//intger
	return atoi(pop(stack));
}

void push(char* value, char** stack) {
	//Increments indexing to keep track of the top element
	topIndex++;
	//Set the top element in the stack to the value passed in
	stack[topIndex] = value;

}

char* pop(char** stack) {
	//Check if the stack does not containg any element
	if( topIndex == -1) {
		return "-1";
	}

	//Initialize a variable to hold the top element of the stack
	char* element = stack[topIndex];

	//Decrement the top index
	topIndex--;

	//Return the top element
	return element;

}
char* top(char** stack) {
	//Returns the top element in the stack
	return stack[topIndex];
}
