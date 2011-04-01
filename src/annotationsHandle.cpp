/* annotationsHandle.cpp
 * Author: Silvia-Laura Pintea
 */
#include "annotationsHandle.h"

//==============================================================================
/** Initializes all the values of the class variables.
 */
void annotationsHandle::init(){
	image     = NULL;
	choice    = ' ';
	withPoses = false;
	poseSize  = 5;
	poseNames.push_back("SITTING");
	poseNames.push_back("STANDING");
	poseNames.push_back("BENDING");
	poseNames.push_back("LONGITUDE");
	poseNames.push_back("LATITUDE");
}
//==============================================================================
/** Define a post-fix increment operator for the enum \c POSE.
 */
annotationsHandle::POSE operator++(annotationsHandle::POSE &refPose, int){
	annotationsHandle::POSE oldPose = refPose;
	refPose                         = (annotationsHandle::POSE)(refPose + 1);
	return oldPose;
}
//==============================================================================
/** Mouse handler for annotating people's positions and poses.
 */
void annotationsHandle::mouseHandlerAnn(int event, int x, int y, int flags, void *param){
	cv::Point3d bkp = backproject(cv::Point(x,y));
	static bool down = false;
	switch (event){
		case CV_EVENT_LBUTTONDOWN:
			if(choice == 'c'){
				down = true;
				cout<<"Left button down at >>> ("<<x<<","<<y<<")"<<endl;
				Annotate::plotAreaTmp(image,(float)bkp.x,(float)bkp.y);
			}
			break;
		case CV_EVENT_MOUSEMOVE:
			if(down){
				Annotate::plotAreaTmp(image,(float)bkp.x,(float)bkp.y);
			}
			break;
		case CV_EVENT_LBUTTONUP:
			if(choice == 'c'){
				cout<<"Left button up at >>> ("<<x<<","<<y<<")"<<endl;
				choice = ' ';
				down = false;
				annotationsHandle::ANNOTATION temp;
				temp.location = cv::Point2d(bkp.x,bkp.y);
				temp.id       = annotations.size();
				temp.poses.assign(poseSize, 0);
				temp.poses[(int)LATITUDE] = 90;
				annotations.push_back(temp);
				showMenu(cv::Point2f(x,y));
				for(unsigned i=0;i!=annotations.size(); ++i){
					Annotate::plotArea(image, (float)annotations[i].location.x, \
						(float)annotations[i].location.y);
				}
			}
			break;
	}
}
//==============================================================================
/** Rotate matrix wrt to the camera location.
 */
cv::Mat annotationsHandle::rotateWrtCamera(cv::Point feetLocation,\
cv::Point cameraLocation, cv::Mat toRotate, cv::Point &borders){
	// GET THE ANGLE TO ROTATE WITH
	double cameraAngle = std::atan2((feetLocation.y-cameraLocation.y),\
						(feetLocation.x-cameraLocation.x));
	cameraAngle = (cameraAngle+M_PI/2.0);
	if(cameraAngle>2.0*M_PI){
		cameraAngle -= 2.0*M_PI;
	}
	cameraAngle *= (180.0/M_PI);

	// ADD A BLACK BORDER TO THE ORIGINAL IMAGE
	double diag       = std::sqrt(toRotate.cols*toRotate.cols+toRotate.rows*\
						toRotate.rows);
	borders.x         = std::ceil((diag-toRotate.cols)/2.0);
	borders.y         = std::ceil((diag-toRotate.rows)/2.0);
	cv::Mat srcRotate = cv::Mat::zeros(cv::Size(toRotate.cols+2*borders.x,\
						toRotate.rows+2*borders.y),toRotate.type());
	cv::copyMakeBorder(toRotate,srcRotate,borders.y,borders.y,borders.x,\
		borders.x,cv::BORDER_CONSTANT);

	// GET THE ROTATION MATRIX
	cv::Mat rotationMat = cv::getRotationMatrix2D(cv::Point2f(\
		srcRotate.cols/2.0,srcRotate.rows/2.0),cameraAngle, 1.0);

	// ROTATE THE IMAGE WITH THE ROTATION MATRIX
	cv::Mat rotated = cv::Mat::zeros(srcRotate.size(),toRotate.type());
	cv::warpAffine(srcRotate, rotated, rotationMat, srcRotate.size());

	rotationMat.release();
	srcRotate.release();
	return rotated;
}
//==============================================================================
/** Shows how the selected orientation looks on the image.
 */
