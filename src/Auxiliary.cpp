/* Auxiliary.cpp
 * Author: Silvia-Laura Pintea
 * Copyright (c) 2010-2011 Silvia-Laura Pintea. All rights reserved.
 * Feel free to use this code, but please retain the above copyright notice.
 */
#include <vector>
#include <string>
#include <sstream>
#include <set>
#include <cstdlib>
#include <stdio.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include "eigenbackground/src/defines.hh"
#include "eigenbackground/src/Helpers.hh"
#include "Auxiliary.h"
//==============================================================================
/** Converts a pointer to an IplImage to an OpenCV Mat.
 */
cv::Mat ipl2mat(IplImage* ipl_image){
	cv::Mat mat_image(ipl_image);
	return mat_image;
}
//==============================================================================
/** Converts an OpenCV Mat to a pointer to an IplImage.
 */
IplImage* mat2ipl(cv::Mat image){
	IplImage* ipl_image = new IplImage(image);
	return ipl_image;
}
//==============================================================================
/** Convert the values from a cv::Mat of doubles to be between 0 and 1.
 */
void normalizeMat(cv::Mat &matrix){
	matrix.convertTo(matrix, CV_32FC1);
	float mini = matrix.at<float>(0,0), maxi = matrix.at<float>(0,0);
	for(int x=0; x<matrix.cols; x++){
		for(int y=0; y<matrix.rows; y++){
			if(mini>matrix.at<float>(y,x)){
				mini = matrix.at<float>(y,x);
			}
			if(maxi<matrix.at<float>(y,x)){
				maxi = matrix.at<float>(y,x);
			}
		}
	}
	matrix -= mini;
	if(static_cast<int>(maxi-mini)!=0){
		matrix /= (maxi-mini);
	}
}
//==============================================================================
/** Changes the values of the matrix to be between [-1,1].
 */
void range1Mat(cv::Mat &matrix){
	normalizeMat(matrix);
	matrix -= 0.5;
	matrix *= 2.0;
}
//==============================================================================
/** Write a 2D-matrix to a text file (first row is the dimension of the matrix).
 */
void mat2TxtFile(cv::Mat matrix, char* fileName, bool append){
	std::ofstream dictOut;
	try{
		if(append){
			dictOut.open(fileName, std::ios::out | std::ios::app);
			dictOut.seekp(0, std::ios::end);
		}else{
			dictOut.open(fileName, std::ios::out);
		}
	}catch(std::exception &e){
		cerr<<"Cannot open file: %s"<<e.what()<<endl;
		exit(1);
	}

	matrix.convertTo(matrix, CV_32FC1);
	dictOut<<matrix.cols<<" "<<matrix.rows<<std::endl;
	for(int y=0; y<matrix.rows; y++){
		for(int x=0; x<matrix.cols; x++){
			dictOut<<matrix.at<float>(y,x)<<" ";
		}
		dictOut<<std::endl;
	}
	dictOut.close();
}
//==============================================================================
/** Reads a 2D-matrix from a text file (first row is the dimension of the matrix).
 */
void txtFile2Mat(cv::Mat &matrix, char* fileName){
	std::ifstream dictFile(fileName);
	int y=0;
	if(dictFile.is_open()){
		// FIRST LINE IS THE SIZE OF THE MATRIX
		std::string fline;
		std::getline(dictFile, fline);
		std::deque<std::string> flineVect = splitLine(const_cast<char*>\
											(fline.c_str()),' ');
		if(flineVect.size() == 2){
			char *pRows, *pCols;
			int cols = strtol(flineVect[0].c_str(), &pCols, 10);
			int rows = strtol(flineVect[1].c_str(), &pRows, 10);
			matrix   = cv::Mat::zeros(cv::Size(cols,rows), CV_32FC1);
		}else return;
		fline.clear();
		flineVect.clear();

		// THE REST OF THE LINES ARE READ ONE BY ONE
		while(dictFile.good()){
			std::string line;
			std::getline(dictFile, line);
			std::deque<std::string> lineVect = splitLine(const_cast<char*>\
												(line.c_str()),' ');
			if(lineVect.size()>=1){
				for(std::size_t x=0; x<lineVect.size(); x++){
					char *pValue;
					matrix.at<float>(y,static_cast<int>(x)) = \
						strtod(lineVect[x].c_str(), &pValue);
				}
				y++;
			}
			line.clear();
			lineVect.clear();
		}
		dictFile.close();
	}
	matrix.convertTo(matrix, CV_32FC1);
}
//==============================================================================
/** Write a 2D-matrix to a binary file (first the dimension of the matrix).
 */
