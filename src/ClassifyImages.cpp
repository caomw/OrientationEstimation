/* ClassifyImages.cpp
 * Author: Silvia-Laura Pintea
 * Copyright (c) 2010-2011 Silvia-Laura Pintea. All rights reserved.
 * Feel free to use this code,but please retain the above copyright notice.
 */
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <err.h>
#include <exception>
#include "Auxiliary.h"
#include "eigenbackground/src/Tracker.hh"
#include "ClassifyImages.h"
//==============================================================================
ClassifyImages::ClassifyImages(int argc,char **argv,ClassifyImages::USES use){
	// DEAFAULT INITIALIZATION
	this->trainFolder_      = "";
	this->testFolder_       = "";
	this->annotationsTrain_ = "";
	this->annotationsTest_  = "";
	this->noise_            = 0.01;
	this->length_           = 1.0;
	this->kFunction_        = &GaussianProcess::sqexp;
	this->feature_          = FeatureExtractor::EDGES;
	this->foldSize_		   = 5;
	this->modelName_        = "";
	this->what_             = use;
	this->useGroundTruth_   = false;
	this->dimRed_           = true;
	this->plot_             = false;

	// INITIALIZE THE DATA MATRIX AND TARGETS MATRIX AND THE GAUSSIAN PROCESSES
	for(unsigned i=0;i<3;++i){
		this->trainData_.push_back(cv::Mat());
		this->trainTargets_.push_back(cv::Mat());
		this->testData_.push_back(cv::Mat());
		this->testTargets_.push_back(cv::Mat());

		this->gpSin_.push_back(GaussianProcess());
		this->gpCos_.push_back(GaussianProcess());
	}

	// READ THE COMMAND LINE ARGUMENTS
	if(argc != 3 && argc != 5){
		cerr<<"Usage: classifier datasetFolder/ textOfImageName [testsetFolder/ "<<\
			"textOfTestImageName] \n"<<\
			"datasetFolder/ and testsetFolder -- contain: \n"<<\
			"\t train: 'annotated_train/'\n "<<\
			"\t train targets: 'annotated_train.txt'\n "<<\
			"\t [test: 'annotated_test/']\n "<<\
			"\t [test targets: 'annotated_test.txt']\n "<<\
			"\t calibration: 'CALIB_textOfImageName.txt'\n "<<\
			"\t prior: 'PRIOR_textOfImageName.txt'\n"<<\
			"\t background: 'BG_textOfImageName.bin'\n"<<\
			"\t [SIFT data: 'annotated_SIFT/']\n"<<\
			"\t [SIFT dictionary: 'SIFT_textOfImageName.bin']\n"<<std::endl;
		exit(1);
	}else{
		this->trainDir_       = std::string(argv[1]);
		this->trainImgString_ = std::string(argv[2]);
		if(this->trainDir_[this->trainDir_.size()-1]!='/'){
			this->trainDir_ += '/';
		}
		if(argc == 5){
			this->testDir_ = std::string(argv[3]);
			if(this->testDir_[this->testDir_.size()-1]!='/'){
				this->testDir_ += '/';
			}
			this->testImgString_ = std::string(argv[4]);
		}

		std::vector<std::string> files2check;
		switch(this->what_){
			case(ClassifyImages::TEST):
				if(argc != 5){
					std::cerr<<"4 Arguments are needed for the final test: "<<\
						"classifier datasetFolder/ textOfImageName [testsetFolder/ "<<\
						"textOfImageNameTest]"<<std::endl;
					exit(1);
				}
				// IF WE WANT TO TEST THE FINAL CLASIFIER'S PERFORMANCE
				this->trainFolder_      = this->trainDir_+"annotated_train/";
				this->annotationsTrain_ = this->trainDir_+"annotated_train.txt";
				this->testFolder_       = this->testDir_+"annotated_test/";
				this->annotationsTest_  = this->testDir_+"annotated_test.txt";
				files2check.push_back(this->trainFolder_);
				files2check.push_back(this->annotationsTrain_);
				files2check.push_back(this->testFolder_);
				files2check.push_back(this->annotationsTest_);
				break;
			case(ClassifyImages::EVALUATE):
				// IF WE WANT TO EVALUATE WITH CORSSVALIDATION
				this->trainFolder_      = this->trainDir_+"annotated_train/";
				this->annotationsTrain_ = this->trainDir_+"annotated_train.txt";
				files2check.push_back(this->trainFolder_);
				files2check.push_back(this->annotationsTrain_);
				break;
			case(ClassifyImages::BUILD_DICTIONARY):
				// IF WE WANT TO BUILD SIFT DICTIONARY
				this->trainFolder_ = this->trainDir_+"annotated_SIFT/";
				this->annotationsTrain_ = this->trainDir_+"annotated_SIFT.txt";
				files2check.push_back(this->annotationsTrain_);
				files2check.push_back(this->trainFolder_);
				break;
		}
		for(std::size_t i=0;i<files2check.size();++i){
			if(!Helpers::file_exists(files2check[i].c_str())){
				std::cerr<<"File/folder not found: "<<files2check[i]<<std::endl;
				exit(1);
			}
		}

		if(use == ClassifyImages::TEST){
			this->modelName_ = "data/TEST/";
		}else if(use == ClassifyImages::EVALUATE){
			this->modelName_ = "data/EVALUATE/";
		}
		Helpers::file_exists(this->modelName_.c_str(),true);
	}
}
//==============================================================================
ClassifyImages::~ClassifyImages(){
	if(this->features_){
		this->features_.reset();
	}

	// LOOP ONLY ONCE (ALL HAVE 3 CLASSES)
	for(std::size_t i=0;i<this->trainData_.size();++i){
		if(!this->trainData_[i].empty()){
			this->trainData_[i].release();
		}
		if(!this->testData_[i].empty()){
			this->testData_[i].release();
		}
		if(!this->trainTargets_[i].empty()){
			this->trainTargets_[i].release();
		}
		if(!this->testTargets_[i].empty()){
			this->testTargets_[i].release();
		}
	}
	this->trainData_.clear();
	this->trainTargets_.clear();
	this->testData_.clear();
	this->testTargets_.clear();

	this->gpSin_.clear();
	this->gpCos_.clear();
	if(this->pca_){
		this->pca_.reset();
	}
}
//==============================================================================
/** Initialize the options for the Gaussian Process regression.
 */
