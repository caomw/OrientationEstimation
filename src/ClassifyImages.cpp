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
ClassifyImages::ClassifyImages(int argc,char **argv,ClassifyImages::USES use,\
ClassifyImages::CLASSIFIER classi){
	// DEAFAULT INITIALIZATION
	this->clasifier_        = classi;
	this->trainFolder_      = "";
	this->testFolder_       = "";
	this->annotationsTrain_ = "";
	this->annotationsTest_  = "";
	this->noise_            = 0.01;
	this->lengthSin_        = 100.0;
	this->lengthCos_        = 100.0;
	this->kFunction_        = &GaussianProcess::sqexp;
	this->feature_          = std::deque<FeatureExtractor::FEATURE>\
		(FeatureExtractor::EDGES);
	this->foldSize_		    = 5;
	this->modelName_        = "";
	this->tmpModelName_     = "";
	this->what_             = use;
	this->useGroundTruth_   = false;
	this->dimRed_           = false;
	this->dimPCA_           = 100;
	this->plot_             = false;
	this->plot_             = false;
	this->withFlip_         = false;
	this->usePCAModel_      = false;

	// INITIALIZE THE DATA MATRIX AND TARGETS MATRIX AND THE GAUSSIAN PROCESSES
	for(unsigned i=0;i<3;++i){
		this->trainMean_.push_back(cv::Mat());
		this->trainVar_.push_back(cv::Mat());
		this->trainData_.push_back(cv::Mat());
		this->trainTargets_.push_back(cv::Mat());
		this->testData_.push_back(cv::Mat());
		this->testTargets_.push_back(cv::Mat());
		this->classiPca_.push_back(std::deque<std::tr1::shared_ptr<cv::PCA> >());
		switch(this->clasifier_){
			case(ClassifyImages::GAUSSIAN_PROCESS):
				this->gpSin_.push_back(GaussianProcess());
				this->gpCos_.push_back(GaussianProcess());
				break;
			case(ClassifyImages::NEURAL_NETWORK):
				this->nnCos_.push_back(CvANN_MLP());
				this->nnSin_.push_back(CvANN_MLP());
				this->nn_.push_back(CvANN_MLP());
				break;
			case(ClassifyImages::K_NEAREST_NEIGHBORS):
				this->sinKNN_.push_back(CvKNearest());
				this->cosKNN_.push_back(CvKNearest());
				break;
		}
	}
	this->pca_ = std::vector<std::tr1::shared_ptr<cv::PCA> >\
		(3,std::tr1::shared_ptr<cv::PCA>());
	this->doWhat(argc,argv);
}
//==============================================================================
/** Implements all the file/folder actions.
 */
void ClassifyImages::doWhat(int argc,char **argv){
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
				this->testFolder_       = this->testDir_+"annotated_train/";
				this->annotationsTest_  = this->testDir_+"annotated_train.txt";
				files2check.push_back(this->trainFolder_);
				files2check.push_back(this->annotationsTrain_);
				files2check.push_back(this->testFolder_);
				files2check.push_back(this->annotationsTest_);
				this->tmpModelName_ = "data/TEST/";
				break;
			case(ClassifyImages::EVALUATE):
				// IF WE WANT TO EVALUATE WITH CORSSVALIDATION
				this->trainFolder_      = this->trainDir_+"annotated_train/";
				this->annotationsTrain_ = this->trainDir_+"annotated_train.txt";
				files2check.push_back(this->trainFolder_);
				files2check.push_back(this->annotationsTrain_);
				this->tmpModelName_ = "data/EVALUATE/";
				break;
			case(ClassifyImages::BUILD_DATA):
				// IF WE WANT TO EVALUATE WITH CORSSVALIDATION
				this->trainFolder_      = this->trainDir_+"annotated_train/";
				this->annotationsTrain_ = this->trainDir_+"annotated_train.txt";
				files2check.push_back(this->trainFolder_);
				files2check.push_back(this->annotationsTrain_);
				this->tmpModelName_ = "data/BUILD_DATA/";
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
		Helpers::file_exists(this->tmpModelName_.c_str(),true);
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
		if(this->pca_[i]){
			this->pca_[i].reset();
		}
		if(!this->trainMean_[i].empty()){
			this->trainMean_[i].release();
		}
		if(!this->trainVar_[i].empty()){
			this->trainVar_[i].release();
		}
	}
	this->trainData_.clear();
	this->trainTargets_.clear();
	this->testData_.clear();
	this->testTargets_.clear();

	switch(this->clasifier_){
		case(ClassifyImages::GAUSSIAN_PROCESS):
			this->gpSin_.clear();
			this->gpCos_.clear();
			break;
		case(ClassifyImages::NEURAL_NETWORK):
			this->nnCos_.clear();
			this->nnSin_.clear();
			this->nn_.clear();
			break;
		case(ClassifyImages::K_NEAREST_NEIGHBORS):
			this->sinKNN_.clear();
			this->cosKNN_.clear();
			break;
	}
	this->classiPca_.clear();
}
//==============================================================================
ClassifyImages::USES ClassifyImages::what(){
	return this->what_;
}
//==============================================================================
/** Initialize the options for the Gaussian Process regression.
 */
void ClassifyImages::init(float theNoise,float theLengthSin,float theLengthCos,\
const std::deque<FeatureExtractor::FEATURE> &theFeature,\
GaussianProcess::kernelFunction theKFunction,bool toUseGT){
	this->noise_          = theNoise;
	this->lengthSin_      = theLengthSin;
	this->lengthCos_      = theLengthCos;
	this->kFunction_      = theKFunction;
	this->feature_        = theFeature;
	this->useGroundTruth_ = toUseGT;
	for(FeatureExtractor::FEATURE f=FeatureExtractor::EDGES;\
	f<=FeatureExtractor::SKIN_BINS;++f){
		if(!FeatureExtractor::isFeatureIn(this->feature_,f)){continue;}
		switch(f){
			case(FeatureExtractor::IPOINTS):
				this->modelName_ = this->tmpModelName_+"IPOINTS_";
				break;
			case(FeatureExtractor::EDGES):
				this->modelName_ = this->tmpModelName_+"EDGES_";
				break;
			case(FeatureExtractor::SURF):
				this->modelName_ = this->tmpModelName_+"SURF_";
				break;
			case(FeatureExtractor::GABOR):
				this->modelName_ = this->tmpModelName_+"GABOR_";
				break;
			case(FeatureExtractor::SIFT):
				this->modelName_ = this->tmpModelName_+"SIFT_";
				break;
			case(FeatureExtractor::TEMPL_MATCHES):
				this->modelName_ = this->tmpModelName_+"TEMPL_MATCHES_";
				break;
			case(FeatureExtractor::RAW_PIXELS):
				this->modelName_ = this->tmpModelName_+"RAW_PIXELS_";
				break;
			case(FeatureExtractor::SKIN_BINS):
				this->modelName_ = this->tmpModelName_+"SKIN_BINS_";
				break;
			case(FeatureExtractor::HOG):
				this->modelName_ = this->tmpModelName_+"HOG_";
				break;
		}
	}
	this->modelName_ += "/";
	Helpers::file_exists(this->modelName_.c_str(),true);
}
//==============================================================================
/** Concatenate the loaded data from the files to the currently computed data.
 */