void mat2BinFile(cv::Mat matrix, char* fileName, bool append){
	std::ofstream mxFile;
	try{
		if(append){
			mxFile.open(fileName, std::ios::out|std::ios::app|std::ios::binary);
			mxFile.seekp(0, std::ios::end);
		}else{
			mxFile.open(fileName, std::ios::out|std::ios::binary);
		}
	}catch(std::exception &e){
		cerr<<"Cannot open file: %s"<<e.what()<<endl;
		exit(1);
	}

	matrix.convertTo(matrix, CV_32FC1);

	// FIRST WRITE THE DIMENSIONS OF THE MATRIX
	mxFile.write(reinterpret_cast<char*>(&matrix.cols), sizeof(int));
	mxFile.write(reinterpret_cast<char*>(&matrix.rows), sizeof(int));

	// WRITE THE MATRIX TO THE FILE
	for(int x=0; x<matrix.cols; x++){
		for(int y=0; y<matrix.rows; y++){
			mxFile.write(reinterpret_cast<char*>(&matrix.at<float>(y,x)),\
				sizeof(float));
		}
	}
	mxFile.close();
}
//==============================================================================
/** Reads a 2D-matrix from a binary file (first the dimension of the matrix).
 */
void binFile2mat(cv::Mat &matrix, char* fileName){
	if(!file_exists(fileName)){
		std::cerr<<"Error opening the file: "<<fileName<<std::endl;
		exit(1);
	}
	std::ifstream mxFile(fileName, std::ios::in | std::ios::binary);

	if(mxFile.is_open()){
		// FIRST READ THE MATRIX SIZE AND ALLOCATE IT
		int cols, rows;
		mxFile.read(reinterpret_cast<char*>(&cols), sizeof(int));
		mxFile.read(reinterpret_cast<char*>(&rows), sizeof(int));
		matrix = cv::Mat::zeros(cv::Size(cols,rows),CV_32FC1);

		// READ THE CONTENT OF THE MATRIX
		for(int x=0; x<matrix.cols; x++){
			for(int y=0; y<matrix.rows; y++){
				mxFile.read(reinterpret_cast<char*>(&matrix.at<float>(y,x)),\
					sizeof(float));
			}
		}
		mxFile.close();
	}
	matrix.convertTo(matrix, CV_32FC1);
}
//==============================================================================
/** Convert int to string.
 */
std::string int2string(int i){
	std::stringstream out;
	out << i;
	return out.str();
}
//==============================================================================
/** Changes a given angle in RADIANS to be positive and between [0,2*M_PI).
 */
void angle0to360(float &angle){
	while(angle >= 2.0*M_PI){
		angle -= 2.0*M_PI;
	}
	if(angle < 0.0){
		angle += 2.0*M_PI;
	}
}
//==============================================================================
/** Changes a given angle in RADIANS to be positive and between [-M_PI,M_PI).
 */
void angle180to180(float &angle){
	while(angle >= 2.0*M_PI){
		angle -= 2.0*M_PI;
	}
	if(angle < 0.0){
		angle += 2.0*M_PI;
	}
	angle = angle - M_PI;
}
//==============================================================================
/** Checks to see if a point is on the same side of a line like another given point.
 */
bool sameSubplane(cv::Point2f test, cv::Point2f point, float m, float b){
	if(isnan(m)){
		return (point.x*test.x)>=0.0;
	}else if(m == 0){
		return (point.y*test.y)>=0.0;
	}else{
		return (m*point.x+b-point.y)*(m*test.x+b-test.y)>=0.0;
	}
}
//==============================================================================
/** Get perpendicular to a line given by 2 points A, B in point C.
 */
void perpendicularLine(cv::Point2f A, cv::Point2f B, cv::Point2f C, float &m,\
float &b){
	float slope = (float)(B.y - A.y)/(float)(B.x - A.x);
	m            = -1.0/slope;
	b            = C.y - m * C.x;
}
//==============================================================================
/** Just displaying an image a bit larger to visualize it better.
 */
void showZoomedImage(cv::Mat image, const std::string title){
	cv::Mat large;
	cv::resize(image, large, cv::Size(0,0), 5, 5, cv::INTER_CUBIC);
	cv::imshow(title, large);
	cv::waitKey(0);
	cvDestroyWindow(title.c_str());
	large.release();
}
//==============================================================================





