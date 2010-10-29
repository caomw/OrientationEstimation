#include "annotationsHandle.h"
#include <iostream>
#include <fstream>
#include <string>
#include <opencv/cv.h>
#include <exception>
#include <opencv/highgui.h>
#include <boost/thread.hpp>
#include <boost/version.hpp>
#if BOOST_VERSION < 103500
	#include <boost/thread/detail/lock.hpp>
#endif
#include <boost/thread/xtime.hpp>
#include "eigenbackground/src/annotatepos.hh"
#include "eigenbackground/src/Helpers.hh"
using namespace std;

/** Define a post-fix increment operator for the enum \c POSE.
 */
annotationsHandle::POSE operator++(annotationsHandle::POSE &refPose, int){
	annotationsHandle::POSE oldPose = refPose;
	refPose                         = (annotationsHandle::POSE)(refPose + 1);
	return oldPose;
}

/** Mouse handler for annotating people's positions and poses.
 */
void annotationsHandle::mouseHandlerAnn(int event, int x, int y, int flags, void *param){
	cv::Point pt = cv::Point(x,y);
	y            = pt.y;
	x            = pt.x;
	static bool down = false;
	switch (event){
		case CV_EVENT_LBUTTONDOWN:
			if(choice == 'c'){
				down = true;
				cout<<"Left button down at >>> ("<<x<<","<<y<<")"<<endl;
				AnnotatePos::plotAreaTmp(image,x,y);
			}
			break;
		case CV_EVENT_MOUSEMOVE:
			if(down){
				AnnotatePos::plotAreaTmp(image,x,y);
			}
			break;
		case CV_EVENT_LBUTTONUP:
			if(choice == 'c'){
				cout<<"Left button up at >>> ("<<x<<","<<y<<")"<<endl;
				choice = ' ';
				down = false;
				ANNOTATION temp;
				temp.location = pt;
				temp.id       = annotations.size();
				annotations.push_back(temp);
				for(unsigned i=0;i!=annotations.size(); ++i){
					AnnotatePos::plotArea(image, annotations[i].location.x, \
						annotations[i].location.y);
				}
				showMenu(pt);
			}
			break;
	}
}

/** Draws the "menu" of possible poses for the current position.
 */
void annotationsHandle::showMenu(cv::Point center){
	int pose0 = 0;
	int pose1 = 0;
	int pose2 = 0;
	int pose3 = 0;
	cv::namedWindow("Poses",CV_WINDOW_AUTOSIZE);
	cv::imshow("Poses", cv::Mat(1, 300, CV_8UC1, cv::Scalar(255,255,255)));
	for(POSE p=SITTING; p<=ORIENTATION;p++){
		void *param = (void *) new unsigned int(p);
		switch(p){
			case SITTING:
				cv::createTrackbar("Sitting","Poses", &pose0, 1, \
					trackbar_callback, param);
				break;
			case STANDING:
				cv::createTrackbar("Standing","Poses", &pose1, 1, \
					trackbar_callback, param);
				break;
			case BENDING:
				cv::createTrackbar("Bending","Poses", &pose2, 1, \
					trackbar_callback, param);
				break;
			case ORIENTATION:
				cv::createTrackbar("Orientation", "Poses", &pose3, 360, \
					trackbar_callback, param);
				break;
			default:
				//do nothing
				break;
		}
	}
	cout<<"Press 'c' once the annotation for poses is done."<<endl;
	while(choice != 'c'){
		choice = (char)(cv::waitKey(0));
	}
	cvDestroyWindow("Poses");
}

/** A function that starts a new thread which handles the track-bar event.
 */
void annotationsHandle::trackBarHandleFct(int position,void *param){
	//cout<< "lock" << *(unsigned int *)(param) << endl;
	trackbarMutex.lock();
	unsigned int *ii    = (unsigned int *)(param);
	ANNOTATION lastAnno = annotations.back();
	annotations.pop_back();
	if(lastAnno.poses.size()==0){
		for(unsigned int i=0; i<4; i++){
			lastAnno.poses.push_back(0);
		}
	}
	try{
		lastAnno.poses.at(*ii) = position;
	}catch (std::exception &e){
		cout<<"Exception "<<e.what()<<endl;
		exit(1);
	}
	annotations.push_back(lastAnno);
	trackbarMutex.unlock();
	if((POSE)(*ii) == SITTING){
		cv::setTrackbarPos("Standing", "Poses", (1-position));
	} else if((POSE)(*ii) == STANDING){
		cv::setTrackbarPos("Sitting", "Poses", (1-position));
	} else if((POSE)(*ii) == ORIENTATION){
		if(position % 10 != 0){
			position = (position / 10) * 10;
			cv::setTrackbarPos("Orientation", "Poses", position);
		}
	}
	//cout<< "unlock" << *(unsigned int *)(param) << endl;
}
/** The "on change" handler for the track-bars.
 */