void ClassifyImages::loadData(const cv::Mat &tmpData1,const cv::Mat &tmpTargets1,\
unsigned i,cv::Mat &outData,cv::Mat &outTargets){
	// LOAD THE DATA AND TARGET MATRIX FROM THE FILE IF IT'S THERE
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");

	cv::Mat tmpData2 = cv::Mat(),tmpTargets2 = cv::Mat();
	std::string modelDataName = this->modelName_+names[i]+"/Data.bin";
	std::string modelTargetsName = this->modelName_+names[i]+"/Labels.bin";

	// lOAD THE DESIRED DATA
	if(Helpers::file_exists(modelDataName.c_str())){
		Auxiliary::binFile2mat(tmpData2,const_cast<char*>(modelDataName.c_str()));
		std::cout<<" Loading "<<modelDataName<<std::endl;
	}
	if(Helpers::file_exists(modelTargetsName.c_str())){
		Auxiliary::binFile2mat(tmpTargets2,const_cast<char*>(modelTargetsName.c_str()));
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
/** Read and load the training/testing data.
 */
void ClassifyImages::getData(std::string &trainFld,std::string &annoFld,\
bool fromFolder,bool test,bool justLoad){
	if(justLoad){ return;}

	// TRAIN ALL 3 CLASSES WITH DATA THAT WE HAVE,PREDICT ONLY ON THE GOOD CLASS
	this->features_->init(trainFld,annoFld,this->feature_,test,fromFolder);
	this->features_->start(fromFolder,this->useGroundTruth_);
}
//==============================================================================
/** Creates the training data (according to the options),the labels and
 * trains the a \c GaussianProcess on the data.
 */
void ClassifyImages::trainGP(AnnotationsHandle::POSE what,int i){
	this->gpSin_[i].init(this->kFunction_);
	this->gpCos_[i].init(this->kFunction_);

	// TRAIN THE SIN AND COS SEPARETELY FOR LONGITUDE || LATITUDE
	if(what == AnnotationsHandle::LONGITUDE){
		cv::Mat sinTargets = this->trainTargets_[i].col(0);
		cv::Mat cosTargets = this->trainTargets_[i].col(1);
		this->gpSin_[i].train(this->trainData_[i],sinTargets,\
			this->kFunction_,static_cast<_float>(this->noise_),\
			static_cast<_float>(this->lengthSin_));
		this->gpCos_[i].train(this->trainData_[i],cosTargets,\
			this->kFunction_,static_cast<_float>(this->noise_),\
			static_cast<_float>(this->lengthCos_));
		sinTargets.release();
		cosTargets.release();

	// TRAIN THE SIN AND COS SEPARETELY FOR LATITUDE
	}else if(what == AnnotationsHandle::LATITUDE){
		cv::Mat sinTargets = this->trainTargets_[i].col(2);
		cv::Mat cosTargets = this->trainTargets_[i].col(3);
		this->gpSin_[i].train(this->trainData_[i],sinTargets,\
			this->kFunction_,static_cast<_float>(this->noise_),\
			static_cast<_float>(this->lengthSin_));
		this->gpCos_[i].train(this->trainData_[i],cosTargets,\
			this->kFunction_,static_cast<_float>(this->noise_),\
			static_cast<_float>(this->lengthCos_));
		sinTargets.release();
		cosTargets.release();
	}
}
//==============================================================================
/** Creates the training data (according to the options),the labels and
 * trains the a \c Neural Network on the data.
 */
void ClassifyImages::trainNN(int i,bool together){
	// DEFINE THE NETWORK ARCHITECTURE
	if(together){
		std::cout<<"Training the Neural Network..."<<std::endl;
		int layerInts[] = {this->trainData_[i].cols,100,100,4};
		cv::Mat layerSizes(1,static_cast<int>(sizeof(layerInts)/sizeof(layerInts[0])),\
			CV_32S,layerInts);
		this->nn_[i].create(layerSizes,1);

		// TRAIN THE SIN AND COS FOR both LONGITUDE & LATITUDE
		this->nn_[i].train(this->trainData_[i],this->trainTargets_[i].colRange(0,4),\
			cv::Mat(),cv::Mat(),CvANN_MLP_TrainParams(cvTermCriteria\
			(CV_TERMCRIT_ITER,1000,0.01),CvANN_MLP_TrainParams::BACKPROP,0.01));
		layerSizes.release();
	}else{
		int layerInts[] = {this->trainData_[i].cols,100,100,1};
		std::cout<<"Training the Neural Network Sin..."<<std::endl;
		cv::Mat layerSizesSin(1,static_cast<int>(sizeof(layerInts)/sizeof(layerInts[0])),\
			CV_32S,layerInts);
		this->nnSin_[i].create(layerSizesSin,1);
		this->nnSin_[i].train(this->trainData_[i],this->trainTargets_[i].col(0),\
			cv::Mat(),cv::Mat(),CvANN_MLP_TrainParams(cvTermCriteria\
			(CV_TERMCRIT_ITER,1000,0.01),CvANN_MLP_TrainParams::BACKPROP,0.01));
		layerSizesSin.release();

		// AND FOR THE COSINE
		std::cout<<"Training the Neural Network Cos..."<<std::endl;
		cv::Mat layerSizesCos(1,static_cast<int>(sizeof(layerInts)/sizeof(layerInts[0])),\
			CV_32S,layerInts);
		this->nnCos_[i].create(layerSizesCos,1);
		this->nnCos_[i].train(this->trainData_[i],this->trainTargets_[i].col(1),\
			cv::Mat(),cv::Mat(),CvANN_MLP_TrainParams(cvTermCriteria\
			(CV_TERMCRIT_ITER,1000,0.01),CvANN_MLP_TrainParams::BACKPROP,0.01));
		layerSizesCos.release();
	}
}
//==============================================================================
/** Creates the training data (according to the options),the labels and
 * trains the a kNN on the data.
 */
void ClassifyImages::trainKNN(AnnotationsHandle::POSE what,int i){
	std::cout<<"Training the kNN..."<<std::endl;
	// TRAIN THE SIN AND COS SEPARETELY FOR LONGITUDE || LATITUDE
	if(what == AnnotationsHandle::LONGITUDE){
		this->sinKNN_[i].train(this->trainData_[i],this->trainTargets_[i].col(0),\
			cv::Mat(),true,3,false);
		this->cosKNN_[i].train(this->trainData_[i],this->trainTargets_[i].col(1),\
			cv::Mat(),true,3,false);

	// TRAIN THE SIN AND COS SEPARETELY FOR LATITUDE
	}else if(what == AnnotationsHandle::LATITUDE){
		this->sinKNN_[i].train(this->trainData_[i],this->trainTargets_[i].col(2),\
			cv::Mat(),true,3,false);
		this->cosKNN_[i].train(this->trainData_[i],this->trainTargets_[i].col(3),\
			cv::Mat(),true,3,false);
	}
}
//==============================================================================
/** Backproject each image on the 4 models, compute distances and return.
 */
cv::Mat ClassifyImages::getPCAModel(const cv::Mat &data,int i,unsigned bins){
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	if(this->classiPca_[i].empty()){
		for(std::size_t l=0;l<bins;++l){
			std::string modelPCA = this->modelName_+names[i]+"/PCA"+\
				Auxiliary::int2string(l)+".xml";
			this->classiPca_[i].push_back(Auxiliary::loadPCA(modelPCA));
		}
	}

	cv::Mat result;
	for(int j=0;j<data.rows;++j){
		cv::Mat preResult = cv::Mat::zeros(cv::Size(data.cols*classiPca_[i].size(),1),\
			CV_32FC1);
		for(std::size_t l=0;l<this->classiPca_[i].size();++l){
			cv::Mat backprojectTest,projectTest;
			projectTest     = this->classiPca_[i][l]->project(data.row(j));
			backprojectTest = this->classiPca_[i][l]->backProject(projectTest);
			cv::Mat part = data.row(j)-backprojectTest;
			cv::Mat dumm = preResult.colRange(l*data.cols,(l+1)*data.cols);
			part.copyTo(dumm);
			dumm.release();
			projectTest.release();
			backprojectTest.release();
		}
		if(result.empty()){
			preResult.copyTo(result);
		}else{
			result.push_back(preResult);
		}
		preResult.release();
	}
	return result;
}
//==============================================================================
/** Build a class model for each one of the 4 classes.
 */
void ClassifyImages::buildPCAModels(int colorSp,FeatureExtractor::FEATUREPART part){
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
		this->feature_,false,true);
	this->features_->start(true,this->useGroundTruth_);
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		Helpers::file_exists((this->modelName_+names[i]).c_str(),true);
		(this->features_->data()[i]).copyTo(this->trainData_[i]);
		(this->features_->targets()[i]).copyTo(this->trainTargets_[i]);

		if(this->trainData_[i].empty()){continue;}
		//EXTRACT THE EIGENSPACES
		this->trainDist2PCA(AnnotationsHandle::LONGITUDE,i,4,3);
		for(std::size_t p=0;p<this->classiPca_[i].size();++p){
			std::string modelPCA = this->modelName_+names[i]+"/PCA"+\
				Auxiliary::int2string(p)+".xml";
			Auxiliary::savePCA(this->classiPca_[i][p],modelPCA);
			std::cout<<"PCA models stored to: "<<modelPCA<<std::endl;
		}
	}
}
//==============================================================================
/** Creates the training data (according to the options),the labels and
 * builds the eigen-orientations.
 */