void ClassifyImages::init(float theNoise,float theLength,\
FeatureExtractor::FEATURE theFeature,GaussianProcess::kernelFunction theKFunction,\
bool toUseGT){
	this->noise_          = theNoise;
	this->length_         = theLength;
	this->kFunction_      = theKFunction;
	this->feature_        = theFeature;
	this->useGroundTruth_ = toUseGT;
	switch(this->feature_){
		case(FeatureExtractor::IPOINTS):
			this->modelName_ += "IPOINTS/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::EDGES):
			this->modelName_     += "EDGES/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::SURF):
			this->modelName_ += "SURF/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::GABOR):
			this->modelName_ += "GABOR/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::SIFT):
			this->modelName_ += "SIFT/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::TEMPL_MATCHES):
			this->modelName_ += "TEMPL_MATCHES/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::RAW_PIXELS):
			this->modelName_ += "RAW_PIXELS/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
		case(FeatureExtractor::HOG):
			this->modelName_ += "HOG/";
			Helpers::file_exists(this->modelName_.c_str(),true);
			break;
	}
}
//==============================================================================
/** Concatenate the loaded data from the files to the currently computed data.
 */
void ClassifyImages::loadData(const cv::Mat &tmpData1,const cv::Mat &tmpTargets1,\
unsigned i,cv::Mat &outData,cv::Mat &outTargets){
	// LOAD THE DATA AND TARGET MATRIX FROM THE FILE IF IT'S THERE
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");

	cv::Mat tmpData2,tmpTargets2;
	std::string modelDataName    = this->modelName_+names[i]+"/Data.bin";
	std::string modelTargetsName = this->modelName_+names[i]+"/Labels.bin";
	if(Helpers::file_exists(modelDataName.c_str())){
		Auxiliary::binFile2mat(tmpData2,const_cast<char*>(modelDataName.c_str()));
	}else{
		tmpData2 = cv::Mat();
	}
	if(Helpers::file_exists(modelTargetsName.c_str())){
		Auxiliary::binFile2mat(tmpTargets2,const_cast<char*>(modelTargetsName.c_str()));
	}else{
		tmpTargets2 = cv::Mat();
	}

	// NOW COPY THEM TOGETHER INTO A FINAL MATRIX AND STORE IT.
	if(!tmpData1.empty() && !tmpData2.empty()){
		if(tmpData1.cols!=tmpData2.cols){
			std::cerr<<"The sizes of the stored data matrix and the newly generated "\
				<<"data matrix do not agree: "<<tmpData1.size()<<" VS. "<<\
				tmpData2.size()<<std::endl;
			exit(1);
		}
	}
	// IF ONE OF THE MATRIXES IS EMPTY USE THE OTHER ONE TO GET THE COLS
	cv::Mat dumData1,dumData2,dumTargets1,dumTargets2;
	unsigned colsData     = std::max(tmpData2.cols,tmpData1.cols);
	unsigned colsTargets  = std::max(tmpTargets2.cols,tmpTargets1.cols);
	outData               = cv::Mat::zeros(cv::Size(colsData,tmpData1.rows+\
		tmpData2.rows),CV_32FC1);
	outTargets            = cv::Mat::zeros(cv::Size(colsTargets,tmpTargets1.rows+\
		tmpTargets2.rows),CV_32FC1);

	// COPY DATA1 AND TARGETS1 TO THE DATA MATRIX
	if(!tmpData1.empty() && !tmpTargets1.empty()){
		dumData1 = outData.rowRange(0,tmpData1.rows);
		tmpData1.copyTo(dumData1);
		dumTargets1 = outTargets.rowRange(0,tmpTargets1.rows);
		tmpTargets1.copyTo(dumTargets1);
	}

	// COPY DATA2 AND TARGETS2 TO THE DATA MATRIX
	if(!tmpData2.empty() && !tmpTargets2.empty()){
		dumData2 = outData.rowRange(tmpData1.rows,tmpData1.rows+tmpData2.rows);
		tmpData2.copyTo(dumData2);
		dumTargets2 = outTargets.rowRange(tmpTargets1.rows,tmpTargets1.rows+\
			tmpTargets2.rows);
		tmpTargets2.copyTo(dumTargets2);
	}
	outData.convertTo(outData,CV_32FC1);
	outTargets.convertTo(outTargets,CV_32FC1);
	std::cout<<names[i]<<" data: "<<outData.size()<<"=("<<tmpData1.size()<<\
		"+"<<tmpData2.size()<<")"<<std::endl;
	std::cout<<names[i]<<" targets: "<<outTargets.size()<<"=("<<\
		tmpTargets1.size()<<"+"<<tmpTargets2.size()<<")"<<std::endl;

	// RELEASE THE BILLION OF ALLOCATED MATRIXES
	dumData1.release();
	dumData2.release();
	dumTargets1.release();
	dumTargets2.release();
	tmpData2.release();
	tmpTargets2.release();
}
//==============================================================================
/** Creates the training data (according to the options),the labels and
 * trains the a \c GaussianProcess on the data.
 */
