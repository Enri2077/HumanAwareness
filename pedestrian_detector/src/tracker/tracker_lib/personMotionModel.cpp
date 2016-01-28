#include "../include/tracker/personMotionModel.hpp"
#include <algorithm>
#include <vector>
#include "../include/HungarianFunctions.hpp"

/*
* Some motion models are implemented here in order to try to
* get better position estimates, based on the velocity and
* previous position of the person.
*
*
*/

Mat PersonModel::getBvtHistogram()
{
    return bvtHistogram;
}


void PersonModel::updateVelocityArray(Point3d detectedPosition)
{
    for(int i=0; i < 25; i++)
        velocity[25-i] = velocity[25-(i+1)];

    if(!deadReckoning)
    {
        if(positionHistory[1].x != -1000 && positionHistory[1].y != -1000)
        {
            velocity[0].x = -(detectedPosition.x - positionHistory[1].x)/delta_t;
            velocity[0].y = -(detectedPosition.y - positionHistory[1].y)/delta_t;
        }
        else
        {
            Point2d vel;
            vel = velocityMedianFilter();
            velocity[0].x = vel.x;
            velocity[0].y = vel.y;
        }

    }
    else
    {
        //Dead reckoning.
        velocity[0].x = velocity[1].x;
        velocity[0].y = velocity[1].y;
    }

    //ROS_ERROR_STREAM("Id:" << id << " | velx: " << velocity[0].x << " | vely: " << velocity[0].y);

    //This must be better thinked...

}


Point3d PersonModel::getPositionEstimate()
{
    //return filteredPosition+filteredVelocity*delta_t;

    if(method == MEDIANTRACKING)
    return medianFilter();
    else if(method == MMAETRACKING)
    {
      Point3d median = medianFilter(); //To get the Z for gaze... damn, this code is awfull. I'm dumb
      Point3d estimate(mmaeEstimator->xMMAE.at<double>(0, 0), mmaeEstimator->xMMAE.at<double>(3, 0), median.z);
//      ROS_ERROR_STREAM("STATE: " << mmaeEstimator->xMMAE);
      ROS_ERROR_STREAM("Returning estimate. Probs: " << mmaeEstimator->probabilities.at(0) << ", " << mmaeEstimator->probabilities.at(1) << ", " << mmaeEstimator->probabilities.at(2));

      return estimate;

    }
    return medianFilter();
}


void PersonModel::updateModel()
{

    //Maybe applying a median filter to the velocity gives good predictive results

    //updateVelocityArray(detectedPosition); - not used yet
    ros::Time now = ros::Time::now();
    ros::Duration sampleTime = now - lastUpdate;

    delta_t = sampleTime.toSec();
    lastUpdate = now;

    for(int i=0; i < median_window-1; i++)
        positionHistory[median_window-1-i] = positionHistory[median_window-1-(i+1)];

    positionHistory[0] = position;

 //   updateVelocityArray(position);

    position.x = -1000;
    position.y = -1000;
    position.z = 0.95;

    for(int i=0; i < 4; i++)
        rectHistory[4-i] = rectHistory[4-(i+1)];

    rectHistory[0] = rect;
    //velocity in m/ns

//    filteredVelocity = velocityMedianFilter();

    if(method == MMAETRACKING)
    {
        Mat measurement;
        if(positionHistory[0].x == -1000 || positionHistory[0].y == -1000)
        {
            measurement = Mat();
        }
        else
        {
            measurement = Mat(2, 1, CV_64F);
            measurement.at<double>(0, 0) = positionHistory[0].x;
            measurement.at<double>(1, 0) = positionHistory[0].y;
        }
            mmaeEstimator->correct(measurement);
    }


}