void ClassifyImages::trainDist2PCA(AnnotationsHandle::POSE what,int i,\
unsigned bins,unsigned dimensions){
	std::cout<<"Training the egein-orientations..."<<std::endl;
	unsigned binSize;
	if(what == AnnotationsHandle::LONGITUDE){
		if(!bins){
			bins    = 36;
			binSize = 10;
		}else{
			binSize = 360/bins;
		}
	}else if(what == AnnotationsHandle::LATITUDE){
		if(!bins){
			binSize = 18;
			binSize = 10;
		}else{
			binSize = 180/bins;
		}
	}
	std::vector<cv::Mat> trainingData(bins,cv::Mat());
	for(int l=0;l<this->trainData_[i].rows;++l){
		float y,x;
		if(what == AnnotationsHandle::LONGITUDE){
			y = this->trainTargets_[i].at<float>(l,0);
			x = this->trainTargets_[i].at<float>(l,1);
		}else{
			y = this->trainTargets_[i].at<float>(l,2);
			x = this->trainTargets_[i].at<float>(l,3);
		}
		float prediction = std::atan2(y,x);
		Auxiliary::angle0to360(prediction);
		unsigned index = (prediction*180.0/M_PI)/binSize;
		cv::Mat dummy1 = this->trainData_[i].row(l);
		if(trainingData[index].empty()){
			dummy1.copyTo(trainingData[index]);
		}else{
			trainingData[index].push_back(dummy1);
		}
		dummy1.release();
	}

	// COMPUTE THE EIGEN SUBSPACE FOR EACH ORIENTATION-MATRIX
	this->classiPca_[i].clear();
	this->classiPca_[i] = std::deque<std::tr1::shared_ptr<cv::PCA> >\
		(trainingData.size(),std::tr1::shared_ptr<cv::PCA>\
		(static_cast<cv::PCA*>(0)));
	for(int l=0;l<trainingData.size();++l){
		if(trainingData[l].rows>1){
			this->classiPca_[i][l] = std::tr1::shared_ptr<cv::PCA>(new cv::PCA\
				(trainingData[l],cv::Mat(),CV_PCA_DATA_AS_ROW,dimensions));
		}
	}
	// RELEASE THE TRAINING DATA
	trainingData.clear();
}
//==============================================================================
/** Trains on the training data using the indicated classifier.
 */