void annotationsHandle::drawLatitude(cv::Point head, cv::Point feet,\
unsigned int orient, annotationsHandle::POSE pose){
	cv::namedWindow("Latitude");
	unsigned int length = 80;
	double angle = (M_PI * orient)/180;

	// GET THE TEMPLATE AND DETERMINE ITS SIZE
	vector<CvPoint> points;
	genTemplate2(feet, persHeight, camHeight, points);
	int maxX=0,maxY=0,minX=image->width,minY=image->height;
	for(unsigned i=0;i<points.size();i++){
		if(maxX<points[i].x){maxX = points[i].x;}
		if(maxY<points[i].y){maxY = points[i].y;}
		if(minX>points[i].x){minX = points[i].x;}
		if(minY>points[i].y){minY = points[i].y;}
	}

	minX = std::max(minX-10,0);
	minY = std::max(minY-10,0);
	maxX = std::min(maxX+10,image->width);
	maxY = std::min(maxY+10,image->height);
	// ROTATE THE TEMPLATE TO HORIZONTAL SO WE CAN SEE IT
	cv::Mat tmpImage((image),cv::Rect(cv::Point(minX,minY),\
		cv::Size(maxX-minX,maxY-minY)));
	cv::Point stupid;
	cv::Mat tmpImg = rotateWrtCamera(head, feet, tmpImage, stupid);
	cv::Mat large;
	cv::resize(tmpImg,large,cv::Size(0,0),1.5,1.5, cv::INTER_CUBIC);
	cv::namedWindow("Latitude");

	// DRAW THE LINE ON WHICH THE ARROW SITS
	cv::Point center(large.cols*1/4,large.rows/2), point1, point2;
	point1.x = center.x - 0.3*length * cos(angle + M_PI/2.0);
	point1.y = center.y + 0.3*length * sin(angle + M_PI/2.0);
	point2.x = center.x - length * cos(angle + M_PI/2.0);
	point2.y = center.y + length * sin(angle + M_PI/2.0);
	cv::clipLine(large.size(),point1,point2);
	cv::line(large,point1,point2,cv::Scalar(100,50,255),2,8,0);

	// DRAW THE TOP OF THE ARROW
	cv::Point point3, point4, point5;
	point3.x = center.x - length * 4/5 * cos(angle + M_PI/2.0);
	point3.y = center.y + length * 4/5 * sin(angle + M_PI/2.0);
	point4.x = point3.x - 7 * cos(M_PI + angle);
	point4.y = point3.y + 7 * sin(M_PI + angle);
	point5.x = point3.x - 7 * cos(M_PI + angle  + M_PI);
	point5.y = point3.y + 7 * sin(M_PI + angle  + M_PI);

	// FILL THE POLLY CORRESPONDING TO THE ARROW
	cv::Point *pts = new cv::Point[4];
	pts[0] = point4;
	pts[1] = point2;
	pts[2] = point5;
	cv::fillConvexPoly(large,pts,3,cv::Scalar(100,50,255),8,0);
	delete [] pts;

	// PUT A CIRCLE ON THE CENTER POINT
	cv::circle(large,center,1,cv::Scalar(255,50,0),1,8,0);
	cv::imshow("Latitude", large);
	tmpImg.release();
	large.release();
	tmpImage.release();
}
//==============================================================================
/** Shows how the selected orientation looks on the image.
 */
void annotationsHandle::drawOrientation(cv::Point center, unsigned int orient,\
annotationsHandle::POSE pose){
	unsigned int length = 60;
	double angle = (M_PI * orient)/180;
	cv::Point point1, point2;
	point1.x = center.x - length * cos(angle);
	point1.y = center.y + length * sin(angle);
	point2.x = center.x - length * cos(angle + M_PI);
	point2.y = center.y + length * sin(angle + M_PI);

	cv::Size imgSize(image->width,image->height);
	cv::clipLine(imgSize,point1,point2);

	IplImage *img = cvCloneImage(image);
	cv::Mat tmpImage(img);
	cv::line(tmpImage,point1,point2,cv::Scalar(100,255,0),2,8,0);

	cv::Point point3, point4, point5;
	point3.x = center.x - length * 4/5 * cos(angle + M_PI);
	point3.y = center.y + length * 4/5 * sin(angle + M_PI);
	point4.x = point3.x - 7 * cos(M_PI/2.0 + angle);
	point4.y = point3.y + 7 * sin(M_PI/2.0 + angle);
	point5.x = point3.x - 7 * cos(M_PI/2.0 + angle  + M_PI);
	point5.y = point3.y + 7 * sin(M_PI/2.0 + angle  + M_PI);

	cv::Point *pts = new cv::Point[4];
	pts[0] = point4;
	pts[1] = point2;
	pts[2] = point5;
	cv::fillConvexPoly(tmpImage,pts,3,cv::Scalar(255,50,0),8,0);

	delete [] pts;
	cv::circle(tmpImage,center,1,cv::Scalar(255,50,0),1,8,0);
	cv::imshow("image", tmpImage);
	tmpImage.release();
	cvReleaseImage(&img);
}
//==============================================================================
/** Draws the "menu" of possible poses for the current position.
 */