void annotationsHandle::trackbar_callback(int position,void *param){
	boost::thread *trackbarHandle;
	trackbarHandle = new boost::thread(&annotationsHandle::trackBarHandleFct,\
		position, param);
	//trackBarHandleFct(position, param);
	trackbarHandle->join();
}

/** Starts the annotation of the images. The parameters that need to be indicated
 * are:
 *
 * \li argv[1] -- name of directory containing the images
 * \li argv[2] -- the file contains the calibration data of the camera
 * \li argv[3] -- the file in which the annotation data needs to be stored
 */
int annotationsHandle::runAnn(int argc, char **argv){
	choice = 'c';
	if(argc != 4){
		cerr<<"usage: ./annotatepos <img_list.txt> <calib.xml> <annotation.txt>\n"<< \
		"<img_directory>   => name of directory containing the images\n"<< \
		"<calib.xml>       => the file contains the calibration data of the camera\n"<< \
		"<annotations.txt> => the file in which the annotation data needs to be stored\n"<<endl;
		exit(-1);
	} else {
		cout<<"Help info:\n"<< \
		"> press 'q' to quite before all the images are annotated;\n"<< \
		"> press 's' to save the annotations for the current image and go to the next one;\n"<<endl;
	}
	unsigned index      = 0;
	vector<string> imgs = readImages(argv[1]);
	loadCalibration(argv[2]);

	// set the handler of the mouse events to the method: <<mouseHandler>>
	image = cvLoadImage(imgs[index].c_str());
	cv::namedWindow("image");
	cvSetMouseCallback("image", mouseHandlerAnn, NULL);
	cv::imshow("image", image);

	// used to write the output stream to a file given in <<argv[3]>>
	ofstream
	ofs(argv[3]);
	if(!ofs){
		errx(1,"Cannot open file %s", argv[3]);
	}

	/* while 'q' was not pressed, annotate images and store the info in
	 * the annotation file */
	int key = 0;
	while((char)key != 'q' && index<imgs.size()) {
		key = cv::waitKey(0);
		/* if the pressed key is 's' stores the annotated positions
		 * for the current image */
		if((char)key == 's'){
			ofs<<imgs[index].substr(imgs[index].rfind("/")+1)<<" ";
			for(unsigned i=0; i!=annotations.size();++i){
				ofs <<"("<<annotations[i].location.x<<","\
					<<annotations[i].location.y<<")|";
				for(unsigned j=0;j<annotations[i].poses.size();j++){
					switch((POSE)j){
						case SITTING:
							ofs<<"(SITTING:"<<annotations[i].poses[j]<<")|";
							break;
						case STANDING:
							ofs<<"(STANDING:"<<annotations[i].poses[j]<<")|";
							break;
						case BENDING:
							ofs<<"(BENDING:"<<annotations[i].poses[j]<<")|";
							break;
						case ORIENTATION:
							ofs<<"(ORIENTATION:"<<annotations[i].poses[j]<<") ";
							break;
						default:
							cout<<"Unknown pose ;)";
							break;
					}
				}
			}
			ofs<<endl;
			cout<<"Annotations for image: "<<\
				imgs[index].substr(imgs[index].rfind("/")+1)\
				<<" were successfully saved!"<<endl;
			annotations.clear();
			cvReleaseImage(&image);

			// load the next image or break if it is the last one
			index++;
			if(index==imgs.size()){
				break;
			}
			image = cvLoadImage(imgs[index].c_str());
			cv::imshow("image", image);
		}else if(isalpha(key)){
			cout<<"key pressed >>> "<<(char)key<<"["<<key<<"]"<<endl;
		}
	}
	cout<<"Thank you for your time ;)!"<<endl;
	cvReleaseImage(&image);
	return 0;
}

/** Load annotations from file.
 */