void ClassifyImages::train(AnnotationsHandle::POSE what,bool fromFolder,bool justLoad){
	this->getData(this->trainFolder_,this->annotationsTrain_,fromFolder,false,justLoad);
	std::deque<std::string> names;
	names.push_back("CLOSE");names.push_back("MEDIUM");names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		if(!this->trainData_[i].empty()){
			this->trainData_[i].release();
		}
		if(!this->trainTargets_[i].empty()){
			this->trainTargets_[i].release();
		}
		// LOAD THE DATA AND THEN DO PCA IF WNATED
		cv::Mat tmpData,tmpTargets;
		this->features_->data()[i].copyTo(tmpData);
		this->features_->targets()[i].copyTo(tmpTargets);
		tmpData.convertTo(tmpData,CV_32FC1);
		tmpTargets.convertTo(tmpTargets,CV_32FC1);
		cv::Mat outData,outTargets;
		this->loadData(tmpData,tmpTargets,i,outData,outTargets);

		// IF DISTANCES TO DATA MODELS SHOULD BE COMPUTED
		if(this->usePCAModel_){
			cv::Mat tmpOut = this->getPCAModel(outData,i,4);
			outData.release();
			tmpOut.copyTo(outData);
			tmpOut.release();
		}

		// IF WE CANNOT LOAD DATA,THEN WE BUILD IT
		if(this->dimRed_ && !outData.empty()){
			this->trainData_[i] = this->reduceDimensionality(outData,i,true,\
				this->dimPCA_);
		}else{
			outData.copyTo(this->trainData_[i]);
		}
		outTargets.copyTo(this->trainTargets_[i]);
		outData.release();
		outTargets.release();
		tmpData.release();
		tmpTargets.release();
		if(this->trainData_[i].empty()) continue;

		// CHECK TO SEE IF THERE IS ANY DATA IN THE CURRENT CLASS
		assert(this->trainData_[i].rows==this->trainTargets_[i].rows);

		// GET THE MEAN AND THE VARIANCE OF THE TRAINING DATA
		Auxiliary::mean0Variance1(this->trainData_[i],this->trainMean_[i],\
			this->trainVar_[i]);

		// CLASSIFY ON IMAGES USING THE TRAINING
		switch(this->clasifier_){
			case(ClassifyImages::GAUSSIAN_PROCESS):
				this->trainGP(what,i);
				break;
			case(ClassifyImages::NEURAL_NETWORK):
				this->trainNN(i);
				break;
			case(ClassifyImages::K_NEAREST_NEIGHBORS):
				this->trainKNN(what,i);
				break;
			case(ClassifyImages::DIST2PCA):
				this->trainDist2PCA(what,i);
				break;
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
		this->feature_,false,true);
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
cv::Point2f ClassifyImages::predictGP(cv::Mat &testRow,int i){
	std::cout<<"Predicting on the GP..."<<std::endl;
	// FOR EACH ROW IN THE TEST MATRIX PREDICT
	GaussianProcess::prediction prediSin;
	GaussianProcess::prediction prediCos;
	this->gpSin_[i].predict(testRow,prediSin,static_cast<_float>(this->lengthSin_));
	this->gpCos_[i].predict(testRow,prediCos,static_cast<_float>(this->lengthCos_));
	cv::Point2f onePrediction = cv::Point2f(prediCos.mean_[0],prediSin.mean_[0]);
	prediSin.mean_.clear();
	prediSin.variance_.clear();
	prediCos.mean_.clear();
	prediCos.variance_.clear();
	return onePrediction;
}
//==============================================================================
/** Creates the test data and applies \c Neural Network prediction on the test
 * data.
 */
cv::Point2f ClassifyImages::predictNN(cv::Mat &testRow,\
AnnotationsHandle::POSE what,int i,bool together){
	std::cout<<"Predicting on the Neural Network..."<<std::endl;
	// FOR EACH ROW IN THE TEST MATRIX PREDICT
	float x,y;
	if(together){
		cv::Mat preds;
		this->nn_[i].predict(testRow,preds);
		preds.convertTo(preds,CV_32FC1);
		std::cout<<"#Predictions="<<preds.size()<<" #Data="<<\
			this->testData_[i].size()<<std::endl;
		if(what == AnnotationsHandle::LONGITUDE){
			y = preds.at<float>(0,0);
			x = preds.at<float>(0,1);
		}else if(what == AnnotationsHandle::LATITUDE){
			y = preds.at<float>(0,2);
			x = preds.at<float>(0,3);
		}
		preds.release();
	}else{
		cv::Mat predsSin,predsCos;
		this->nnSin_[i].predict(testRow,predsSin);
		predsSin.convertTo(predsSin,CV_32FC1);
		y = predsSin.at<float>(0,0);
		this->nnCos_[i].predict(testRow,predsCos);
		predsCos.convertTo(predsCos,CV_32FC1);
		x = predsCos.at<float>(0,0);
		predsSin.release();
		predsCos.release();
	}
	return cv::Point2f(x,y);
}
//==============================================================================
/** Creates the test data and applies \c kNN prediction on the test data.
 */
cv::Point2f ClassifyImages::predictKNN(cv::Mat &testRow,int i){
	std::cout<<"Predicting on the kNN..."<<std::endl;
 	cv::Mat sinPreds(cv::Size(1,1),CV_32FC1);
 	cv::Mat cosPreds(cv::Size(1,1),CV_32FC1);
 	cv::Mat neighbors,dists;
 	this->sinKNN_[i].find_nearest(testRow,3,sinPreds,neighbors,dists);
 	this->cosKNN_[i].find_nearest(testRow,3,cosPreds,neighbors,dists);
 	sinPreds.convertTo(sinPreds,CV_32FC1);
 	cosPreds.convertTo(cosPreds,CV_32FC1);

	// FOR EACH ROW IN THE TEST MATRIX COMPUTE THE PREDICTION
	float y = sinPreds.at<float>(0,0);
	float x = cosPreds.at<float>(0,0);
	sinPreds.release();
	cosPreds.release();
	neighbors.release();
	dists.release();
	return cv::Point2f(x,y);
}
//==============================================================================
/** Creates the test data and applies computes the distances to the stored
 * eigen-orientations.
 */
cv::Point2f ClassifyImages::predictDist2PCA(cv::Mat &testRow,\
AnnotationsHandle::POSE what,int i){
	std::cout<<"Predicting on the eigen-orientations..."<<std::endl;
	unsigned bins;
	if(what == AnnotationsHandle::LONGITUDE){bins = 36;
	}else{bins = 18;}
	cv::Mat testingData;
	// PROJECT EACH IMAGE ON EACH SPACE AND THEN BACKPROJECT IT
	for(int l=0;l<this->classiPca_[i].size();++l){
		cv::Mat backprojectTest,projectTest;
		if(this->classiPca_[i][l].get()){
			projectTest = this->classiPca_[i][l]->project(testRow);
			backprojectTest = this->classiPca_[i][l]->backProject(projectTest);
		}else{
			backprojectTest = cv::Mat::zeros(testRow.size(),CV_32FC1);
		}
		if(testingData.empty()){
			backprojectTest.copyTo(testingData);
		}else{
			testingData.push_back(backprojectTest);
		}
		projectTest.release();
		backprojectTest.release();
	}

	// FIND THE PROJECTION CLOSEST TO ONE OF THE ORIENTATIONS
	cv::Mat minDists,minLabs;
	FeatureExtractor::dist2(testingData,testRow,minDists,minLabs);
	cv::Point minLoc(0,0);
	cv::minMaxLoc(minDists,NULL,NULL,&minLoc,NULL,cv::Mat());
	float prediction = minLoc.x*10.0*(M_PI/180.0);
	cv::Point2f onePrediction = cv::Point2f(std::cos(prediction),std::sin(prediction));
	minDists.release();
	minLabs.release();
	testingData.release();
	return onePrediction;
}
//==============================================================================
/** Check if the classifier was initialized.
 */
bool ClassifyImages::isClassiInit(int i){
	switch(this->clasifier_){
		case(ClassifyImages::GAUSSIAN_PROCESS):
			return (!this->gpSin_[i].empty() || !this->gpCos_[i].empty());
			break;
		case(ClassifyImages::NEURAL_NETWORK):
			return (this->nn_[i].get_layer_count()!=0 ||\
				(this->nnSin_[i].get_layer_count()!=0 &&\
				this->nnSin_[i].get_layer_count()!=0));
			break;
		case(ClassifyImages::K_NEAREST_NEIGHBORS):
			return (this->sinKNN_[i].get_max_k()!=0 && this->cosKNN_[i].get_max_k()!=0);
			break;
		case(ClassifyImages::DIST2PCA):
			return (!this->classiPca_[i].empty());
			break;
	}
}
//==============================================================================
/** Predicts on the test data.
 */
std::deque<std::deque<cv::Point2f> > ClassifyImages::predict\
(AnnotationsHandle::POSE what,bool fromFolder){
	for(unsigned c=0;c<3;++c){
		this->testData_[c].release();
		this->testTargets_[c].release();
	}

	std::deque<std::deque<cv::Point2f> > allPreds(3,std::deque<cv::Point2f>());
	PeopleDetector::dataIsProduced_ = true;
	boost::thread* predictor;
	predictor = new boost::thread(&ClassifyImages::getData,this,\
		this->testFolder_,this->annotationsTest_,fromFolder,true,false);

	// AS LONG AS THERE IS A ROW IN THE DATA MATRIX
	std::tr1::shared_ptr<PeopleDetector::DataRow> dataRow;
	while(PeopleDetector::dataIsProduced_ || !this->features_->dataInfoEmpty()){
		while(this->features_->dataInfoEmpty() && PeopleDetector::dataIsProduced_){
			sleep(.5);
		};
		if(this->features_->dataInfoEmpty()){break;}
		dataRow = this->features_->popDataRow();
		if(!dataRow.get()) continue;
		std::deque<cv::Point2f> predictions = this->doPredict(dataRow,what,\
			fromFolder);
		std::cout<<this->features_->dataInfoEmpty()<<") >>> Consume"<<\
			dataRow->imgName_<<"..."<<std::endl;
		for(std::size_t p=0;p<predictions.size();++p){
			if(predictions[p] != cv::Point2f(1e+10,1e+10)){
				allPreds[p].push_back(predictions[p]);
				// DRAW THE PREDICTED ORIENTATIONS FOR
				this->features_->drawPredictions(predictions[p],dataRow);
			}
		}
		dataRow.reset(static_cast<PeopleDetector::DataRow*>(NULL));
	}
	// JOIN THE THREADS
	predictor->join();
	delete predictor;
	return allPreds;
}
//==============================================================================
/** Predicts on the test data.
 */
std::deque<cv::Point2f> ClassifyImages::doPredict(std::tr1::shared_ptr\
<PeopleDetector::DataRow> dataRow,AnnotationsHandle::POSE what,bool fromFolder){
	// FOR TESTING WE ALWAYS BUILT THE DATA (THERE IS NOT SAVED MODEL)
	std::deque<std::string> names;
	std::deque<cv::Point2f> predictions(3,cv::Point2f(1e+10,1e+10));
	// CHECK TO SEE IF THERE IS ANY DATA IN THE CURRENT CLASS
	if(dataRow->testRow_.empty() || !this->isClassiInit(dataRow->groupNo_)){
		return predictions;
	}
	// IF DISTANCES TO DATA MODELS SHOULD BE COMPUTED
	cv::Mat outData,data,targs;
	dataRow->testRow_.copyTo(outData);
	if(this->usePCAModel_){
		cv::Mat tmpOut = this->getPCAModel(outData,dataRow->groupNo_,4);
		outData.release();
		tmpOut.copyTo(outData);
		tmpOut.release();
	}
	if(this->dimRed_){
		data = this->reduceDimensionality(outData,dataRow->groupNo_,false,this->dimPCA_);
	}else{
		outData.copyTo(data);
	}
	outData.release();
	// GET ONLY THE ANGLES YOU NEED
	if(what == AnnotationsHandle::LONGITUDE){
		cv::Mat dum = dataRow->testTarg_.colRange(0,2);
		dum.copyTo(targs);
		dum.release();
	}else if(what == AnnotationsHandle::LATITUDE){
		cv::Mat dum = dataRow->testTarg_.colRange(2,4);
		dum.copyTo(targs);
		dum.release();
	}
	Auxiliary::mean0Variance1(data,this->trainMean_[dataRow->groupNo_],\
		this->trainVar_[dataRow->groupNo_]);
	// COPY THE DATA LABELS AND THE MATRIX TO THE CLASS VARIABLES
	if(this->testData_[dataRow->groupNo_].empty()){
		data.copyTo(this->testData_[dataRow->groupNo_]);
	}else{
		this->testData_[dataRow->groupNo_].push_back(data);
	}
	if(this->testTargets_[dataRow->groupNo_].empty()){
		targs.copyTo(this->testTargets_[dataRow->groupNo_]);
	}else{
		this->testTargets_[dataRow->groupNo_].push_back(targs);
	}
	// PREDICT ON THE TEST IMAGES
	cv::Point2f onePrediction;
	switch(this->clasifier_){
		case(ClassifyImages::GAUSSIAN_PROCESS):
			onePrediction = this->predictGP(data,dataRow->groupNo_);
			break;
		case(ClassifyImages::NEURAL_NETWORK):
			onePrediction = this->predictNN(data,what,dataRow->groupNo_);
			break;
		case(ClassifyImages::K_NEAREST_NEIGHBORS):
			onePrediction = this->predictKNN(data,dataRow->groupNo_);
			break;
		case(ClassifyImages::DIST2PCA):
			onePrediction = this->predictDist2PCA(data,what,dataRow->groupNo_);
			break;
	}
	predictions[dataRow->groupNo_] = onePrediction;
	return predictions;
}
//==============================================================================
/** Evaluate one prediction versus its target.
 */
void ClassifyImages::evaluate(const std::deque<std::deque<cv::Point2f> > &prediAngles,\
float &error,float &normError,float &meanDiff,cv::Mat &bins){
	error = 0.0;normError = 0.0;meanDiff = 0.0;
	float errorSin = 0.0,normErrorSin = 0.0,meanDiffSin = 0.0;
	float errorCos = 0.0,normErrorCos = 0.0,meanDiffCos = 0.0;
	unsigned noPeople = 0;
	std::deque<std::string> names;
	cv::Mat binsSin = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
	cv::Mat binsCos = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
	names.push_back("CLOSE");names.push_back("MEDIUM");	names.push_back("FAR");
	for(PeopleDetector::CLASSES i=PeopleDetector::CLOSE;i<=PeopleDetector::FAR;++i){
		std::cout<<"Class "<<names[i]<<">>> targets:"<<this->testTargets_[i].size()<<\
			" data:"<<this->testData_[i].size()<<" predictions:"<<\
			prediAngles[i].size()<<std::endl;
		assert(this->testData_[i].rows == this->testTargets_[i].rows &&\
			this->testTargets_[i].rows == prediAngles[i].size());
		for(int y=0;y<this->testTargets_[i].rows;++y){
			float targetAngle = std::atan2(this->testTargets_[i].at<float>(y,0),\
				this->testTargets_[i].at<float>(y,1));
			float angle       = std::atan2(prediAngles[i][y].y,prediAngles[i][y].x);
			float targetAngleSin = std::asin(this->testTargets_[i].at<float>(y,0));
			float targetAngleCos = std::acos(this->testTargets_[i].at<float>(y,1));
			std::cout<<"sin="<<prediAngles[i][y].y<<" cos="<<\
				prediAngles[i][y].y<<std::endl<<std::endl;
			float angleSin = std::asin(prediAngles[i][y].y);
			float angleCos = std::acos(prediAngles[i][y].x);
			Auxiliary::angle0to360(angle);
			Auxiliary::angle0to360(angleSin);
			Auxiliary::angle0to360(angleCos);
			Auxiliary::angle0to360(targetAngle);
			Auxiliary::angle0to360(targetAngleSin);
			Auxiliary::angle0to360(targetAngleCos);

			std::cout<<"target>"<<targetAngle<<"("<<(targetAngle*180.0/M_PI)<<\
				")--angle>"<<angle<<"("<<(angle*180.0/M_PI)<<\
				")\t target_sin>"<<targetAngleSin<<"("<<(targetAngleSin*180.0/M_PI)<<\
				")--angle_sin>"<<angleSin<<"("<<(angleSin*180.0/M_PI)<<\
				")\t target_cos>"<<targetAngleCos<<"("<<(targetAngleCos*180.0/M_PI)<<\
				")--angle_cos>"<<angleCos<<"("<<(angleCos*180.0/M_PI)<<")"<<std::endl;
			float absDiff = std::abs(targetAngle-angle);
			if(absDiff > M_PI){
				absDiff = 2.0*M_PI - absDiff;
			}
			float absDiffSin = std::abs(targetAngleSin-angleSin);
			if(absDiffSin > M_PI){
				absDiffSin = 2.0*M_PI - absDiffSin;
			}
			float absDiffCos = std::abs(targetAngleCos-angleCos);
			if(absDiffCos > M_PI){
				absDiffCos = 2.0*M_PI - absDiffCos;
			}

			int ind = ((absDiff*180.0/M_PI)/10);
			++bins.at<float>(0,ind);
			int indSin = ((absDiffSin*180.0/M_PI)/10);
			++binsSin.at<float>(0,indSin);
			int indCos = ((absDiffCos*180.0/M_PI)/10);
			++binsCos.at<float>(0,indCos);

			std::cout<<"Difference:"<<absDiff<<"\t DifferenceSin:"<<absDiffSin<<\
				"\t DifferenceCos:"<<absDiffCos<<std::endl;
			error     += absDiff*absDiff;
			normError += (absDiff*absDiff)/(M_PI*M_PI);
			meanDiff  += absDiff;

			errorSin     += absDiffSin*absDiffSin;
			normErrorSin += (absDiffSin*absDiffSin)/(M_PI*M_PI);
			meanDiffSin  += absDiffSin;

			errorCos     += absDiffCos*absDiffCos;
			normErrorCos += (absDiffCos*absDiffCos)/(M_PI*M_PI);
			meanDiffCos  += absDiffCos;
		}
		noPeople += this->testTargets_[i].rows;
	}
	std::cout<<"Number of people: "<<noPeople<<std::endl;
	error     = std::sqrt(error/(noPeople));
	normError = std::sqrt(normError/(noPeople));
	meanDiff  = meanDiff/(noPeople);

	errorSin     = std::sqrt(errorSin/(noPeople));
	normErrorSin = std::sqrt(normErrorSin/(noPeople));
	meanDiffSin  = meanDiffSin/(noPeople);

	errorCos     = std::sqrt(errorCos/(noPeople));
	normErrorCos = std::sqrt(normErrorCos/(noPeople));
	meanDiffCos  = meanDiffCos/(noPeople);

	std::cout<<"RMSN-error:"<<normError<<"\t RMSN-error(sin):"<<normErrorSin<<\
		"\t RMSN-error(cos):"<<normErrorCos<<std::endl;
	std::cout<<"RMSN-accuracy:"<<(1-normError)<<"\t RMSN-accuracy(sin):"<<(1-normErrorSin)<<\
		"\t RMSN-accuracy(cos):"<<(1-normErrorCos)<<std::endl;
	std::cout<<"RMS-error:"<<error<<"\t RMS-error(sin):"<<errorSin<<"\t RMS-error(cos):"<<\
		errorCos<<std::endl;
	std::cout<<"Avg-Diff-Radians:"<<meanDiff<<"\t Avg-Diff-Radians(sin):"<<meanDiffSin<<\
		"\t Avg-Diff-Radians(cos):"<<meanDiffCos<<std::endl;
	std::cout<<"bins >>> "<<bins<<""<<std::endl;
	std::cout<<"binsSin >>> "<<binsSin<<""<<std::endl;
	std::cout<<"binsCos >>> "<<binsCos<<""<<std::endl;
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
}
//==============================================================================
/** Try to optimize the prediction of the angle considering the variance of
 * sin^2 and cos^2.
 */
float ClassifyImages::optimizeSin2Cos2Prediction(const GaussianProcess::prediction \
&predictionsSin,const GaussianProcess::prediction &predictionsCos){
	float betaS = 1.0/(predictionsSin.variance_[0]);
	float betaC = 1.0/(predictionsCos.variance_[0]);
	float y     = predictionsSin.mean_[0];
	float x     = predictionsCos.mean_[0];
	float closeTo;
	closeTo = std::atan2(predictionsSin.mean_[0],predictionsCos.mean_[0]);
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
}
//==============================================================================
/** Build dictionary for vector quantization.
 */
void ClassifyImages::buildDictionary(int colorSp,bool toUseGT){
	// SET THE CALIBRATION AND OTHER FEATURE SETTINGS
	this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp);

	// EXTRACT THE SIFT FEATURES AND CONCATENATE THEM
	std::deque<FeatureExtractor::FEATURE> feat(1,FeatureExtractor::SIFT_DICT);
	this->features_->init(this->trainFolder_,this->annotationsTrain_,feat,false,true);
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

		// NORMALIZE THE DICTIONARY WORDS TO HAVE "LENGTH" 1
		cv::Mat matrix(*centers);
		for(int j=0;j<matrix.rows;++j){
			cv::Mat rowsI = matrix.row(j);
			rowsI         = rowsI/cv::norm(rowsI);
			rowsI.release();
		}

		// WRITE TO FILE THE MEANS
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
	cv::Mat finalBins = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
	// SET THE CALIBRATION ONLY ONCE (ALL IMAGES ARE READ FROM THE SAME DIR)
	this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp,part);
	for(unsigned i=0;i<k;++i){
		std::cout<<"Round "<<i<<"___________________________________________"<<\
			"_____________________________________________________"<<std::endl;
		// SPLIT TRAINING AND TESTING ACCORDING TO THE CURRENT FOLD
		std::deque<std::deque<cv::Point2f> > predicted;
		this->crossValidation(k,i,onTrain);
		//______________________________________________________________________
		if(what == AnnotationsHandle::LONGITUDE){
			//LONGITUDE TRAINING AND PREDICTING
			std::cout<<"Longitude >>> "<<i<<"___________________________________"<<\
				"_____________________________________________________"<<std::endl;
			this->train(AnnotationsHandle::LONGITUDE,false);
			this->features_->setFlip(false);
			predicted = this->predict(AnnotationsHandle::LONGITUDE,false);
			// EVALUATE PREDICITONS
			float errorLong,normErrorLong,meanDiffLong;
			cv::Mat bins = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
			this->evaluate(predicted,errorLong,normErrorLong,meanDiffLong,bins);
			finalError     += errorLong;
			finalNormError += normErrorLong;
			finalMeanDiff  += meanDiffLong;
			finalBins      += bins;
			bins.release();
			predicted.clear();
			this->features_->setFlip(this->withFlip_);
		//______________________________________________________________________
		}else if(what == AnnotationsHandle::LATITUDE){
			//LATITUDE TRAINING AND PREDICTING
			std::cout<<"Latitude >>> "<<i<<"____________________________________"<<\
				"_____________________________________________________"<<std::endl;
			this->train(AnnotationsHandle::LATITUDE,false);
			this->features_->setFlip(false);
			predicted = this->predict(AnnotationsHandle::LATITUDE,false);
			// EVALUATE PREDICITONS
			float errorLat,normErrorLat,meanDiffLat;
			cv::Mat bins = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
			this->evaluate(predicted,errorLat,normErrorLat,meanDiffLat,bins);
			finalError     += errorLat;
			finalNormError += normErrorLat;
			finalMeanDiff  += meanDiffLat;
			finalBins      += bins;
			predicted.clear();
			bins.release();
			this->features_->setFlip(this->withFlip_);
		}
	}
	finalError     /= static_cast<float>(k);
	finalNormError /= static_cast<float>(k);
	finalMeanDiff  /= static_cast<float>(k);
	finalBins      /= static_cast<float>(k);

	std::cout<<">>> final-RMS-error:"<<finalError<<std::endl;
	std::cout<<">>> final-RMS-normalized-error:"<<finalNormError<<std::endl;
	std::cout<<">>> final-avg-difference:"<<finalMeanDiff<<std::endl;
	std::cout<<">>> final-bins:"<<finalBins<<std::endl;
	return finalNormError;
}
//==============================================================================
/** Reorder a vector to a given ordering.
 */
