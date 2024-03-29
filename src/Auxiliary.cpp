/* Auxiliary.cpp
 * Author: Silvia-Laura Pintea
 * Copyright (c) 2010-2011 Silvia-Laura Pintea. All rights reserved.
 * Feel free to use this code,but please retain the above copyright notice.
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
#include "Auxiliary.h"
//==============================================================================
/** Converts a pointer to an IplImage to an OpenCV Mat.
 */
cv::Mat Auxiliary::ipl2mat(IplImage* ipl_image){
	cv::Mat mat_image = cv::Mat(ipl_image);
	return mat_image;
}
//==============================================================================
/** Converts an OpenCV Mat to a pointer to an IplImage.
 */
IplImage* Auxiliary::mat2ipl(const cv::Mat &image){
	IplImage* ipl_image = new IplImage(image);
	return ipl_image;
}
//==============================================================================
/** Convert the values from a cv::Mat of floats to be between 0 and 1.
 */
void Auxiliary::normalizeMat(cv::Mat &matrix){
	int matType = matrix.type();
	matrix.convertTo(matrix,CV_32FC1);
	float mini = matrix.at<float>(0,0),maxi = matrix.at<float>(0,0);
	for(int x=0;x<matrix.cols;++x){
		for(int y=0;y<matrix.rows;++y){
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
	if(matType == CV_8UC1){
		matrix *= 255;
		matrix.convertTo(matrix,CV_8UC1);
	}
}
//==============================================================================
/** Changes the values of the matrix to be between [-1,1].
 */
void Auxiliary::range1Mat(cv::Mat &matrix){
	normalizeMat(matrix);
	matrix -= 0.5;
	matrix *= 2.0;
}
//==============================================================================
/** Write a 2D-matrix to a text file (first row is the dimension of the matrix).
 */
void Auxiliary::mat2TxtFile(cv::Mat &matrix,char* fileName,bool append){
	std::ofstream dictOut;
	try{
		if(append){
			dictOut.open(fileName,std::ios::out | std::ios::app);
			dictOut.seekp(0,std::ios::end);
		}else{
			dictOut.open(fileName,std::ios::out);
		}
	}catch(std::exception &e){
		cerr<<"Cannot open file: %s"<<e.what()<<endl;
		exit(1);
	}

	matrix.convertTo(matrix,CV_32FC1);
	dictOut<<matrix.cols<<" "<<matrix.rows<<std::endl;
	for(int y=0;y<matrix.rows;++y){
		for(int x=0;x<matrix.cols;++x){
			dictOut<<matrix.at<float>(y,x)<<" ";
		}
		dictOut<<std::endl;
	}
	dictOut.close();
}
//==============================================================================
/** Reads a 2D-matrix from a text file (first row is the dimension of the matrix).
 */
void Auxiliary::txtFile2Mat(cv::Mat &matrix,char* fileName){
	std::ifstream dictFile(fileName);
	int y=0;
	if(dictFile.is_open()){
		// FIRST LINE IS THE SIZE OF THE MATRIX
		std::string fline;
		std::getline(dictFile,fline);
		std::deque<std::string> flineVect = Helpers::splitLine(const_cast<char*>\
			(fline.c_str()),' ');
		if(flineVect.size() == 2){
			char *pRows,*pCols;
			int cols = strtol(flineVect[0].c_str(),&pCols,10);
			int rows = strtol(flineVect[1].c_str(),&pRows,10);
			matrix   = cv::Mat::zeros(cv::Size(cols,rows),CV_32FC1);
		}else return;
		fline.clear();
		flineVect.clear();

		// THE REST OF THE LINES ARE READ ONE BY ONE
		while(dictFile.good()){
			std::string line;
			std::getline(dictFile,line);
			std::deque<std::string> lineVect = Helpers::splitLine(const_cast<char*>\
				(line.c_str()),' ');
			if(lineVect.size()>=1){
				for(std::size_t x=0;x<lineVect.size();++x){
					char *pValue;
					matrix.at<float>(y,static_cast<int>(x)) = \
						strtod(lineVect[x].c_str(),&pValue);
				}
				++y;
			}
			line.clear();
			lineVect.clear();
		}
		dictFile.close();
	}
	matrix.convertTo(matrix,CV_32FC1);
}
//==============================================================================
/** Write a 2D-matrix to a binary file (first the dimension of the matrix).
 */
void Auxiliary::mat2BinFile(cv::Mat &matrix,char* fileName,bool append){
	std::ofstream mxFile;
	try{
		if(append){
			mxFile.open(fileName,std::ios::out|std::ios::app|std::ios::binary);
			mxFile.seekp(0,std::ios::end);
		}else{
			mxFile.open(fileName,std::ios::out|std::ios::binary);
		}
	}catch(std::exception &e){
		cerr<<"Cannot open file: %s"<<e.what()<<endl;
		exit(1);
	}

	matrix.convertTo(matrix,CV_32FC1);

	// FIRST WRITE THE DIMENSIONS OF THE MATRIX
	mxFile.write(reinterpret_cast<char*>(&matrix.cols),sizeof(int));
	mxFile.write(reinterpret_cast<char*>(&matrix.rows),sizeof(int));

	// WRITE THE MATRIX TO THE FILE
	for(int x=0;x<matrix.cols;++x){
		for(int y=0;y<matrix.rows;++y){
			mxFile.write(reinterpret_cast<char*>(&matrix.at<float>(y,x)),\
				sizeof(float));
		}
	}
	mxFile.close();
}
//==============================================================================
/** Reads a 2D-matrix from a binary file (first the dimension of the matrix).
 */
void Auxiliary::binFile2mat(cv::Mat &matrix,char* fileName){
	if(!Helpers::file_exists(fileName)){
		std::cerr<<"Error opening the file: "<<fileName<<std::endl;
		exit(1);
	}
	std::ifstream mxFile(fileName,std::ios::in | std::ios::binary);

	if(mxFile.is_open()){
		// FIRST READ THE MATRIX SIZE AND ALLOCATE IT
		int cols,rows;
		mxFile.read(reinterpret_cast<char*>(&cols),sizeof(int));
		mxFile.read(reinterpret_cast<char*>(&rows),sizeof(int));
		matrix = cv::Mat::zeros(cv::Size(cols,rows),CV_32FC1);

		// READ THE CONTENT OF THE MATRIX
		for(int x=0;x<matrix.cols;++x){
			for(int y=0;y<matrix.rows;++y){
				mxFile.read(reinterpret_cast<char*>(&matrix.at<float>(y,x)),\
					sizeof(float));
			}
		}
		mxFile.close();
	}
	matrix.convertTo(matrix,CV_32FC1);
}
//==============================================================================
/** Convert int to string.
 */
std::string Auxiliary::int2string(int i){
	std::stringstream out;
	out << i;
	return out.str();
}
//==============================================================================
/** Changes a given angle in RADIANS to be positive and between [0,2*M_PI).
 */
void Auxiliary::angle0to360(float &angle){
	while(angle >= 2.0*M_PI){
		angle -= 2.0*M_PI;
	}
	while(angle < 0.0){
		angle += 2.0*M_PI;
	}
}
//==============================================================================
/** Changes a given angle in RADIANS to be positive and between [-M_PI,M_PI).
 */
void Auxiliary::angle180to180(float &angle){
	while(angle >= 2.0*M_PI){
		angle -= 2.0*M_PI;
	}
	while(angle < 0.0){
		angle += 2.0*M_PI;
	}
	angle = angle - M_PI;
}
//==============================================================================
/** Checks to see if a point is on the same side of a line like another given point.
 */
bool Auxiliary::sameSubplane(const cv::Point2f &test,const cv::Point2f &point,\
float m,float b){
	if(isnan(m)){
		return (point.x*test.x)>=0.0;
	}else if(m == 0){
		return (point.y*test.y)>=0.0;
	}else{
		return (m*point.x+b-point.y)*(m*test.x+b-test.y)>=0.0;
	}
}
//==============================================================================
/** Get perpendicular to a line given by 2 points A,B in point C.
 */
void Auxiliary::perpendicularLine(const cv::Point2f &A,const cv::Point2f &B,\
const cv::Point2f &C,float &m,float &b){
	float slope = (float)(B.y - A.y)/(float)(B.x - A.x);
	m            = -1.0/slope;
	b            = C.y - m * C.x;
}
//==============================================================================
/** Just displaying an image a bit larger to visualize it better.
 */
void Auxiliary::showZoomedImage(const cv::Mat &image,const std::string &title){
	cv::Mat large;
	cv::resize(image,large,cv::Size(0,0),5,5,cv::INTER_CUBIC);
	cv::imshow(title,large);
	cv::waitKey(0);
	cvDestroyWindow(title.c_str());
	large.release();
}
//==============================================================================
void Auxiliary::mean0Variance1(cv::Mat &mat,cv::Mat &mean, cv::Mat &var){
	int matType = mat.type();
	mat.convertTo(mat,CV_32FC1);

	// GE THE MEAN AND COVARIANCE
	if(mean.empty() || var.empty()){
		mean = cv::Mat::zeros(cv::Size(mat.cols,1),CV_32FC1);
		var  = cv::Mat::zeros(cv::Size(mat.cols,1),CV_32FC1);
		for(unsigned c=0;c<mat.cols;++c){
			cv::Scalar smean,stddev;
			cv::Mat aCol = mat.col(c);
			cv::meanStdDev(aCol,smean,stddev);
			mean.at<float>(0,c) = smean.val[0];
			var.at<float>(0,c)  = stddev.val[0];
		}
	}

	for(unsigned r=0;r<mat.rows;++r){
		cv::Mat aRow = mat.row(r);
		aRow = (aRow - mean);
		cv::Mat tmp;
		cv::divide(aRow,var,tmp,1);
		tmp.copyTo(aRow);
		tmp.release();
	}
	mat.convertTo(mat,matType);
}
//==============================================================================
/** A function that transforms the data such that it has zero mean and unit
 * variance: img = (img-mean(img(:)))/std(img(:)).
 */
void Auxiliary::mean0Variance1(cv::Mat &mat){
	int matType = mat.type();
	mat.convertTo(mat,CV_32FC1);
	unsigned rows = mat.rows;
	if(rows != 1){
		mat = mat.reshape(0,1);
	}
	cv::Scalar mean,stddev;
	cv::meanStdDev(mat,mean,stddev);
	if(stddev.val[0]){
		mat = (mat-mean.val[0])/stddev.val[0];
	}
	mat.convertTo(mat,CV_32FC1);
	if(rows != 1){
		mat = mat.reshape(0,rows);
	}
	if(matType == CV_8UC1){
		mat *= 255;
		mat.convertTo(mat,CV_8UC1);
	}
}
//==============================================================================
/** Used to sort a vector of points -- compares points on the X coordinate.
 */
bool Auxiliary::isSmallerPointX(const cv::Point2f &p1,const cv::Point2f &p2){
	return (p1.x<p2.x);
}
//==============================================================================
/** Compares 2 keypoints based on their response.
 */
bool Auxiliary::isLargerKey(const cv::KeyPoint &k1,\
const cv::KeyPoint &k2){
	return (k1.response>k2.response);
}
//==============================================================================
/** Compares 2 the lengths of 2 openCV contours (vectors of vectors of cv::Point).
 */
bool Auxiliary::isLongerContours(const std::vector<cv::Point> &c1,\
const std::vector<cv::Point> &c2){
	return (c1.size()<c2.size());
}
//==============================================================================
/** Store the PCA model locally so you can load it next time when you need it.
 */
void Auxiliary::savePCA(const std::tr1::shared_ptr<cv::PCA> pcaPtr,\
const std::string &file){
	CvFileStorage *fs;
	CvMat *eigenvectors,*eigenvalues,*mean;
	fs = cvOpenFileStorage(file.c_str(),0,CV_STORAGE_WRITE);
	eigenvectors = new CvMat(pcaPtr->eigenvectors);
	eigenvalues  = new CvMat(pcaPtr->eigenvalues);
	mean         = new CvMat(pcaPtr->mean);
	cvWrite(fs,"eigenvectors",eigenvectors,cvAttrList(0,0));
	cvWrite(fs,"eigenvalues",eigenvalues,cvAttrList(0,0));
	cvWrite(fs,"mean",mean,cvAttrList(0,0));
	cvReleaseFileStorage(&fs);
	delete eigenvectors;
	delete eigenvalues;
	delete mean;
}
//==============================================================================
/** Load the PCA model locally so you can load it next time when you need it.
 */
std::tr1::shared_ptr<cv::PCA> Auxiliary::loadPCA(const std::string &file){
	CvFileStorage *fs;
	CvMat *eigenvectors,*eigenvalues,*mean;
	std::tr1::shared_ptr<cv::PCA> pcaPtr = std::tr1::shared_ptr<cv::PCA>\
		(new cv::PCA(),Auxiliary::getRidOfPCA);
	fs = cvOpenFileStorage(file.c_str(),0,CV_STORAGE_READ);
	eigenvectors = (CvMat*)cvReadByName(fs,0,"eigenvectors",0);
	eigenvalues  = (CvMat*)cvReadByName(fs,0,"eigenvalues",0);
	mean         = (CvMat*)cvReadByName(fs,0,"mean",0);
	(cv::Mat(eigenvectors)).copyTo(pcaPtr->eigenvectors);
	(cv::Mat(eigenvalues)).copyTo(pcaPtr->eigenvalues);
	(cv::Mat(mean)).copyTo(pcaPtr->mean);
	cvReleaseFileStorage(&fs);
	cvReleaseMat(&eigenvectors);
	cvReleaseMat(&eigenvalues);
	cvReleaseMat(&mean);
	return pcaPtr;
}
//==============================================================================
/** Deallocates a PCA pointed by a pointer.
 */
void Auxiliary::getRidOfPCA(cv::PCA *pca){
	if(pca){
		pca->eigenvalues.release();
		pca->eigenvectors.release();
		pca->mean.release();
		delete pca;
		pca = NULL;
	}
}
//==============================================================================