void annotationsHandle::showMenu(cv::Point center){
	int pose0 = 0;
	int pose1 = 0;
	int pose2 = 0;
	int pose3 = 0;
	int pose4 = 90;
	cv::namedWindow("Poses",CV_WINDOW_AUTOSIZE);
	IplImage *tmpImg = cvCreateImage(cv::Size(400,1),8,1);

	POSE sit = SITTING, stand = STANDING, bend = BENDING, longi = LONGITUDE,
		lat = LATITUDE;
	for(POSE p=SITTING; p<=LATITUDE;p++){
		switch(p){
			case SITTING:
				if(withPoses){
					cv::createTrackbar("Sitting","Poses", &pose0, 1, \
						trackbar_callback, &sit);
				}
				break;
			case STANDING:
				if(withPoses){
					cv::createTrackbar("Standing","Poses", &pose1, 1, \
						trackbar_callback, &stand);
				}
				break;
			case BENDING:
				if(withPoses){
					cv::createTrackbar("Bending","Poses", &pose2, 1, \
						trackbar_callback, &bend);
				}
				break;
			case LONGITUDE:
				cv::createTrackbar("Longitude", "Poses", &pose3, 360, \
					trackbar_callback, &longi);
				break;
			case LATITUDE:
				cv::createTrackbar("Latitude", "Poses", &pose4, 180, \
					trackbar_callback, &lat);
				break;
			default:
				//do nothing
				break;
		}
	}
	cv::imshow("Poses", tmpImg);

	cout<<"Press 'c' once the annotation for poses is done."<<endl;
	while(choice != 'c' && choice != 'C'){
		choice = (char)(cv::waitKey(0));
	}
	cvReleaseImage(&tmpImg);
	cv::destroyWindow("Poses");
	cv::destroyWindow("Latitude");
}
//==============================================================================
/** A function that starts a new thread which handles the track-bar event.
 */
void annotationsHandle::trackBarHandleFct(int position,void *param){
	unsigned int *ii                       = (unsigned int *)(param);
	annotationsHandle::ANNOTATION lastAnno = annotations.back();
	annotations.pop_back();
	if(lastAnno.poses.empty()){
		lastAnno.poses.assign(poseSize,0);
		lastAnno.poses[(int)LATITUDE] = 90;
	}

	// DRAW THE ORIENTATION TO SEE IT
	vector<CvPoint> points;
	genTemplate2(lastAnno.location, persHeight, camHeight, points);
	cv::Point headCenter((points[12].x+points[14].x)/2,\
		(points[12].y+points[14].y)/2);
	if((POSE)(*ii)==LONGITUDE){
		drawOrientation(headCenter, position, (POSE)(*ii));
	}else if((POSE)(*ii)==LATITUDE){
		drawLatitude(headCenter, lastAnno.location, position, (POSE)(*ii));
	}

	// FOR ALL CASES STORE THE POSITION
	try{
		lastAnno.poses.at(*ii) = position;
	}catch (std::exception &e){
		cout<<"Exception "<<e.what()<<endl;
		exit(1);
	}
	annotations.push_back(lastAnno);

	// FIX TRACKBARS
	if((POSE)(*ii) == LATITUDE || (POSE)(*ii) == LONGITUDE){
		if(position % 10 != 0){
			position = (int)(position / 10) * 10;
			if((POSE)(*ii) == LATITUDE){
				cv::setTrackbarPos("Latitude", "Poses", position);
			}else{
				cv::setTrackbarPos("Longitude", "Poses", position);
			}
		}
	}else if((POSE)(*ii) == SITTING){
		int oppPos = cv::getTrackbarPos("Standing","Poses");
		if(oppPos == position){
			cv::setTrackbarPos("Standing", "Poses", (1-oppPos));
		}
	}else if((POSE)(*ii) == STANDING){
		int oppPos = cv::getTrackbarPos("Sitting","Poses");
		if(oppPos == position){
			cv::setTrackbarPos("Sitting", "Poses", (1-oppPos));
		}
	}
}
//==============================================================================
/** The "on change" handler for the track-bars.
 */