void ClassifyImages::reorderDeque(const std::vector<unsigned> &order,\
std::deque<std::string> &input){
	std::deque<std::string> copyInput = input;
	input.clear();
	for(std::size_t s=0; s<order.size();++s){
			input.push_back(copyInput[order[s]]);
	}
}
//==============================================================================
/** Do k-fold cross-validation by splitting the training folder into training-set
 * and validation-set.
 */
void ClassifyImages::crossValidation(unsigned k,unsigned fold,bool onTrain,bool
rndm){
	// READ ALL IMAGES ONCE AND NOT THEY ARE SORTED
	if(this->imageList_.empty()){
		std::vector<unsigned> order;
		this->imageList_ = Helpers::readImages(this->trainFolder_.c_str());
		this->foldSize_  = this->imageList_.size()/k;
		std::ifstream annoIn(this->annotationsTrain_.c_str());
		if(annoIn.is_open()){
			while(annoIn.good()){
				std::string line;
				std::getline(annoIn,line);
				if(!line.empty()){
					order.push_back(this->annoList_.size());
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

		// define a random order
		if(rndm){
			std::random_shuffle(order.begin(),order.end());
			ClassifyImages::reorderDeque(order,this->imageList_);
			ClassifyImages::reorderDeque(order,this->annoList_);
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
		(new PeopleDetector(3,args,false,false,colorSp,part,this->withFlip_));
	delete [] args;
}
//==============================================================================
/** Runs the final evaluation (test).
 */
std::deque<std::deque<cv::Point2f> > ClassifyImages::runTest(int colorSp,\
AnnotationsHandle::POSE what,float &normError,FeatureExtractor::FEATUREPART part){
	// LONGITUDE TRAINING AND PREDICTING
	std::deque<std::deque<cv::Point2f> > predicted;
	cv::Mat finalBins = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
	if(what == AnnotationsHandle::LONGITUDE){
		std::cout<<"Longitude >>> ______________________________________________"<<\
			"_____________________________________________________"<<std::endl;
		// BEFORE TRAINING CAMERA CALIBRATION AND OTHER SETTINGS MIGHT NEED TO BE RESET
		this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp,part);
		this->train(AnnotationsHandle::LONGITUDE,true);
		this->withFlip_ = false;
		this->resetFeatures(this->testDir_,this->testImgString_,colorSp,part);
		predicted = this->predict(AnnotationsHandle::LONGITUDE,true);
		// EVALUATE PREDICTIONS
		float errorLong,normErrorLong,meanDiffLong;
		this->evaluate(predicted,errorLong,normErrorLong,meanDiffLong,finalBins);
		normError = normErrorLong;
	}else if(what == AnnotationsHandle::LATITUDE){
		//__________________________________________________________________________
		// LATITUDE TRAINING AND PREDICTING
		std::cout<<"Latitude >>> _______________________________________________"<<\
			"_____________________________________________________"<<std::endl;
		// BEFORE TRAINING CAMERA CALIBRATION AND OTHER SETTINGS MIGHT NEED TO BE RESET
		this->resetFeatures(this->trainDir_,this->trainImgString_,colorSp,part);
		this->train(AnnotationsHandle::LATITUDE,true);
		this->withFlip_ = false;
		this->resetFeatures(this->testDir_,this->testImgString_,colorSp,part);
		predicted = this->predict(AnnotationsHandle::LATITUDE,true);
		// EVALUATE PREDICTIONS
		float errorLat,normErrorLat,meanDiffLat;
		this->evaluate(predicted,errorLat,normErrorLat,meanDiffLat,finalBins);
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
ClassifyImages &classi,float noise,float lengthSin,float lengthCos,\
GaussianProcess::kernelFunction kernel,bool useGT,\
FeatureExtractor::FEATUREPART part){
	std::deque<FeatureExtractor::FEATURE> feat(1,FeatureExtractor::IPOINTS);
	classi.init(noise,lengthSin,lengthCos,feat,kernel,useGT);

	std::deque<std::deque<std::deque<cv::Point2f> > > predictions;
	std::deque<std::deque<cv::Point2f> > tmpPrediction;
	float dummy = 0;

	for(FeatureExtractor::FEATURE f=FeatureExtractor::EDGES;\
	f<=FeatureExtractor::SKIN_BINS;++f){
		switch(f){
			case(FeatureExtractor::IPOINTS):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::IPOINTS);
				tmpPrediction = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
			case(FeatureExtractor::EDGES):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::EDGES);
				tmpPrediction  = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
			case(FeatureExtractor::SURF):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::SURF);
				tmpPrediction  = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
			case(FeatureExtractor::GABOR):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::GABOR);
				tmpPrediction  = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
			case(FeatureExtractor::SIFT):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::SIFT);
				tmpPrediction  = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
			case(FeatureExtractor::RAW_PIXELS):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::RAW_PIXELS);
				tmpPrediction  = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
			case(FeatureExtractor::HOG):
				classi.feature_ = std::deque<FeatureExtractor::FEATURE>\
					(FeatureExtractor::HOG);
				tmpPrediction  = classi.runTest(colorSp,what,dummy,part);
				predictions.push_back(tmpPrediction);
				tmpPrediction.clear();
		}
	}
	// HOW TO COMBINE THEM?BIN VOTES EVERY 20DEGREES AND AVERAGE THE WINNING BIN
	std::deque<std::deque<cv::Point2f> > finalPreds;
	for(std::size_t n=0;n<predictions[0].size();++n){// CLASSES
		std::deque<cv::Point2f> preFinalPreds;
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
				float angle = std::atan2(predictions[m][n][o].y,predictions[m][n][o].x);
				Auxiliary::angle0to360(angle);
				if(angle>=angleMin || angle<angleMax){
					unsigned degrees = static_cast<unsigned>(angle*180.0/M_PI);
					unsigned label   = degrees/20;
					++votes[label];
					bins[label] += angle;
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
			preFinalPreds.push_back(cv::Point2f(std::sin(guess),std::cos(guess)));
			votes.clear();
			bins.clear();
		}
		finalPreds.push_back(preFinalPreds);
	}
	// FINALLY EVALUATE THE FINAL PREDICTIONS
	std::cout<<"FINAL EVALUATION____________________________________________"<<\
		"________________________"<<std::endl;
	float error = 0.0,normError=0.0,meanDiff=0.0;
	cv::Mat finalBins = cv::Mat::zeros(cv::Size(18,1),CV_32FC1);
	classi.evaluate(finalPreds,error,normError,meanDiff,finalBins);
}
//==============================================================================
/** Run over multiple settings of the parameters to find the best ones.
 */