void ClassifyImages::trainGP(AnnotationsHandle::POSE what,bool fromFolder){
	// TRAIN ALL 3 CLASSES WITH DATA THAT WE HAVE,PREDICT ONLY ON THE GOOD CLASS
	this->features_->init(this->trainFolder_,this->annotationsTrain_,this->feature_,\
		fromFolder);
	this->features_->start(fromFolder,this->useGroundTruth_);

	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		if(!this->trainData_[i].empty()){
			this->trainData_[i].release();
		}
		if(!this->trainTargets_[i].empty()){
			this->trainTargets_[i].release();
		}
		this->gpSin_[i].init(this->kFunction_);
		this->gpCos_[i].init(this->kFunction_);

		// IF WE CANNOT LOAD DATA,THEN WE BUILD IT
		std::string modelNameData   = this->modelName_+"Data.bin";
		std::string modelNameLabels = this->modelName_+"Labels.bin";

		cv::Mat tmpData,tmpTargets;
		if(this->dimRed_ && !this->features_->data()[i].empty()){
			tmpData = this->reduceDimensionality(this->features_->data()[i],true,20);
		}else{
			this->features_->data()[i].copyTo(tmpData);
		}
		this->features_->targets()[i].copyTo(tmpTargets);
		tmpData.convertTo(tmpData,CV_32FC1);
		tmpTargets.convertTo(tmpTargets,CV_32FC1);
		cv::Mat outData,outTargets;
		this->loadData(tmpData,tmpTargets,i,outData,outTargets);
		outTargets.copyTo(this->trainTargets_[i]);
		outData.copyTo(this->trainData_[i]);
		outData.release();
		outTargets.release();
		tmpData.release();
		tmpTargets.release();
		if(this->trainData_[i].empty()) continue;

		// CHECK TO SEE IF THERE IS ANY DATA IN THE CURRENT CLASS
		assert(this->trainData_[i].rows==this->trainTargets_[i].rows);

		// TRAIN THE SIN AND COS SEPARETELY FOR LONGITUDE || LATITUDE
		if(what == AnnotationsHandle::LONGITUDE){
			cv::Mat sinTargets = this->trainTargets_[i].col(0);
			cv::Mat cosTargets = this->trainTargets_[i].col(1);
			this->gpSin_[i].train(this->trainData_[i],sinTargets,\
				this->kFunction_,static_cast<_float>(this->noise_),\
				static_cast<_float>(this->length_));
			this->gpCos_[i].train(this->trainData_[i],cosTargets,\
				this->kFunction_,static_cast<_float>(this->noise_),\
				static_cast<_float>(this->length_));
			sinTargets.release();
			cosTargets.release();
		}else if(what == AnnotationsHandle::LATITUDE){
			// TRAIN THE SIN AND COS SEPARETELY FOR LATITUDE
			cv::Mat sinTargets = this->trainTargets_[i].col(2);
			cv::Mat cosTargets = this->trainTargets_[i].col(3);
			this->gpSin_[i].train(this->trainData_[i],sinTargets,\
				this->kFunction_,static_cast<_float>(this->noise_),\
				static_cast<_float>(this->length_));
			this->gpCos_[i].train(this->trainData_[i],cosTargets,\
				this->kFunction_,static_cast<_float>(this->noise_),\
				static_cast<_float>(this->length_));
			sinTargets.release();
			cosTargets.release();
		}
	}
}
//==============================================================================
/** Just build data matrix and store it;it can be called over multiple datasets
 * by adding the the new data rows at the end to the stored matrix.
 */
void ClassifyImages::buildDataMatrix(int colorSp,FeatureExtractor::FEATUREPART part){
	// SET THE CALIBRATION AND OTHER FEATURE SETTINGS
	this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp,part);
	for(std::size_t i=0;i<this->trainData_.size();++i){
		if(!this->trainData_[i].empty()){
			this->trainData_[i].release();
		}
		if(!this->trainTargets_[i].empty()){
			this->trainTargets_[i].release();
		}
	}

	this->features_->init(this->trainFolder_,this->annotationsTrain_,\
		this->feature_,true);
	this->features_->start(true,this->useGroundTruth_);
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		// CHECK TO SEE IF THE FOLDER IS ALREADY CREATED
		Helpers::file_exists((this->modelName_+names[i]).c_str(),true);
		std::string modelNameData   = this->modelName_+names[i]+"/Data.bin";
		std::string modelNameLabels = this->modelName_+names[i]+"/Labels.bin";

		// LOAD THE DATA AND TARGET MATRIX FROM THE FILE IF IT'S THERE
		cv::Mat tmpData1,tmpTargets1,tmpData2,tmpTargets2;
		if(Helpers::file_exists(modelNameData.c_str())){
			Auxiliary::binFile2mat(tmpData1,const_cast<char*>(modelNameData.c_str()));
		}else{
			tmpData1 = cv::Mat();
		}
		if(Helpers::file_exists(modelNameLabels.c_str())){
			Auxiliary::binFile2mat(tmpTargets1,const_cast<char*>(modelNameLabels.c_str()));
		}else{
			tmpTargets1 = cv::Mat();
		}

		// NOW CREATE NEW DATA AND TARGETS AND ADD THEM TO THE OLD ONES
		this->features_->data()[i].copyTo(tmpData2);
		this->features_->targets()[i].copyTo(tmpTargets2);

		// NOW COPY THEM TOGETHER INTO A FINAL MATRIX AND STORE IT.
		if(!tmpData1.empty() && !tmpData2.empty()){
			if(tmpData1.cols!=tmpData2.cols){
				std::cerr<<"The sizes of the stored data matrix and the newly generated "\
					<<"data matrix do not agree: "<<tmpData1.size()<<" VS. "<<\
					tmpData2.size()<<std::endl;
				exit(1);
			}
		}
		cv::Mat dumData1,dumData2,dumTargets1,dumTargets2;
		this->trainData_[i]    = cv::Mat::zeros(cv::Size(tmpData2.cols,tmpData1.rows+\
			tmpData2.rows),CV_32FC1);
		this->trainTargets_[i] = cv::Mat::zeros(cv::Size(tmpTargets2.cols,\
			tmpTargets1.rows+tmpTargets2.rows),CV_32FC1);
		if(!tmpData1.empty()){
			dumData1 = this->trainData_[i].rowRange(0,tmpData1.rows);
			tmpData1.copyTo(dumData1);
			dumTargets1 = this->trainTargets_[i].rowRange(0,tmpTargets1.rows);
			tmpTargets1.copyTo(dumTargets1);
		}
		dumData2 = this->trainData_[i].rowRange(tmpData1.rows,tmpData1.rows+tmpData2.rows);
		tmpData2.copyTo(dumData2);
		dumTargets2 = this->trainTargets_[i].rowRange(tmpTargets1.rows,tmpTargets1.rows+\
			tmpTargets2.rows);
		tmpTargets2.copyTo(dumTargets2);

		// WRITE THE FINAL MATRIX TO THE FILES
		if(!this->trainData_[i].empty()){
			Auxiliary::mat2BinFile(this->trainData_[i],const_cast<char*>\
				(modelNameData.c_str()),false);
			Auxiliary::mat2BinFile(this->trainTargets_[i],const_cast<char*>\
				(modelNameLabels.c_str()),false);
			std::cout<<"Data size: "<<this->trainData_[i].size()<<std::endl;
			std::cout<<"Labels size: "<<this->trainTargets_[i].size()<<std::endl;
			std::cout<<"Data stored to: "<<modelNameData<<" and "<<\
				modelNameLabels<<std::endl;
		}
		// RELEASE THE BILLION OF ALLOCATED MATRIXES
		dumData1.release();
		dumData2.release();
		dumTargets1.release();
		dumTargets2.release();
		tmpData1.release();
		tmpTargets1.release();
		tmpData2.release();
		tmpTargets2.release();
	}
}
//==============================================================================
/** Creates the test data and applies \c GaussianProcess prediction on the test
 * data.
 */