void annotationsHandle::trackbar_callback(int position,void *param){
	trackBarHandleFct(position, param);
}
//==============================================================================
/** Plots the hull indicated by the parameter \c hull on the given image.
 */
void annotationsHandle::plotHull(IplImage *img, std::vector<CvPoint> &hull){
	hull.push_back(hull.front());
	for(unsigned i=1; i<hull.size(); ++i){
		cvLine(img, hull[i-1],hull[i],CV_RGB(255,0,0),2);
	}
}
//==============================================================================
/** Starts the annotation of the images. The parameters that need to be indicated
 * are:
 * \li step       -- every "step"^th image is opened for annotation
 * \li usedImages -- the folder where the annotated images are moved
 * \li imgIndex   -- the image index from which to start
 * \li argv[1]    -- name of directory containing the images
 * \li argv[2]    -- the file contains the calibration data of the camera
 * \li argv[3]    -- the file in which the annotation data needs to be stored
 */
int annotationsHandle::runAnn(int argc, char **argv, unsigned step, std::string \
usedImages, int imgIndex){
	init();
	if(imgIndex!= -1){
		imgIndex += step;
	}
	choice = 'c';
	if(argc != 5){
		cerr<<"usage: ./annotatepos <img_list.txt> <calib.xml> <annotation.txt>\n"<< \
		"<img_directory>   => name of directory containing the images\n"<< \
		"<calib.xml>       => the file contains the calibration data of the camera\n"<< \
		"<prior.txt>       => the file containing the location prior\n"<< \
		"<annotations.txt> => the file in which the annotation data needs to be stored\n"<<endl;
		exit(1);
	} else {
		cout<<"Help info:\n"<< \
		"> press 'q' to quite before all the images are annotated;\n"<< \
		"> press 'n' to skip to the next image without saving the current one;\n"<< \
		"> press 's' to save the annotations for the current image and go to the next one;\n"<<endl;
	}
	unsigned index                = 0;
	cerr<<"Loading the images...."<< argv[1] << endl;
	std::vector<std::string> imgs = readImages(argv[1],imgIndex);
	cerr<<"Loading the calibration...."<< argv[2] << endl;
	loadCalibration(argv[2]);
	std::vector<CvPoint> priorHull;
	cerr<<"Loading the location prior...."<< argv[3] << endl;
	loadPriorHull(argv[3], priorHull);

	std::cerr<<"LATITUDE: Only looking upwards or downwards matters!"<<std::endl;

	// set the handler of the mouse events to the method: <<mouseHandler>>
	image = cvLoadImage(imgs[index].c_str());
	plotHull(image, priorHull);

	cv::namedWindow("image");
	cvSetMouseCallback("image", mouseHandlerAnn, NULL);
	cv::imshow("image", image);

	// used to write the output stream to a file given in <<argv[3]>>
	ofstream annoOut;
	annoOut.open(argv[4], ios::out | ios::app);
	if(!annoOut){
		errx(1,"Cannot open file %s", argv[4]);
	}
	annoOut.seekp(0, ios::end);

	/* while 'q' was not pressed, annotate images and store the info in
	 * the annotation file */
	int key = 0;
	while((char)key != 'q' && (char)key != 'Q' && index<imgs.size()) {
		key = cv::waitKey(0);
		/* if the pressed key is 's' stores the annotated positions
		 * for the current image */
		if((char)key == 's'){
			annoOut<<imgs[index].substr(imgs[index].rfind("/")+1);
			for(unsigned i=0; i!=annotations.size();++i){
				annoOut <<" ("<<annotations[i].location.x<<","\
					<<annotations[i].location.y<<")|";
				for(unsigned j=0;j<annotations[i].poses.size();j++){
					annoOut<<"("<<poseNames[j]<<":"<<annotations[i].poses[j]<<")";
					if(j<annotations[i].poses.size()-1){
						annoOut<<"|";
					}
				}
			}
			annoOut<<endl;
			cout<<"Annotations for image: "<<\
				imgs[index].substr(imgs[index].rfind("/")+1)\
				<<" were successfully saved!"<<endl;
			//clean the annotations and release the image
			for(unsigned ind=0; ind<annotations.size(); ind++){
				annotations[ind].poses.clear();
			}
			annotations.clear();
			cvReleaseImage(&image);

			//move image to a different directory to keep track of annotated images
			string currLocation = imgs[index].substr(0,imgs[index].rfind("/"));
			string newLocation  = currLocation.substr(0,currLocation.rfind("/")) + \
				"/annotated"+usedImages+"/";
			if(!boost::filesystem::is_directory(newLocation)){
				boost::filesystem::create_directory(newLocation);
			}
			newLocation += imgs[index].substr(imgs[index].rfind("/")+1);
			cerr<<"NEW LOCATION >> "<<newLocation<<endl;
			cerr<<"CURR LOCATION >> "<<currLocation<<endl;
			if(rename(imgs[index].c_str(), newLocation.c_str())){
				perror(NULL);
			}

			// load the next image or break if it is the last one
			// skip to the next step^th image
			while(index+step>imgs.size() && step>0){
				step = step/10;
			}
			index += step;
			if(index>=imgs.size()){
				break;
			}

			image = cvLoadImage(imgs[index].c_str());
			plotHull(image, priorHull);
			cv::imshow("image", image);
		}else if((char)key == 'n'){
			cout<<"Annotations for image: "<<\
				imgs[index].substr(imgs[index].rfind("/")+1)\
				<<" NOT saved!"<<endl;

			//clean the annotations and release the image
			for(unsigned ind=0; ind<annotations.size(); ind++){
				annotations[ind].poses.clear();
			}
			annotations.clear();
			cvReleaseImage(&image);

			// skip to the next step^th image
			while(index+step>imgs.size() && step>0){
				step = step/10;
			}
			index += step;
			if(index>=imgs.size()){
				break;
			}
			image = cvLoadImage(imgs[index].c_str());
			plotHull(image, priorHull);
			cv::imshow("image", image);
		}else if(isalpha(key)){
			cout<<"key pressed >>> "<<(char)key<<"["<<key<<"]"<<endl;
		}
	}
	annoOut.close();
	cout<<"Thank you for your time ;)!"<<endl;
	cvReleaseImage(&image);
	cvDestroyWindow("image");
	return 0;
}
//==============================================================================
/** Load annotations from file.
 */
