/* classifyImages.h
 * Author: Silvia-Laura Pintea
 */
#ifndef CLASSIFYIMAGES_H_
#define CLASSIFYIMAGES_H_
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <exception>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv/ml.h>
#include "eigenbackground/src/Tracker.hh"
#include "eigenbackground/src/Helpers.hh"
#include "featureDetector.h"
#include "gaussianProcess.h"
#include "annotationsHandle.h"

/** Class used for classifying the training data.
 */
class classifyImages {
	protected:
		/** @var features
		 * An instance of \c featureDetector class.
		 */
		featureDetector *features;

		/** @var trainData
		 * The training data matrix.
		 */
		cv::Mat trainData;

		/** @var testData
		 * The test data matrix.
		 */
		cv::Mat testData;

		/** @var trainFolder
		 * The folder containing the training images.
		 */
		std::string trainFolder;

		/** @var testFolder
		 * The folder containing the test images.
		 */
		std::string testFolder;

		/** @var annotationsTrain
		 * The file contains the annotations for the training images.
		 */
		std::string annotationsTrain;

		/** @var annotationsTest
		 * The file contains the annotations for the test images.
		 */
		std::string annotationsTest;

		/** @var trainTargets
		 * The column matrix containing the train annotation data (targets).
		 */
		cv::Mat trainTargets;

		/** @var testTargets
		 * The column matrix containing the test annotation data (targets).
		 */
		cv::Mat testTargets;

		/** @var gaussianProcess
		 * An instance of the class gaussianProcess.
		 */
		gaussianProcess gp;
		//======================================================================
	public:
		classifyImages(int argc, char **argv);
		virtual ~classifyImages();

		/** Creates the training data (according to the options), the labels and
		 * trains the a \c GaussianProcess on the data.
		 */
		void trainGP(std::vector<std::string> options = std::vector<std::string>(0));

		/** Creates the test data and applies \c GaussianProcess prediction on the test
		 * data.
		 */
		void predictGP(std::vector<std::string> options = std::vector<std::string>(0));
};

#endif /* CLASSIFYIMAGES_H_ */