std::deque<std::deque<float> > ClassifyImages::predictGP\
(std::deque<GaussianProcess::prediction> &predictionsSin,\
std::deque<GaussianProcess::prediction> &predictionsCos,\
AnnotationsHandle::POSE what,bool fromFolder){
	this->features_->init(this->testFolder_,this->annotationsTest_,\
		this->feature_,fromFolder);
	this->features_->start(fromFolder,this->useGroundTruth_);

	// FOR TESTING WE ALWAYS BUILT THE DATA (THERE IS NOT SAVED MODEL)
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	std::deque<std::deque<float> > predictions;
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		if(!this->testData_[i].empty()){
			this->testData_[i].release();
		}
		if(!this->testTargets_[i].empty()){
			this->testTargets_[i].release();
		}
		// CHECK TO SEE IF THERE IS ANY DATA IN THE CURRENT CLASS
		std::deque<float> oneClassPredictions;
		if(this->features_->data()[i].empty() || this->gpSin_[i].empty() ||\
		this->gpCos_[i].empty()){
			predictions.push_back(oneClassPredictions);
			continue;
		}
		if(this->dimRed_){
			this->testData_[i] = this->reduceDimensionality(this->features_->data()[i],\
				false,20);
		}else{
			this->features_->data()[i].copyTo(this->testData_[i]);
		}

		// GET ONLY THE ANGLES YOU NEED
		if(what == AnnotationsHandle::LONGITUDE){
			cv::Mat dum = this->features_->targets()[i].colRange(0,2);
			dum.copyTo(this->testTargets_[i]);
			dum.release();
		}else if(what == AnnotationsHandle::LATITUDE){
			cv::Mat dum = this->features_->targets()[i].colRange(2,4);
			dum.copyTo(this->testTargets_[i]);
			dum.release();
		}
		assert(this->testData_[i].rows==this->testTargets_[i].rows);
		this->testData_[i].convertTo(this->testData_[i],CV_32FC1);
		this->testTargets_[i].convertTo(this->testTargets_[i],CV_32FC1);

		// FOR EACH ROW IN THE TEST MATRIX PREDICT
		for(int j=0;j<this->testData_[i].rows;++j){
			GaussianProcess::prediction prediSin;
			cv::Mat testRow = this->testData_[i].row(j);
			this->gpSin_[i].predict(testRow,prediSin,static_cast<_float>(this->length_));
			GaussianProcess::prediction prediCos;
			this->gpCos_[i].predict(testRow,prediCos,static_cast<_float>(this->length_));
			predictionsSin.push_back(prediSin);
			predictionsCos.push_back(prediCos);
			oneClassPredictions.push_back(this->optimizePrediction(prediSin,prediCos));
			prediSin.mean_.clear();
			prediSin.variance_.clear();
			prediCos.mean_.clear();
			prediCos.variance_.clear();
		}
		predictions.push_back(oneClassPredictions);
	}
	return predictions;
}
//==============================================================================
/** Evaluate one prediction versus its target.
 */
void ClassifyImages::evaluate(const std::deque<std::deque<float> > &prediAngles,\
float &error,float &normError,float &meanDiff){
	error = 0.0;normError = 0.0;meanDiff = 0.0;
	unsigned noPeople = 0;
	std::deque<std::string> names;

	cv::Mat bins = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);

	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		std::cout<<"Class "<<names[i]<<": "<<this->testTargets_[i].size()<<\
			" people"<<std::endl;
		assert(this->testTargets_[i].rows == prediAngles[i].size());
		for(int y=0;y<this->testTargets_[i].rows;++y){
			float targetAngle = std::atan2(this->testTargets_[i].at<float>(y,0),\
				this->testTargets_[i].at<float>(y,1));
			Auxiliary::angle0to360(targetAngle);

			std::cout<<"Target: "<<targetAngle<<"("<<(targetAngle*180.0/M_PI)<<\
				") VS "<<prediAngles[i][y]<<"("<<(prediAngles[i][y]*180.0/M_PI)<<\
				")"<<std::endl;
			float absDiff = std::abs(targetAngle-prediAngles[i][y]);
			if(absDiff > M_PI){
				absDiff = 2.0*M_PI - absDiff;
			}

			int ind = ((absDiff*180.0/M_PI)/10);
			++bins.at<float>(0,ind);

			std::cout<<"Difference: "<< absDiff <<std::endl;
			error     += absDiff*absDiff;
			normError += (absDiff*absDiff)/(M_PI*M_PI);
			meanDiff  += absDiff;
		}
		noPeople += this->testTargets_[i].rows;
	}
	std::cout<<"Number of people: "<<noPeople<<std::endl;
	error     = std::sqrt(error/(noPeople));
	normError = std::sqrt(normError/(noPeople));
	meanDiff  = meanDiff/(noPeople);

	std::cout<<"RMS-error normalized: "<<normError<<std::endl;
	std::cout<<"RMS-accuracy normalized: "<<(1-normError)<<std::endl;
	std::cout<<"RMS-error: "<<error<<std::endl;
	std::cout<<"Avg-Radians-Difference: "<<meanDiff<<std::endl;
	std::cout<<"bins=["<<bins<<"]"<<std::endl;
	bins.release();
}
//==============================================================================
/** Try to optimize the prediction of the angle considering the variance of sin
 * and cos.
 */