void annotationsHandle::loadAnnotations(char* filename, vector<FULL_ANNOTATIONS> \
	&loadedAnno){
	ifstream annoFile(filename);
	vector<char*> lineVect;

	char *line = new char[1024];
	if(annoFile.is_open()){
		FULL_ANNOTATIONS tmpFullAnno;
		while(annoFile.good()){
			annoFile.getline(line,sizeof(char*)*1024);
			lineVect            = splitWhite(line,true);
			tmpFullAnno.imgFile = lineVect[0];
			for(unsigned i=1; i<lineVect.size();i++){
				ANNOTATION tmpAnno;
				vector<char *> subVect = splitLine(lineVect[i],'|');
				for(unsigned j=0; j<subVect.size();j++){
					string temp(subVect[j]);
					if(temp.find('(')!=string::npos){
						temp.erase(temp.find('('),1);
					}
					if(temp.find(')')!=string::npos){
						temp.erase(temp.find(')'),1);
					}
					subVect[j] = (char*)temp.c_str();
					if(j==0){ // location is on the first position
						vector<char *> locVect = splitLine(subVect[j],',');
						if(locVect.size()==2){
							tmpAnno.location.x = atoi(locVect[0]);
							tmpAnno.location.y = atoi(locVect[1]);
						}
					}else{
						if(tmpAnno.poses.size()<4){
							for(unsigned l=0; l<4;l++){
								tmpAnno.poses.push_back(0);
							}
						}
						vector<char *> poseVect = splitLine(subVect[j],':');
						if(poseVect.size()==2){
							tmpAnno.poses[(POSE)(j-1)] = atoi(poseVect[1]);
						}
					}
				}
				tmpAnno.id = tmpFullAnno.annos.size();
				tmpFullAnno.annos.push_back(tmpAnno);
			}
			loadedAnno.push_back(tmpFullAnno);
			tmpFullAnno.annos.clear();
		}
		annoFile.close();
	}
}

/** Checks to see if a location can be assigned to a specific ID given the
 * new distance.
 */
bool annotationsHandle::canBeAssigned(vector<ASSIGNED> &idAssignedTo, short int \
	id, double newDist, short int to){
	bool isHere = false;
	for(unsigned int i=0; i<idAssignedTo.size(); i++){
		if(idAssignedTo[i].id == id){
			isHere = true;
			if(idAssignedTo[i].dist>newDist){
				idAssignedTo[i].to   = to;
				idAssignedTo[i].dist = newDist;
				return true;
			}
		}
	}
	if(!isHere){
		bool alreadyTo = false;
		for(unsigned i=0;i<idAssignedTo.size();i++){
			if(idAssignedTo[i].to == to){
				alreadyTo = true;
				if(idAssignedTo[i].dist>newDist){
					//assigned the id to this one and un-assign the old one
					ASSIGNED temp;
					temp.id   = id;
					temp.to   = to;
					temp.dist = newDist;
					idAssignedTo.push_back(temp);
					idAssignedTo.erase(idAssignedTo.begin()+i);
					return true;
				}
			}
		}
		if(!alreadyTo){
			ASSIGNED temp;
			temp.id   = id;
			temp.to   = to;
			temp.dist = newDist;
			idAssignedTo.push_back(temp);
			return true;
		}
	}
	return false;
}

/** Correlate annotations' from locations in \c annoOld to locations in \c
 * annoNew through IDs.
 */
void annotationsHandle::correltateLocs(vector<ANNOTATION> &annoOld, \
	vector<ANNOTATION> &annoNew, vector<ASSIGNED> &idAssignedTo){
	vector< vector<double> > distMatrix;

	//1. compute the distances between all annotations
	for(unsigned k=0;k<annoNew.size();k++){
		distMatrix.push_back(vector<double>());
		for(unsigned l=0;l<annoOld.size();l++){
			distMatrix[k].push_back(0.0);
			distMatrix[k][l] = dist(annoNew[k].location, annoOld[l].location);
		}
	}

	//2. assign annotations between the 2 groups of annotations
	bool canAssign = true;
	while(canAssign){
		canAssign = false;
		for(unsigned k=0;k<distMatrix.size();k++){ //for each row in new
			double minDist = (double)INFINITY;
			for(unsigned l=0;l<distMatrix[k].size();l++){ // loop over all old
				if(distMatrix[k][l]<minDist && canBeAssigned(idAssignedTo, \
					annoOld[l].id, distMatrix[k][l], annoNew[k].id)){
					minDist       =	distMatrix[k][l];
					canAssign     = true;
				}
			}
		}
	}

	//3. update the ids to the new one to correspond to the ids of the old one!!
	for(unsigned i=0;i<annoNew.size();i++){
		for(unsigned n=0;n<idAssignedTo.size();n++){
			if(annoNew[i].id == idAssignedTo[n].to){
				if(idAssignedTo[n].to != idAssignedTo[n].id){
					for(unsigned j=0;j<annoNew.size();j++){
						if(annoNew[j].id == idAssignedTo[n].id){
							annoNew[j].id = -1;
						}
					}
				}
				annoNew[i].id = idAssignedTo[n].id;
				break;
			}
		}
	}

}

/** Computes the average distance from the predicted location and the annotated
 * one, the number of unpredicted people in each image and the differences in the
 * pose estimation.
 */