PersonModel::PersonModel(Point3d detectedPosition, cv::Rect_<int> bb, int id, int median_window, Mat bvtHistogram, int method)
{
    this->method = method;

    if(method == MEDIANTRACKING)
    {

        mmaeEstimator = NULL;

    }
    else if(method==MMAETRACKING)
    {
        static const double samplingRate = 15.0;
        static const double T = 1.0/samplingRate;

//Constant position filter

        KalmanFilter constantPosition(2, 2, 0, CV_64F);

        //Phi
        constantPosition.transitionMatrix = (Mat_<double>(2, 2) << 1, 0, 0, 1);
        //H
        constantPosition.measurementMatrix = (Mat_<double>(2, 2) << 1, 0, 0, 1);
        //Q
        constantPosition.processNoiseCov = (Mat_<double>(2, 2) << pow(T, 2), 0, 0, pow(T, 2));  //Q*sigma
        constantPosition.processNoiseCov  = constantPosition.processNoiseCov*70;
        //R
        constantPosition.measurementNoiseCov = (Mat_<double>(2, 2) << 1, 0, 0, 1);
        constantPosition.measurementNoiseCov = constantPosition.measurementNoiseCov*5; //R*sigma

        //P0
        constantPosition.errorCovPost = Mat::eye(2, 2, CV_64F)*50;

//Constant velocity filter

        KalmanFilter constantVelocity(4, 2, 0, CV_64F);

        //Phi
        constantVelocity.transitionMatrix = (Mat_<double>(4, 4) << 1, T, 0, 0, 0, 1, 0, 0, 0, 0, 1, T, 0, 0, 0, 1);
        //H
        constantVelocity.measurementMatrix = (Mat_<double>(2, 4) << 1, 0, 0, 0, 0, 0, 1, 0);
        //Q
        constantVelocity.processNoiseCov = (Mat_<double>(4, 4) << pow(T,4)/4, pow(T,3)/2, 0, 0, pow(T,3)/2, pow(T,2), 0, 0, 0, 0, pow(T,4)/4, pow(T,3)/2, 0, 0, pow(T,3)/2, pow(T,2));
        constantVelocity.processNoiseCov  = constantVelocity.processNoiseCov*150;  //Q*sigma
        //R
        constantVelocity.measurementNoiseCov = (Mat_<double>(2, 2) << 1, 0, 0, 1);
        constantVelocity.measurementNoiseCov = constantVelocity.measurementNoiseCov*5; //R*sigma

        //P0
        constantVelocity.errorCovPost = Mat::eye(4, 4, CV_64F)*50;

//Constant acceleration filter

        KalmanFilter constantAcceleration(6, 2, 0, CV_64F);

        //Phi
        constantAcceleration.transitionMatrix = (Mat_<double>(6, 6) << 1, T, pow(T, 2)/2, 0, 0, 0, 0, 1, T, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, T, pow(T, 2)/2, 0, 0, 0, 0, 1, T, 0, 0, 0, 0, 0, 1);
        //H
        constantAcceleration.measurementMatrix = (Mat_<double>(2, 6) << 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
        //Q
        constantAcceleration.processNoiseCov = (Mat_<double>(6, 6) << pow(T, 5)/20, pow(T, 4)/8, pow(T, 3)/6, 0, 0, 0, pow(T, 4)/8, pow(T, 3)/3, pow(T, 2)/2, 0, 0, 0, pow(T, 3)/6, pow(T, 2)/2, T, 0, 0, 0, 0, 0, 0, pow(T, 5)/20, pow(T, 4)/8, pow(T, 3)/3, 0, 0, 0, pow(T, 4)/8, pow(T, 3)/3, pow(T, 2)/2, 0, 0, 0, pow(T, 3)/6, pow(T, 2)/2, T);
        constantAcceleration.processNoiseCov  = constantAcceleration.processNoiseCov*2;  //Q*sigma
        //R
        constantAcceleration.measurementNoiseCov = (Mat_<double>(2, 2) << 1, 0, 0, 1);
        constantAcceleration.measurementNoiseCov = constantAcceleration.measurementNoiseCov*5.2; //R*sigma

        //P0
        constantAcceleration.errorCovPost = Mat::eye(6, 6, CV_64F)*50;

// Put them all in the bank
        std::vector<KalmanFilter> kalmanBank;
        kalmanBank.push_back(constantPosition);
        kalmanBank.push_back(constantVelocity);
        kalmanBank.push_back(constantAcceleration);

// Link each of these filters states to the ponderated state

        //State [i] from models corresponds to State indexes[i] in xMMAEx
        int indexes1[] = {0, 3};
        int indexes2[] = {0, 1, 3, 4};
        int indexes3[] = {0, 1, 2, 3, 4, 5};

        std::vector<int> indexes1_(indexes1, indexes1+sizeof(indexes1)/sizeof(int));
        std::vector<int> indexes2_(indexes2, indexes2+sizeof(indexes2)/sizeof(int));
        std::vector<int> indexes3_(indexes3, indexes3+sizeof(indexes3)/sizeof(int));

        std::vector<std::vector<int> > indexList;
        indexList.push_back(indexes1_);
        indexList.push_back(indexes2_);
        indexList.push_back(indexes3_);

//Guess the initial states - a good guess for the position is the measurement we just got!


        double states[] = {detectedPosition.x, 0, 0, detectedPosition.y, 0, 0};
//        double states[] = {0, 0, 0, 0, 0, 0};
        Mat initialStates = Mat(6, 1, CV_64F, states).clone();

//Build the Bank!
        mmaeEstimator = new MMAEFilterBank(kalmanBank, indexList, true, false, initialStates, CV_64F);
    }


    this->median_window = median_window;
    toBeDeleted = false;


    //  positionHistory = new Point2d[median_window];


    for(int i=0; i < 25; i++)
        velocity[i] = Point2d(0, 0);

    position = detectedPosition;

    this->bvtHistogram = bvtHistogram;


    rect = bb;
    rectHistory[0] = bb;

    positionHistory[0] = detectedPosition;


    for(int i=1; i < median_window; i++)
    {
        positionHistory[i].x =-1000;
        positionHistory[i].y = -1000;
        positionHistory[i].z = 0.95;
    }


    lockedOnce = false;

    this->id = id;
    noDetection = 0;

    delta_t = 0.1;
    lastUpdate = ros::Time::now();

    deadReckoning = false;
}

Point3d PersonModel::getNearestPoint(vector<cv::Point3d> coordsInBaseFrame, Point3d estimation)
{
    Point3d nearest(-1000, -1000, 0);
    Point3d estimation3d(estimation.x, estimation.y, 0);

    if(coordsInBaseFrame.size() == 0)
        return nearest;

    double distance=1000000;

    for(vector<cv::Point3d>::iterator it = coordsInBaseFrame.begin(); it != coordsInBaseFrame.end(); it++)
    {
        //It's dumb and slow to create matrix headers just to wrap 2 points...
        //I'm leaving this commented just to remember that!
        // double dist = norm(est3d, Mat((*it)), NORM_L2);

        double dist = norm(estimation3d-(*it));
        if(dist < distance && dist < 0.5)
        {
            distance = dist;
            nearest = (*it);
        }

    }
    return nearest;
}

Point3d PersonModel::medianFilter()
{
    //Optimize this later

    double x[median_window];
    double y[median_window];
    double z[median_window];

    for(int i = 0; i<median_window; i++)
    {
        x[i] = positionHistory[i].x;
        y[i] = positionHistory[i].y;
        z[i] = positionHistory[i].z;
    }

    vector<double> x_vect(x, x + sizeof(x)/sizeof(x[0]));
    vector<double> y_vect(y, y + sizeof(y)/sizeof(y[0]));
    vector<double> z_vect(z, z + sizeof(z)/sizeof(y[0]));

    std::sort(x_vect.begin(), x_vect.begin() + median_window);
    std::sort(y_vect.begin(), y_vect.begin() + median_window);
    std::sort(z_vect.begin(), z_vect.begin() + median_window);

    Point3d medianPoint(x_vect.at((int) (median_window/2)), y_vect.at((int) (median_window/2)), z_vect.at((int) (median_window/2)));

    return medianPoint;

}

Point2d PersonModel::velocityMedianFilter()
{
    //Optimize this later

    double x[25];
    double y[25];

    for(int i = 0; i<25; i++)
    {
        x[i] = velocity[i].x;
        y[i] = velocity[i].y;
    }

    vector<double> x_vect(x, x + sizeof(x)/sizeof(x[0]));
    vector<double> y_vect(y, y + sizeof(y)/sizeof(y[0]));

    std::sort(x_vect.begin(), x_vect.begin() + 5);
    std::sort(y_vect.begin(), y_vect.begin() + 5);


    Point2d medianPoint(x_vect.at(2), y_vect.at(2));

    return medianPoint;

}

PersonList::PersonList(int median_window, int numberOfFramesBeforeDestruction, int numberOfFramesBeforeDestructionLocked, double associatingDistance, int method)
{
    //This will never get reseted. That will make sure that we have a new id for every new detection
    nPersons = 0;
    this->numberOfFramesBeforeDestruction = numberOfFramesBeforeDestruction;
    this->numberOfFramesBeforeDestructionLocked = numberOfFramesBeforeDestructionLocked;
    this-> associatingDistance = associatingDistance;
    this->median_window = median_window;
    this->method = method;

}

PersonList::~PersonList()
{
    for(std::vector<PersonModel>::iterator it = personList.begin(); it!=personList.end(); it++)
    {
        if(it->mmaeEstimator != NULL)
        delete it->mmaeEstimator;
    }
}

void PersonList::updateList()
{
    //TODO

    for(vector<PersonModel>::iterator it = personList.begin(); it != personList.end();)
    {

        if(method == MMAETRACKING)
        {
            it->mmaeEstimator->predict();
        }

        if((*it).position.x != -1000 && (*it).position.y != -1000)
        {
            (*it).noDetection = 0;
            (*it).deadReckoning = false;
        }
        else
        {
            (*it).noDetection++;
            //Dead reckoning
            (*it).deadReckoning = true;
        }
        (*it).updateModel();

        if(((*it).noDetection > numberOfFramesBeforeDestruction && (*it).lockedOnce==false) || ((*it).noDetection > numberOfFramesBeforeDestructionLocked && (*it).lockedOnce==true))
        {
            it->toBeDeleted = true;
        }

        it++;
    }

}

void PersonList::addPerson(Point3d pos, cv::Rect_<int> rect, Mat bvtHistogram, int method)
{
    PersonModel person(pos, rect, nPersons, median_window, bvtHistogram, method);
    personList.push_back(person);
    nPersons++;

}

void PersonList::associateData(vector<Point3d> coordsInBaseFrame, vector<cv::Rect_<int> > rects, vector<Mat> colorFeaturesList)
{
    //Create distance matrix.
    //Rows represent the detections and columns represent the trackers


    int nTrackers = personList.size();
    int nDetections = coordsInBaseFrame.size();

    if(nTrackers > 0 && nDetections > 0)
    {

        double *distMatrixIn = (double*) malloc(sizeof(double)*nTrackers*nDetections);
        double *assignment = (double*) malloc(sizeof(double)*nDetections);
        double *cost = (double*) malloc(sizeof(double)*nDetections);

        //Fill the matrix
        /*  _           _
    *  |  l1c1 l1c2  |
    *  |  l2c1 l2c2  |
    *  |_ l3c1 l3c2 _|
    *
    *  [l1c1, l2c1, l3c1, l1c2,l2c2,l3c2]
    */

        int row = 0;

        //Each detection -> a row
        for(vector<Point3d>::iterator itrow = coordsInBaseFrame.begin(); itrow != coordsInBaseFrame.end(); itrow++, row++)
        {
            //Each tracker -> a column
            int col = 0;
            for(vector<PersonModel>::iterator itcolumn = personList.begin(); itcolumn != personList.end(); itcolumn++, col++)
            {

                //Calculate the distance

                Point3d trackerPos2d = (*itcolumn).getPositionEstimate();
                Mat trackerColorHist = (*itcolumn).getBvtHistogram();
                Mat detectionColorHist = colorFeaturesList.at(row);


                detectionColorHist.convertTo(detectionColorHist, CV_32F);
                trackerColorHist.convertTo(trackerColorHist, CV_32F);

                Point3d trackerPos;

                trackerPos.x = trackerPos2d.x;
                trackerPos.y = trackerPos2d.y;
                trackerPos.z = 0;

                Point3d detectionPos = (*itrow);
                detectionPos.z = 0;

                double dist = norm(trackerPos-detectionPos);

                double colorDist;

                colorDist = compareHist(detectionColorHist, trackerColorHist, CV_COMP_CORREL);
                if(dist < 3)
                {
                    if(colorDist < 0.3)
                        distMatrixIn[row+col*nDetections] = 1000; //Colors are too different. It cant be the same person...
                    else
                        distMatrixIn[row+col*nDetections] = (1+dist)*pow(1.1-colorDist, 3);
                }
                else
                {
                    double best = 1000;
                    for(int i = 1; i<median_window; i++)
                    {
                        Point3d hist((*itcolumn).positionHistory[i].x, (*itcolumn).positionHistory[i].y, 0);
                        double distHist = norm(hist-detectionPos);
                        if(distHist < 1.8)
                            if(distHist < best)
                                best = distHist;
                    }

                    distMatrixIn[row+col*nDetections] = (1+best)*pow(1.1-colorDist, 3);
                }
            }
        }


        assignmentoptimal(assignment, cost, distMatrixIn, nDetections, nTrackers);


        //assignment vector positions represents the detections and the value in each position represents the assigned tracker
        //if there is no possible association, then the value is -1
        //cost vector doesn't mean anything at this point...

        //for each detection we update it's tracker with the position

        for(int i=0; i<nDetections; i++)
        {

            //If there is an associated tracker and the distance is less than 2 meters we update the position
            if(assignment[i] != -1)
            {
                //
                if(distMatrixIn[i+((int)assignment[i])*nDetections] < 3)
                {
                    personList.at(assignment[i]).position.x = coordsInBaseFrame.at(i).x;
                    personList.at(assignment[i]).position.y = coordsInBaseFrame.at(i).y;
                    personList.at(assignment[i]).position.z = coordsInBaseFrame.at(i).z;

                    //Bounding boxes...
                    personList.at(assignment[i]).rect = rects.at(i);

                    //Color histograms
                    personList.at(assignment[i]).bvtHistogram = colorFeaturesList.at(i);
                }
            }
            else
            {
                //If there isn't, create a new tracker for each one - IF THERE IS NO OTHER TRACKER IN A 1.5m radius
                bool existsInRadius = false;

                Point3d coords = coordsInBaseFrame.at(i);
                coords.z = 0;

                for(vector<PersonModel>::iterator it = personList.begin(); it != personList.end(); it++)
                {
                    Point3d testPoint((*it).positionHistory[0].x, (*it).positionHistory[0].y, 0);

                    if(norm(coords-testPoint) < associatingDistance)
                    {
                        existsInRadius = true;
                        break;
                    }
                }
                if(!existsInRadius)
                {
                    addPerson(coordsInBaseFrame.at(i), rects.at(i), colorFeaturesList.at(i), MMAETRACKING);
                }
            }
        }


        //free the memory!

        free(distMatrixIn);
        free(assignment);
        free(cost);

    }
    else
    {
        //We create a tracker for each detection, and if there are none, we do nothing! - can't create 2 trackers in the same 1.5m radius
        for(int i=0; i<nDetections; i++)
        {
            bool existsInRadius = false;

            Point3d coords = coordsInBaseFrame.at(i);
            coords.z = 0;

            for(vector<PersonModel>::iterator it = personList.begin(); it != personList.end(); it++)
            {
                Point3d testPoint((*it).positionHistory[0].x, (*it).positionHistory[0].y, 0);
                if(norm(coords-testPoint) < associatingDistance)
                {
                    existsInRadius = true;
                    break;
                }
            }
            if(!existsInRadius)
            {
                addPerson(coordsInBaseFrame.at(i), rects.at(i), colorFeaturesList.at(i), MMAETRACKING);
            }
        }

    }
    //for each associated tracker we update the detection. For everyone else there is Mastercard. Just kidding... trackers
    //wich have no detections associated will be updated with a -1000, -1000 detection

    updateList();
}

std::vector<PersonModel> PersonList::getValidTrackerPosition()
{

    std::vector<PersonModel> validTrackers;

    for(std::vector<PersonModel>::iterator it = personList.begin(); it != personList.end(); it++)
    {
        if(method == MEDIANTRACKING)
        {
        Point3d med;
        med = (*it).medianFilter();

        if(med.x != -1000 and med.y != -1000)
            validTrackers.push_back(*it);
        }
        else
        {
            Point3d estimate = it->getPositionEstimate();
            if(estimate.x != -1000 && estimate.y != -1000)
            {
                validTrackers.push_back(*it);
            }
        }
    }

    return validTrackers;

}