float ClassifyImages::optimizePrediction(const GaussianProcess::prediction \
&predictionsSin,const GaussianProcess::prediction &predictionsCos){
	float y          = predictionsSin.mean_[0];
	float x          = predictionsCos.mean_[0];
	float prediction = std::atan2(y,x);
	Auxiliary::angle0to360(prediction);
	return prediction;
/*
	float betaS = 1.0/(predictionsSin.variance[0]);
	float betaC = 1.0/(predictionsCos.variance[0]);
	float y     = predictionsSin.mean[0];
	float x     = predictionsCos.mean[0];

	if(betaS == betaC){
		return std::atan2(betaS*y,betaC*x);
	}else{
		return std::atan2(y,x);
	}
*/
	/*
	float closeTo;
	closeTo = std::atan2(predictionsSin.mean[0],predictionsCos.mean[0]);
	std::deque<float> alphas;
	if(betaS != betaC){
		std::cout<<"betaS="<<betaS<<" betaC="<<betaC<<" x="<<x<<" y="<<y<<std::endl;

		float b = -1.0*(betaS*x + betaC*y + betaS - betaC);
		float a = betaS - betaC;
		float c = betaS*x;

		std::cout<<"b="<<b<<" a="<<a<<" c="<<c<<std::endl;
		std::cout<<"alpha1: "<<((-b + std::sqrt(b*b - 4.0*a*c))/2.0*a)<<std::endl;
		std::cout<<"alpha2: "<<((-b - std::sqrt(b*b - 4.0*a*c))/2.0*a)<<std::endl;

		alphas.push_back((-b + std::sqrt(b*b - 4.0*a*c))/2.0*a);
		alphas.push_back((-b - std::sqrt(b*b - 4.0*a*c))/2.0*a);
	}else{
		std::cout<<"alpha1: "<<(betaS*x/(betaS*x + betaC*y))<<std::endl;
		alphas.push_back(betaS*x/(betaS*x + betaC*y));
	}
	float minDist = 2.0*M_PI,minAngle;
	for(unsigned i=0;i<alphas.size();++i){
		if(alphas[i]>=0){
			float alpha1 = std::asin(std::sqrt(alphas[i]));
			float alpha2 = std::asin(-std::sqrt(alphas[i]));
			if(std::abs(alpha1-closeTo)<minDist){
				minDist  = std::abs(alpha1-closeTo);
				minAngle = alpha1;
			}
			if(std::abs(alpha2-closeTo)<minDist){
				minDist  = std::abs(alpha2-closeTo);
				minAngle = alpha2;
			}
		}
	}
	return minAngle;
	*/
}
//==============================================================================
/** Build dictionary for vector quantization.
 */
void ClassifyImages::buildDictionary(int colorSp,bool toUseGT){
	// SET THE CALIBRATION AND OTHER FEATURE SETTINGS
	this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp);

	// EXTRACT THE SIFT FEATURES AND CONCATENATE THEM
	this->features_->init(this->trainFolder_,this->annotationsTrain_,\
		FeatureExtractor::SIFT_DICT,true);
	this->features_->start(true,toUseGT);

	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		if(this->features_->data()[i].empty()) continue;
		cv::Mat dictData;
		this->features_->data()[i].copyTo(dictData);
		this->features_->extractor()->setImageClass(static_cast<unsigned>(i));

		// DO K-means IN ORDER TO RETRIEVE BACK THE CLUSTER MEANS
		cv::Mat labels = cv::Mat::zeros(cv::Size(1,dictData.rows),CV_32FC1);

		//LABEL EACH SAMPLE ASSIGNMENT
		cv::Mat* centers = new cv::Mat(cv::Size(dictData.cols,\
			this->features_->extractor()->readNoMeans()),CV_32FC1);
		dictData.convertTo(dictData,CV_32FC1);
		cv::kmeans(dictData,this->features_->extractor()->readNoMeans(),labels,\
			cv::TermCriteria(cv::TermCriteria::MAX_ITER|cv::TermCriteria::EPS,2,1),\
			5,cv::KMEANS_RANDOM_CENTERS,centers);
		dictData.release();
		labels.release();

		// WRITE TO FILE THE MEANS
		cv::Mat matrix(*centers);
		std::string dictName = this->features_->extractor()->readDictName();
		std::cout<<"Size("<<names[i]<<"): "<<this->features_->data()[i].size()<<\
			" stored in: "<<dictName<<std::endl;

		Auxiliary::mat2BinFile(matrix,const_cast<char*>(dictName.c_str()));
		centers->release();
		matrix.release();
		delete centers;
	}
}
//==============================================================================
/** Does the cross-validation and computes the average error over all folds.
 */