void parameterSetting(const std::string &errorsOnTrain,const std::string &errorsOnTest,\
ClassifyImages &classi,int argc,char** argv,\
const std::deque<FeatureExtractor::FEATURE> &feat,int colorSp,bool useGt,\
AnnotationsHandle::POSE what,GaussianProcess::kernelFunction kernel,unsigned folds,\
FeatureExtractor::FEATUREPART part){
  	std::ofstream train,test;
	test.open(errorsOnTest.c_str(),std::ios::out | std::ios::app);
	if(classi.what() == ClassifyImages::EVALUATE){
		train.open(errorsOnTrain.c_str(),std::ios::out | std::ios::app);
	}
	for(float v=1e-10;v<1.0;v*=10.0){
		for(float l=1;l<1e+10;l*=5.0){
//		for(float v=1e-10;v<1.1;v+=0.1){
//			for(float l=1;l<1100;l+=100.0){
			classi.init(v,l,l,feat,kernel,useGt);
			float errorTest = 0.0f;
			if(classi.what() == ClassifyImages::EVALUATE){
				errorTest = classi.runCrossValidation(folds,what,colorSp,false,part);
				test<<v<<" "<<l<<" "<<errorTest<<std::endl;
			}else if(classi.what() == ClassifyImages::TEST){
				classi.runTest(-1,AnnotationsHandle::LONGITUDE,errorTest,part);
				test<<v<<" "<<l<<" "<<errorTest<<std::endl;
				continue;
			}
			//-------------------------------------------
			classi.init(v,l,l,feat,kernel,useGt);
			float errorTrain = 0.0f;
			errorTrain = classi.runCrossValidation(folds,what,colorSp,true,part);
			train<<v<<" "<<l<<" "<<errorTrain<<std::endl;
		}
	}
	test.close();
	if(classi.what() == ClassifyImages::EVALUATE){
		train.close();
	}
}
//==============================================================================
/** Set the use of the classifier */
void ClassifyImages::setWhat(int argc,char **argv,ClassifyImages::USES use){
	this->what_ = use;
	this->doWhat(argc,argv);
}
//==============================================================================
/** Applies PCA on top of a data-row to reduce its dimensionality.
 */
