#ifndef PEDEST0RIAN_DETECTOR_FOLLOWER_H
#define PEDESTRIAN_DETECTOR_FOLLOWER_H

#include <ros/ros.h>
#include <string>
#include <vector>
#include <std_msgs/String.h>
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/PoseWithCovarianceStamped.h"
#include "geometry_msgs/PointStamped.h"
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>
#include <geometry_msgs/Twist.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include "std_msgs/UInt8MultiArray.h"
#include "nav_msgs/GetPlan.h"
#include "std_srvs/Empty.h"
#include <tf/transform_broadcaster.h>


class Follower
{
 public:
  Follower(); //constructor
  ~Follower(); //distructor

  // get parameters from param server
  void getParams();

  // callback for event_in recieved msg;
  void eventInCallBack(const geometry_msgs::PointStampedConstPtr& msg);

  // callback for event_in recieved msg;
  void robotPoseCallBack(const geometry_msgs::PoseWithCovarianceStampedConstPtr& msg);
  
  // ros node main loop
  void update();

  void update_head();

  void clearCostmaps();
  
  // rotate head towards person
  void rotateHead(geometry_msgs::Point p);

  // flag used to know when we have received a callback
  bool is_event_in_received_;  
  
 private:  
  // ros related variables
  ros::NodeHandle nh_;
  ros::Publisher pub_pose_;
  //ros::Publisher pub_event_in_;
  ros::Publisher pub_pose_in_;
  ros::Publisher pub_head_rot_;
  ros::Subscriber sub_event_in_;
  ros::Subscriber sub_robot_pose_;
  ros::ServiceClient plan_client_;
  ros::ServiceClient clear_client_;
  
 

  // stores the msg in event_in callback
  geometry_msgs::PointStamped event_in_msg_;
  
  // stores que msg that will be published on event_out topic
  geometry_msgs::PoseStamped event_out_msg_;
  
  // for storing the arguments that will be read from param server
  std::vector<std::string> script_arguments_;

  // out topics
  //std_msgs::String stop_;
  //std_msgs::String start_;

  // position of the head
  double head_pos; 

  tf::TransformListener* listener_;

  bool isthereaPath(geometry_msgs::PoseStamped goal);
  
  //std::vector<geometry_msgs::PoseStamped> personPoses_;

  geometry_msgs::PoseStamped keptPose_;

  geometry_msgs::Point keptPoint_;
  
  geometry_msgs::PoseStamped start_;

  geometry_msgs::PoseStamped goal_;
  geometry_msgs::Point pointGoal_;
  
  bool path_;

  //TF stuff
  static tf::TransformBroadcaster br_;
  tf::Transform transform;
  tf::Quaternion q;
  
};

#endif // PEDESTRIAN_DETECTOR_FOLLOWER_H