float ClassifyImages::runCrossValidation(unsigned k,AnnotationsHandle::POSE what,\
int colorSp,bool onTrain,FeatureExtractor::FEATUREPART part){
	float finalError=0.0,finalNormError=0.0,finalMeanDiff=0.0;

	// SET THE CALIBRATION ONLY ONCE (ALL IMAGES ARE READ FROM THE SAME DIR)
	this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp,part);
	for(unsigned i=0;i<k;++i){
		std::cout<<"Round "<<i<<"___________________________________________"<<\
			"_____________________________________________________"<<std::endl;
		// SPLIT TRAINING AND TESTING ACCORDING TO THE CURRENT FOLD
		std::deque<GaussianProcess::prediction> predictionsSin;
		std::deque<GaussianProcess::prediction> predictionsCos;
		std::deque<std::deque<float> > predicted;
		this->crossValidation(k,i,onTrain);
		//______________________________________________________________________
		if(what == AnnotationsHandle::LONGITUDE){
			//LONGITUDE TRAINING AND PREDICTING
			std::cout<<"Longitude >>> "<<i<<"___________________________________"<<\
				"_____________________________________________________"<<std::endl;
			this->trainGP(AnnotationsHandle::LONGITUDE,false);

			// PREDICT ON THE REST OF THE IMAGES
			predicted = this->predictGP(predictionsSin,predictionsCos,\
				AnnotationsHandle::LONGITUDE,false);

			// EVALUATE PREDICITONS
			float errorLong,normErrorLong,meanDiffLong;
			this->evaluate(predicted,errorLong,normErrorLong,meanDiffLong);
			finalError     += errorLong;
			finalNormError += normErrorLong;
			finalMeanDiff  += meanDiffLong;
			predictionsSin.clear();
			predictionsCos.clear();
			predicted.clear();
		//______________________________________________________________________
		}else if(what == AnnotationsHandle::LATITUDE){
			//LATITUDE TRAINING AND PREDICTING
			std::cout<<"Latitude >>> "<<i<<"____________________________________"<<\
				"_____________________________________________________"<<std::endl;
			this->trainGP(AnnotationsHandle::LATITUDE,false);

			// PREDICT ON THE REST OF THE IMAGES
			predicted = this->predictGP(predictionsSin,predictionsCos,\
				AnnotationsHandle::LATITUDE,false);

			// EVALUATE PREDICITONS
			float errorLat,normErrorLat,meanDiffLat;
			this->evaluate(predicted,errorLat,normErrorLat,meanDiffLat);
			finalError     += errorLat;
			finalNormError += normErrorLat;
			finalMeanDiff  += meanDiffLat;
			predictionsSin.clear();
			predictionsCos.clear();
			predicted.clear();
		}
		sleep(6);

//------------------REMOVE------------------------------------------------------
		break;
//------------------REMOVE------------------------------------------------------

	}
	finalError     /= static_cast<float>(k);
	finalNormError /= static_cast<float>(k);
	finalMeanDiff  /= static_cast<float>(k);
	std::cout<<">>> final-RMS-error:"<<finalError<<std::endl;
	std::cout<<">>> final-RMS-normalized-error:"<<finalNormError<<std::endl;
	std::cout<<">>> final-avg-difference:"<<finalMeanDiff<<std::endl;
	return finalNormError;
}
//==============================================================================
/** Do k-fold cross-validation by splitting the training folder into training-set
 * and validation-set.
 */
void ClassifyImages::crossValidation(unsigned k,unsigned fold,bool onTrain){
	// READ ALL IMAGES ONCE AND NOT THEY ARE SORTED
	if(this->imageList_.empty()){
		this->imageList_ = Helpers::readImages(this->trainFolder_.c_str());
		this->foldSize_  = this->imageList_.size()/k;

		std::ifstream annoIn(this->annotationsTrain_.c_str());
		if(annoIn.is_open()){
			while(annoIn.good()){
				std::string line;
				std::getline(annoIn,line);
				if(!line.empty()){
					this->annoList_.push_back(line);
				}
				line.clear();
			}
			annoIn.close();
		}
		std::sort(this->annoList_.begin(),this->annoList_.end(),(&Helpers::sortAnnotations));
		if(this->annoList_.size()!=this->imageList_.size()){
			std::cerr<<"The number of images != The number of annotations!"<<\
				std::endl;
			exit(1);
		}
	}

	// DEFINE THE FOLDERS WERE THE TEMPORARY FILES NEED TO BE STORED
	unsigned pos       = this->trainFolder_.find_first_of("/\\");
	std::string root   = this->trainFolder_.substr(0,pos+1);
	std::string folder = root+"trash/";
	Helpers::file_exists(folder.c_str(),true);
	this->trainFolder_      = root+"trash/targets.txt";
	this->annotationsTrain_ = root+"trash/annoTargets.txt";
	this->testFolder_       = root+"trash/ttargets.txt";
	this->annotationsTest_  = root+"trash/annoTtargets.txt";

	// WRITE THE IMAGE NAMES & ANNOTATIONS IN THE CORRESPONDING FILES
	std::ofstream testOut,trainOut,annoTest,annoTrain;
	testOut.open(this->testFolder_.c_str(),std::ios::out);
	if(!testOut){
		errx(1,"Cannot open file %s",this->testFolder_.c_str());
	}
	trainOut.open(this->trainFolder_.c_str(),std::ios::out);
	if(!trainOut){
		errx(1,"Cannot open file %s",this->trainFolder_.c_str());
	}

	annoTest.open(this->annotationsTest_.c_str(),std::ios::out);
	if(!annoTest){
		errx(1,"Cannot open file %s",this->annotationsTest_.c_str());
	}
	annoTrain.open(this->annotationsTrain_.c_str(),std::ios::out);
	if(!annoTrain){
		errx(1,"Cannot open file %s",this->annotationsTrain_.c_str());
	}

	for(unsigned i=0;i<this->imageList_.size();++i){
		if(i>=(this->foldSize_*fold) && i<(this->foldSize_*(fold+1))){
			testOut<<this->imageList_[i]<<std::endl;
			annoTest<<this->annoList_[i]<<std::endl;
		}else{
			trainOut<<this->imageList_[i]<<std::endl;
			annoTrain<<this->annoList_[i]<<std::endl;
		}
	}

	testOut.close();
	trainOut.close();
	annoTest.close();
	annoTrain.close();
	if(onTrain){
		this->testFolder_      = root+"trash/targets.txt";
		this->annotationsTest_ = root+"trash/annoTargets.txt";
	}
}
//==============================================================================
/** Reset the features_ object when the training and testing might have different
 * calibration,background models...
 */