cv::Mat ClassifyImages::reduceDimensionality(const cv::Mat &data,int i,\
bool train,int nEigens,int reshapeRows){
	cv::Mat preData;
	data.copyTo(preData);
	preData.convertTo(preData,CV_32FC1);
	if(!nEigens){nEigens = data.rows/4;}
	cv::Mat finalMat;
	if(train){
		this->pca_[i] = std::tr1::shared_ptr<cv::PCA>(new cv::PCA\
			(preData,cv::Mat(),CV_PCA_DATA_AS_ROW,nEigens));
	}
	finalMat = this->pca_[i]->project(preData);
	finalMat.convertTo(finalMat,CV_32FC1);
	if(this->plot_ && reshapeRows){
		for(int j=0;j<finalMat.rows;++j){
			cv::Mat test1 = this->pca_[i]->backProject(finalMat.row(j));
			cv::Mat dummy = preData.row(j), test2;
			dummy.copyTo(test2);
			test2 = test2.reshape(0,reshapeRows);
			test1 = test1.reshape(0,reshapeRows);
			cv::imshow("back_proj",test1);
			cv::imshow("original",test2);
			cv::waitKey(5);
			test1.release();
			test2.release();
			dummy.release();
		}
	}
	preData.release();
	return finalMat;
}
//==============================================================================
int main(int argc,char **argv){
	float normError = 0.0f;
	ClassifyImages classi(argc,argv,ClassifyImages::BUILD_DATA,ClassifyImages::GAUSSIAN_PROCESS);
	//--------------------------------------------------------------------------
	std::deque<FeatureExtractor::FEATURE> feat;
//	feat.push_back(FeatureExtractor::EDGES);
//	feat.push_back(FeatureExtractor::TEMPL_MATCHES);
//	feat.push_back(FeatureExtractor::GABOR);
//	feat.push_back(FeatureExtractor::HOG);
//	feat.push_back(FeatureExtractor::SURF);
//	feat.push_back(FeatureExtractor::SIFT);
//	feat.push_back(FeatureExtractor::IPOINTS);
//	feat.push_back(FeatureExtractor::SKIN_BINS);
	feat.push_back(FeatureExtractor::RAW_PIXELS);
	//--------------------------------------------------------------------------

	// DATASET 1
	float sinGP                        = 100.0;
	float cosGP                        = 125.0;
	float noiseGP                      = 1e-05;

/*
	// DATASET 2
	float sinGP                        = 70000;
	float cosGP                        = 80000;
	float noiseGP                      = 0.0001;
*/
	FeatureExtractor::FEATUREPART part = FeatureExtractor::HEAD;
	ClassifyImages::TORUN what         = ClassifyImages::run_test;
	bool useGT                         = false;
	unsigned noFolds                   = 0;
	std::string fileTrain              = "train_topGt.txt";
	std::string fileTest               = "test_topGt.txt";
	//--------------------------------------------------------------------------
	switch(what){
		case(ClassifyImages::run_build_data):
			// build data matrix
			classi.setWhat(argc,argv,ClassifyImages::BUILD_DATA);
			classi.init(noiseGP,sinGP,cosGP,feat,&GaussianProcess::sqexp,useGT);
			classi.buildDataMatrix(-1,part);
			break;
		case(ClassifyImages::run_build_pca):
			// build PCA models
			classi.setWhat(argc,argv,ClassifyImages::TEST);
			classi.init(noiseGP,sinGP,cosGP,feat,&GaussianProcess::sqexp,useGT);
			classi.buildPCAModels(-1,part);
			break;
		case(ClassifyImages::run_build_dictionary):
			// BUILD THE SIFT DICTIONARY
			classi.setWhat(argc,argv,ClassifyImages::BUILD_DICTIONARY);
			classi.buildDictionary(-1,useGT);
			break;
		case(ClassifyImages::run_test):
			// test
			classi.setWhat(argc,argv,ClassifyImages::TEST);
			classi.init(noiseGP,sinGP,cosGP,feat,&GaussianProcess::sqexp,useGT);
			classi.runTest(-1,AnnotationsHandle::LONGITUDE,normError,part);
			break;
		case(ClassifyImages::run_evaluate):
			// evaluate
			classi.setWhat(argc,argv,ClassifyImages::EVALUATE);
		 	classi.init(noiseGP,sinGP,cosGP,feat,&GaussianProcess::sqexp,useGT);
			classi.runCrossValidation(noFolds,AnnotationsHandle::LONGITUDE,-1,\
				false,part);
			break;
		case(ClassifyImages::run_find_params):
			// find parameters
			classi.setWhat(argc,argv,ClassifyImages::EVALUATE);
			parameterSetting(fileTrain,fileTest,classi,argc,argv,feat,-1,\
				useGT,AnnotationsHandle::LONGITUDE,&GaussianProcess::sqexp,\
				noFolds,part);
			break;
		case(ClassifyImages::run_multiple_class):
			// multiple classifiers
			classi.setWhat(argc,argv,ClassifyImages::TEST);
			multipleClassifier(-1,AnnotationsHandle::LONGITUDE,classi,noiseGP,\
				sinGP,cosGP,&GaussianProcess::sqexp,useGT,part);
			break;
	}
}

