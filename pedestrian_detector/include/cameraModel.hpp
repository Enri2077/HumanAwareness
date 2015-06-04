#ifndef CAMERAMODEL_HPP
#define CAMERAMODEL_HPP

#include <opencv/cv.hpp>
#include <stack>
#include <string>

using namespace std;

class cameraModel
{
  private:
  cv::Mat K_;
  cv::Mat distCoefs_;
  cv::Mat projectionMat_;

  public:
  cv::Mat invertedK;

  cameraModel(string configFile);
  std::vector<cv::Point3d> calculatePointsOnBaseFrame(cv::Mat imagePoints, cv::Mat baseLinkToCamera);
  cv::Mat getK();
  cv::Mat getDistCoefs();
  cv::Mat getProjectionMat();



};

#endif // CAMERAMODEL_HPP