void ClassifyImages::resetFeatures(const std::string &dir,const std::string &imStr,\
int colorSp,FeatureExtractor::FEATUREPART part){
	if(this->features_){
		this->features_.reset();
	}
	char** args = new char*[3];
	args[0] = const_cast<char*>("PeopleDetector");
	args[1] = const_cast<char*>(dir.c_str());
	args[2] = const_cast<char*>(imStr.c_str());
	this->features_ = std::tr1::shared_ptr<PeopleDetector>\
		(new PeopleDetector(3,args,false,false,colorSp,part));
	delete [] args;
}
//==============================================================================
/** Runs the final evaluation (test).
 */
std::deque<std::deque<float> > ClassifyImages::runTest(int colorSp,\
AnnotationsHandle::POSE what,float &normError){
	// LONGITUDE TRAINING AND PREDICTING
	std::deque<GaussianProcess::prediction> predictionsSin;
	std::deque<GaussianProcess::prediction> predictionsCos;
	std::deque<std::deque<float> > predicted;
	if(what == AnnotationsHandle::LONGITUDE){
		std::cout<<"Longitude >>> ______________________________________________"<<\
			"_____________________________________________________"<<std::endl;
		// BEFORE TRAINING CAMERA CALIBRATION AND OTHER SETTINGS MIGHT NEED TO BE RESET
		this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp);
		this->trainGP(AnnotationsHandle::LONGITUDE,true);

		// BEFORE TESTING CAMERA CALIBRATION AND OTHER SETTINGS MIGHT NEED TO BE RESET
		this->resetFeatures(this->testDir_,this->testImgString_,colorSp);
		predicted = this->predictGP(predictionsSin,predictionsCos,\
			AnnotationsHandle::LONGITUDE,true);

		// EVALUATE PREDICTIONS
		float errorLong,normErrorLong,meanDiffLong;
		this->evaluate(predicted,errorLong,normErrorLong,meanDiffLong);
		normError = normErrorLong;
	}else if(what == AnnotationsHandle::LATITUDE){
		//__________________________________________________________________________
		// LATITUDE TRAINING AND PREDICTING
		std::cout<<"Latitude >>> _______________________________________________"<<\
			"_____________________________________________________"<<std::endl;
		// BEFORE TRAINING CAMERA CALIBRATION AND OTHER SETTINGS MIGHT NEED TO BE RESET
		this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp);
		this->trainGP(AnnotationsHandle::LATITUDE,true);

		// BEFORE TESTING CAMERA CALIBRATION AND OTHER SETTINGS MIGHT NEED TO BE RESET
		this->resetFeatures(this->testDir_,this->testImgString_,colorSp);
		predicted = this->predictGP(predictionsSin,predictionsCos,\
			AnnotationsHandle::LATITUDE,true);

		// EVALUATE PREDICTIONS
		float errorLat,normErrorLat,meanDiffLat;
		this->evaluate(predicted,errorLat,normErrorLat,meanDiffLat);
		normError = normErrorLat;
	}
	return predicted;
}
//==============================================================================
/** Get the minimum and maximum angle given the motion vector.
 */
void ClassifyImages::getAngleLimits(unsigned classNo,unsigned predNo,\
float &angleMin,float &angleMax){
	if(this->features_->dataMotionVectors()[classNo][predNo] == -1.0){
		angleMax = 2*M_PI;
		angleMin = 0.0;
	}else{
		angleMin = this->features_->dataMotionVectors()[classNo][predNo]-M_PI/2.0;
		Auxiliary::angle0to360(angleMin);
		angleMax = this->features_->dataMotionVectors()[classNo][predNo]+M_PI/2.0;
		Auxiliary::angle0to360(angleMax);
		if(angleMin>angleMax){
			float aux = angleMin;
			angleMin  = angleMax;
			angleMax  = aux;
		}
	}
}
//==============================================================================
/** Combine the output of multiple classifiers (only on testing,no multiple
 * predictions).
 */
void multipleClassifier(int colorSp,AnnotationsHandle::POSE what,\
ClassifyImages &classi,float noise,float length,GaussianProcess::kernelFunction \
kernel,bool useGT){
	classi.init(noise,length,FeatureExtractor::IPOINTS,kernel,useGT);

	std::deque<std::deque<std::deque<float> > > predictions;
	std::deque<std::deque<float> > tmpPrediction;
	float dummy = 0;
	switch(classi.feature_){
		case(FeatureExtractor::IPOINTS):
			classi.feature_ = FeatureExtractor::IPOINTS;
			tmpPrediction = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
/*
		case(FeatureExtractor::EDGES):
			classi.feature_ = FeatureExtractor::EDGES;
			tmpPrediction  = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
		case(FeatureExtractor::SURF):
			classi.feature_ = FeatureExtractor::SURF;
			tmpPrediction  = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
		case(FeatureExtractor::GABOR):
			classi.feature_ = FeatureExtractor::GABOR;
			tmpPrediction  = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
		case(FeatureExtractor::SIFT):
			classi.feature_ = FeatureExtractor::SIFT;
			tmpPrediction  = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
		case(FeatureExtractor::PIXELS):
			classi.feature_ = FeatureExtractor::PIXELS;
			tmpPrediction  = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
		case(FeatureExtractor::HOG):
			classi.feature_ = FeatureExtractor::HOG;
			tmpPrediction  = classi.runTest(colorSp,what,dummy);
			predictions.push_back(tmpPrediction);
			tmpPrediction.clear();
*/
	}

	// HOW TO COMBINE THEM?BIN VOTES EVERY 20DEGREES AND AVERAGE THE WINNING BIN
	std::deque<std::deque<float> > finalPreds;
	for(std::size_t n=0;n<predictions[0].size();++n){// CLASSES
		std::deque<float> preFinalPreds;
		for(std::size_t o=0;o<predictions[0][n].size();++o){// PREDICTIONS
			// FOR EACH PREDICTION BIN THE VOTES AND FIND THE "WINNING" ANGLE
			std::deque<unsigned> votes(18,0);
			std::deque<float> bins(18,0.0);
			unsigned winningLabel   = 0;
			unsigned winningNoVotes = 0;
			float angleMin,angleMax;
			classi.getAngleLimits(n,o,angleMin,angleMax);
			for(std::size_t m=0;m<predictions.size();++m){ // OVER FEATURES
				// CHECK IF THE PREDICTIONS ARE IN THE GOOD RANGE
				if(predictions[m][n][o]>=angleMin || predictions[m][n][o]<angleMax){
					unsigned degrees = static_cast<unsigned>(predictions[m][n][o]*\
						180.0/M_PI);
					unsigned label   = degrees/20;
					++votes[label];
					bins[label] += predictions[m][n][o];
					if(votes[label]>winningNoVotes){
						winningNoVotes = votes[label];
						winningLabel   = label;
					}
				}
			}
			// IF NOT PREDICTION WAS WITHIN THE LIMITS:
			float guess;
			if(winningNoVotes==0){
				guess = classi.features_->dataMotionVectors()[n][o];
			}else{
				guess = bins[winningLabel]/static_cast<float>(votes[winningLabel]);
			}
			// STORE THE FINAL PREDICTIONS
			preFinalPreds.push_back(guess);
			votes.clear();
			bins.clear();
		}
		finalPreds.push_back(preFinalPreds);
	}

	// FINALLY EVALUATE THE FINAL PREDICTIONS
	std::cout<<"FINAL EVALUATION________________________________________________"<<\
		"________________________"<<std::endl;
	float error = 0.0,normError=0.0,meanDiff=0.0;
	classi.evaluate(finalPreds,error,normError,meanDiff);
}
//==============================================================================
/** Run over multiple settings of the parameters to find the best ones.
 */