void annotationsHandle::annoDifferences(vector<FULL_ANNOTATIONS> &train, \
	vector<FULL_ANNOTATIONS> &test, double &avgDist, double &Ndiff, double \
	avgOrientDiff, double poseDiff){
	if(train.size() != test.size()) {exit(1);}
	for(unsigned i=0;i<train.size();i++){
		if(train[i].imgFile != test[i].imgFile) {
			errx(1,"Images on positions %i do not correspond",i);
		}

		/* the images might not be in the same ordered so they need to be assigned
		 * to the closest one to them */
		vector<ASSIGNED> idAssignedTo;
		//0. if one of them is 0 then no correlation can be done
		if(train[i].annos.size()!=0 && test[i].annos.size()!=0){
			correltateLocs(train[i].annos, test[i].annos,idAssignedTo);
		}
		//1. update average difference for the current image
		for(unsigned l=0; l<idAssignedTo.size(); l++){
			cout<<idAssignedTo[l].id<<" assigned to "<<idAssignedTo[l].to<<endl;
			avgDist += idAssignedTo[l].dist;
		}
		if(idAssignedTo.size()>0){
			avgDist /= idAssignedTo.size();
		}

		//2. update the difference in number of people detected
		if(train[i].annos.size()!=test[i].annos.size()){
			Ndiff += abs((double)train[i].annos.size() - \
				(double)test[i].annos.size());
		}

		//3. update the poses estimation differences
		unsigned int noCorresp = 0;
		for(unsigned int l=0;l<train[i].annos.size();l++){
			for(unsigned int k=0;k<test[i].annos.size();k++){
				if(train[i].annos[l].id == test[i].annos[k].id){
					for(unsigned int m=0;m<train[i].annos[l].poses.size()-1;m++){
						if(train[i].annos[l].poses[m]!=test[i].annos[k].poses[m]){
							poseDiff += 1;
						}
					}
					avgOrientDiff += abs((double)train[i].annos[l].poses[3] - \
						(double)test[i].annos[k].poses[3]);
					noCorresp++;
					break;
				}
			}
		}
		if(noCorresp>0){
			avgOrientDiff /= noCorresp;
		}
		cout<<endl;
	}
	cout<<"avgDist: "<<avgDist<<endl;
	cout<<"Ndiff: "<<Ndiff<<endl;
	cout<<"PoseDiff: "<<poseDiff<<endl;
	cout<<"avgOrientDiff: "<<avgOrientDiff<<endl;

	avgOrientDiff /= train.size();
	avgDist /= train.size();
}

/** Displays the complete annotations for all images.
 */
void annotationsHandle::displayFullAnns(vector<FULL_ANNOTATIONS> &fullAnns){
	for(unsigned int i=0; i<fullAnns.size();i++){
		cout<<"Image name: "<<fullAnns[i].imgFile<<endl;
		for(unsigned int j=0; j<fullAnns[i].annos.size();j++){
			cout<<"Annotation Id:"<<fullAnns[i].annos[j].id<<\
				"_____________________________________________"<<endl;
			cout<<"Location: ["<<fullAnns[i].annos[j].location.x<<","\
				<<fullAnns[i].annos[j].location.y<<"]"<<endl;
			cout<<"Pose: (Sitting: "<<fullAnns[i].annos[j].poses[0]<<\
				"),(Standing: "<<fullAnns[i].annos[j].poses[1]<<\
				"),(Bending: "<<fullAnns[i].annos[j].poses[2]<<\
				"),(Orientation: "<<fullAnns[i].annos[j].poses[3]<<")"<<endl;
		}
		cout<<endl;
	}
}

/** Starts the annotation of the images. The parameters that need to be indicated
 * are:
 *
 * \li argv[1] -- train file with the correct annotations;
 * \li argv[2] -- test file with predicted annotations;
 */
int annotationsHandle::runEvaluation(int argc, char **argv){
	if(argc != 3){
		cerr<<"usage: cmmd <train_annotations.txt> <train_annotations.txt>\n"<< \
		"<train_annotations.txt> => file containing correct annotations\n"<< \
		"<test_annotations.txt>  => file containing predicted annotations\n"<<endl;
		exit(-1);
	}

	vector<FULL_ANNOTATIONS> allAnnoTrain;
	vector<FULL_ANNOTATIONS> allAnnoTest;
	loadAnnotations(argv[1],allAnnoTrain);
	loadAnnotations(argv[2],allAnnoTest);

	displayFullAnns(allAnnoTrain);

	double avgDist = 0, Ndiff = 0, avgOrientDiff = 0, poseDiff = 0;
	annoDifferences(allAnnoTrain, allAnnoTest, avgDist, Ndiff, avgOrientDiff, \
		poseDiff);
}

char annotationsHandle::choice;
boost::mutex annotationsHandle::trackbarMutex;
IplImage *annotationsHandle::image;
vector<annotationsHandle::ANNOTATION> annotationsHandle::annotations;

int main(int argc, char **argv){
	//annotationsHandle::runAnn(argc,argv);
	annotationsHandle::runEvaluation(argc,argv);
}