void annotationsHandle::loadAnnotations(char* filename, \
std::vector<annotationsHandle::FULL_ANNOTATIONS> &loadedAnno){
	init();
	ifstream annoFile(filename);

	std::cout<<"Loading annotations of...."<<filename<<std::endl;
	if(annoFile.is_open()){
		annotationsHandle::FULL_ANNOTATIONS tmpFullAnno;
		while(annoFile.good()){
			char *line = new char[1024];
			annoFile.getline(line,sizeof(char*)*1024);
			std::vector<std::string> lineVect = splitLine(line,' ');

			// IF IT IS NOT AN EMPTY FILE
			if(lineVect.size()>0){
				tmpFullAnno.imgFile = std::string(lineVect[0]);
				for(unsigned i=1; i<lineVect.size();i++){
					annotationsHandle::ANNOTATION tmpAnno;
					std::vector<std::string> subVect = \
						splitLine(const_cast<char*>(lineVect[i].c_str()),'|');
					for(unsigned j=0; j<subVect.size();j++){
						std::string temp(subVect[j]);
						if(temp.find("(")!=string::npos){
							temp.erase(temp.find("("),1);
						}
						if(temp.find(")")!=string::npos){
							temp.erase(temp.find(")"),1);
						}
						subVect[j] = temp;
						if(j==0){
							// location is on the first position
							std::vector<std::string> locVect = \
								splitLine(const_cast<char*>(subVect[j].c_str()),',');
							if(locVect.size()==2){
								char *pEndX, *pEndY;
								tmpAnno.location.x = strtol(locVect[0].c_str(),&pEndX,10);
								tmpAnno.location.y = strtol(locVect[1].c_str(),&pEndY,10);
							}
						}else{
							if(tmpAnno.poses.empty()){
								tmpAnno.poses = std::vector<unsigned int>(poseSize,0);
							}
							std::vector<std::string> poseVect = \
								splitLine(const_cast<char*>(subVect[j].c_str()),':');
							if(poseVect.size()==2){
								char *pEndP;
								tmpAnno.poses[(POSE)(j-1)] = \
									strtol(poseVect[1].c_str(),&pEndP,10);
							}
						}
					}
					tmpAnno.id = tmpFullAnno.annos.size();
					tmpFullAnno.annos.push_back(tmpAnno);
					tmpAnno.poses.clear();
				}
				loadedAnno.push_back(tmpFullAnno);
				tmpFullAnno.annos.clear();
			}
			delete [] line;
		}
		annoFile.close();
	}
}
//==============================================================================
/** Writes a given FULL_ANNOTATIONS structure into a given file.
 */