void parameterSetting(const std::string &errorsOnTrain,const std::string &errorsOnTest,\
ClassifyImages &classi,int argc,char** argv,FeatureExtractor::FEATURE feat,\
int colorSp,bool useGt,AnnotationsHandle::POSE what,\
GaussianProcess::kernelFunction kernel){
  	std::ofstream train,test;
	train.open(errorsOnTrain.c_str(),std::ios::out | std::ios::app);
	test.open(errorsOnTest.c_str(),std::ios::out | std::ios::app);
	for(float v=0.2;v<5.0;v+=0.1){
		for(float l=68.0;l<150.0;l+=1.0){
			classi.init(v,l,feat,kernel,useGt);
			float errorTrain = classi.runCrossValidation(2,what,colorSp,true);
			train<<v<<" "<<l<<" "<<errorTrain<<std::endl;
			//-------------------------------------------
			classi.init(v,l,feat,kernel,useGt);
			float errorTest = classi.runCrossValidation(2,what,colorSp,false);
			test<<v<<" "<<l<<" "<<errorTest<<std::endl;
		}
	}
	train.close();
	test.close();
}
//==============================================================================
/** Applies PCA on top of a data-row to reduce its dimensionality.
 */
cv::Mat ClassifyImages::reduceDimensionality(const cv::Mat &data,bool train,\
int nEigens,int reshapeRows){
	cv::Mat preData;
	data.copyTo(preData);
	preData.convertTo(preData,CV_32FC1);
	if(train){
		if(!nEigens){nEigens = data.rows/4;}
		this->pca_ = std::tr1::shared_ptr<cv::PCA>(new cv::PCA(preData,cv::Mat(),\
			CV_PCA_DATA_AS_ROW,nEigens));
	}
	cv::Mat finalMat = this->pca_->project(preData);
	if(this->plot_ && reshapeRows){
		for(int i=0;i<finalMat.rows;++i){
			cv::Mat test1 = this->pca_->backProject(finalMat.row(i));
			cv::Mat dummy = preData.row(i), test2;
			dummy.copyTo(test2);
			test2 = test2.reshape(0,reshapeRows);
			test1 = test1.reshape(0,reshapeRows);
			cv::imshow("back_proj",test1);
			cv::imshow("original",test2);
			cv::waitKey(0);
			test1.release();
			test2.release();
			dummy.release();
		}
	}
	preData.release();
	finalMat.convertTo(finalMat,CV_32FC1);
	return finalMat;
}
//==============================================================================
int main(int argc,char **argv){
/*
	// test
	float normError = 0.0f;
	ClassifyImages classi(argc,argv,ClassifyImages::TEST);
	classi.init(0.85,85.0,FeatureExtractor::HOG,&GaussianProcess::sqexp,true);
 	classi.runTest(-1,AnnotationsHandle::LONGITUDE,normError);
*/
	//--------------------------------------------------------------------------
/*
	// build data matrix
 	ClassifyImages classi(argc,argv,ClassifyImages::EVALUATE);
	classi.init(0.8,150.0,FeatureExtractor::HOG,&GaussianProcess::sqexp,true);
	classi.buildDataMatrix(-1,FeatureExtractor::TOP);
*/
	//--------------------------------------------------------------------------

	// evaluate
 	ClassifyImages classi(argc,argv,ClassifyImages::EVALUATE);
	classi.init(0.01,100.0,FeatureExtractor::RAW_PIXELS,&GaussianProcess::sqexp,true);
	classi.runCrossValidation(4,AnnotationsHandle::LONGITUDE,-1,false,\
		FeatureExtractor::HEAD);

	//--------------------------------------------------------------------------
/*
	// BUILD THE SIFT DICTIONARY
  	ClassifyImages classi(argc,argv,ClassifyImages::BUILD_DICTIONARY);
	classi.buildDictionary(-1,true);
*/
	//--------------------------------------------------------------------------
/*
	// find parmeteres
	ClassifyImages classi(argc,argv,ClassifyImages::EVALUATE);
	parameterSetting("train.txt","test.txt",classi,argc,argv,FeatureExtractor::HOG,\
		CV_BGR2Lab,true,AnnotationsHandle::LONGITUDE,&GaussianProcess::sqexp);
*/
	//--------------------------------------------------------------------------
/*
	// multiple classifiers
	ClassifyImages classi(argc,argv,ClassifyImages::TEST);
	multipleClassifier(-1,AnnotationsHandle::LONGITUDE,classi,0.85,\
		85,&GaussianProcess::sqexp,false);
*/
}