void annotationsHandle::writeAnnoToFile(\
std::vector<annotationsHandle::FULL_ANNOTATIONS> fullAnno, std::string fileName){
	// OPEN THE FILE TO WRITE ANNOTATIONS
	ofstream annoOut;
	annoOut.open(fileName.c_str(), ios::out | ios::app);
	if(!annoOut){
		errx(1,"Cannot open file %s", fileName.c_str());
	}
	annoOut.seekp(0, ios::end);

	for(std::size_t k=0; k<fullAnno.size();++k){
		// WRITE THE IMAGE NAME
		annoOut<<fullAnno[k].imgFile<<" ";

		// FOR EACH ANNOTATION IN THE ANNOTATIONS ARRAY
		for(std::size_t i=0; i<fullAnno[k].annos.size();++i){

			// WRITE THE LOCATION OF THE DETECTED PERSON
			annoOut<<"("<<fullAnno[k].annos[i].location.x<<","<<\
				fullAnno[k].annos[i].location.y<<")|";
			for(std::size_t j=0;j<fullAnno[k].annos[i].poses.size();j++){
				annoOut<<"("<<(POSE)(j)<<":"<<fullAnno[k].annos[i].poses[j]<<")|";
			}
		}
		annoOut<<endl;
		cout<<"Annotations for image: "<<fullAnno[k].imgFile<<\
			" were successfully saved!"<<endl;
	}
	annoOut.close();
}
//==============================================================================
/** Checks to see if a location can be assigned to a specific ID given the
 * new distance.
 */
bool annotationsHandle::canBeAssigned(std::vector<annotationsHandle::ASSIGNED>\
&idAssignedTo, short int id, double newDist, short int to){
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
					annotationsHandle::ASSIGNED temp;
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
			annotationsHandle::ASSIGNED temp;
			temp.id   = id;
			temp.to   = to;
			temp.dist = newDist;
			idAssignedTo.push_back(temp);
			return true;
		}
	}
	return false;
}
//==============================================================================
/** Correlate annotations' from locations in \c annoOld to locations in \c
 * annoNew through IDs.
 */
void annotationsHandle::correltateLocs(std::vector<annotationsHandle::ANNOTATION>\
&annoOld, std::vector<annotationsHandle::ANNOTATION> &annoNew,\
std::vector<annotationsHandle::ASSIGNED> &idAssignedTo){
	std::vector< std::vector<double> > distMatrix;

	//1. compute the distances between all annotations
	for(unsigned k=0;k<annoNew.size();k++){
		distMatrix.push_back(std::vector<double>());
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
//==============================================================================
/** Computes the average distance from the predicted location and the annotated
 * one, the number of unpredicted people in each image and the differences in the
 * pose estimation.
 */
void annotationsHandle::annoDifferences(std::vector<annotationsHandle::FULL_ANNOTATIONS>\
&train, std::vector<annotationsHandle::FULL_ANNOTATIONS> &test, double &avgDist,\
double &Ndiff, double ssdLongDiff, double ssdLatDiff, double poseDiff){
	if(train.size() != test.size()) {
		std::cerr<<"Training annotations and test annotations have different sizes";
		exit(1);
	}
	for(unsigned i=0;i<train.size();i++){
		if(train[i].imgFile != test[i].imgFile) {
			errx(1,"Images on positions %i do not correspond",i);
		}

		/* the images might not be in the same ordered so they need to be assigned
		 * to the closest one to them */
		std::vector<annotationsHandle::ASSIGNED> idAssignedTo;
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
					// FOR POSES COMPUTE THE DIFFERENCES
					if(withPoses){
						for(unsigned int m=0;m<train[i].annos[l].poses.size()-1;m++){
							poseDiff += abs((double)train[i].annos[l].poses[m] - \
								(double)test[i].annos[k].poses[m])/2.0;
						}
					}

					// SSD between the predicted values and the correct ones
					int longPos = (int)LONGITUDE, latPos = (int)LATITUDE;
					double angleTrain = ((double)train[i].annos[l].poses[longPos]*M_PI/180.0);
					double angleTest  = ((double)test[i].annos[k].poses[longPos]*M_PI/180.0);
					ssdLongDiff += pow(cos(angleTrain)-cos(angleTest),2) +\
									pow(sin(angleTrain)-sin(angleTest),2);
					angleTrain = ((double)train[i].annos[l].poses[latPos]*M_PI/180.0);
					angleTest  = ((double)test[i].annos[k].poses[latPos]*M_PI/180.0);
					ssdLatDiff += pow(cos(angleTrain)-cos(angleTest),2) +\
									pow(sin(angleTrain)-sin(angleTest),2);
				}
			}
		}
		cout<<endl;
	}
	cout<<"avgDist: "<<avgDist<<endl;
	cout<<"Ndiff: "<<Ndiff<<endl;
	cout<<"PoseDiff: "<<poseDiff<<endl;
	cout<<"ssdLongDiff: "<<ssdLongDiff<<endl;
	cout<<"ssdLatDiff: "<<ssdLatDiff<<endl;

	ssdLongDiff /= train.size();
	ssdLatDiff /= train.size();
	avgDist /= train.size();
}
//==============================================================================
/** Displays the complete annotations for all images.
 */
void annotationsHandle::displayFullAnns(std::vector<annotationsHandle::FULL_ANNOTATIONS>\
&fullAnns){
	for(unsigned int i=0; i<fullAnns.size();i++){
		cout<<"Image name: "<<fullAnns[i].imgFile<<endl;
		for(unsigned int j=0; j<fullAnns[i].annos.size();j++){
			cout<<"Annotation Id:"<<fullAnns[i].annos[j].id<<\
				"_____________________________________________"<<endl;
			cout<<"Location: ["<<fullAnns[i].annos[j].location.x<<","\
				<<fullAnns[i].annos[j].location.y<<"]"<<endl;
			for(unsigned l=0; l<poseSize; l++){
				cout<<"("<<poseNames[l]<<": "<<fullAnns[i].annos[j].poses[l]<<")";
			}
			std::cout<<std::endl;
		}
		cout<<endl;
	}
}
//==============================================================================
/** Starts the annotation of the images. The parameters that need to be indicated
 * are:
 *
 * \li argv[1] -- train file with the correct annotations;
 * \li argv[2] -- test file with predicted annotations;
 */
int annotationsHandle::runEvaluation(int argc, char **argv){
	init();
	if(argc != 3){
		cerr<<"usage: cmmd <train_annotations.txt> <train_annotations.txt>\n"<< \
		"<train_annotations.txt> => file containing correct annotations\n"<< \
		"<test_annotations.txt>  => file containing predicted annotations\n"<<endl;
		exit(-1);
	}

	std::vector<annotationsHandle::FULL_ANNOTATIONS> allAnnoTrain;
	std::vector<annotationsHandle::FULL_ANNOTATIONS> allAnnoTest;
	loadAnnotations(argv[1],allAnnoTrain);
	loadAnnotations(argv[2],allAnnoTest);

	displayFullAnns(allAnnoTrain);

	double avgDist = 0, Ndiff = 0, ssdLatDiff = 0, ssdLongDiff = 0, poseDiff = 0;
	annoDifferences(allAnnoTrain, allAnnoTest, avgDist, Ndiff, ssdLongDiff, \
		ssdLatDiff, poseDiff);
}

char annotationsHandle::choice = ' ';
std::vector<std::string> annotationsHandle::poseNames;
bool annotationsHandle::withPoses = false;
unsigned annotationsHandle::poseSize = 5;
boost::mutex annotationsHandle::trackbarMutex;
IplImage *annotationsHandle::image;
std::vector<annotationsHandle::ANNOTATION> annotationsHandle::annotations;
//==============================================================================

int main(int argc, char **argv){
	annotationsHandle::runAnn(argc,argv,20,"_train",91);
	//annotationsHandle::runEvaluation(argc,argv);
}

