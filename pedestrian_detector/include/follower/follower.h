#ifndef PEDEST0RIAN_DETECTOR_FOLLOWER_H
#define PEDESTRIAN_DETECTOR_FOLLOWER_H

#include <string>
#include <vector>

#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>

// Messages
#include <std_msgs/String.h>
#include <std_msgs/UInt8MultiArray.h>
#include <nav_msgs/Path.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/PointStamped.h>
#include <geometry_msgs/Twist.h>

// Services
#include <std_srvs/Empty.h>
#include <nav_msgs/GetPlan.h>

// Actions
#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <scout_msgs/NavigationByTargetAction.h>


class Follower
{
 public:
  Follower(); //constructor
  ~Follower(); //distructor
  
  void setup();
  void loop();

  void eventInCallback(const std_msgs::String& msg);
  void filteredPersonPositionCallback(const geometry_msgs::PointStampedConstPtr& msg);
  void robotPoseCallBack(const geometry_msgs::PoseWithCovarianceStampedConstPtr& msg);

  void doneCb(const actionlib::SimpleClientGoalState& state, const scout_msgs::NavigationByTargetResultConstPtr& result);
  void activeCb();
  void feedbackCb(const scout_msgs::NavigationByTargetFeedbackConstPtr& feedback);



  void initialiseNavigationGoal();
  void updateNavigationGoal();
  void updateHeadPosition();

  void getParams();
  void clearCostmaps();

  
 private:
  ros::NodeHandle nh_;

  // Publishers
  ros::Publisher pub_pose_;
  ros::Publisher pub_pose_in_;
  ros::Publisher pub_head_rot_;
  ros::Publisher trajectory_publisher_;
  ros::Publisher residual_trajectory_publisher_;
  ros::Publisher current_target_pose_publisher_;
  ros::Publisher next_target_pose_publisher_;

  // Subscribers
  ros::Subscriber event_in_subscriber_;
  ros::Subscriber filtered_person_position_subscriber_;
  ros::Subscriber robot_position_subscriber_;

  // Service Clients
  ros::ServiceClient plan_client_;
  ros::ServiceClient clear_client_;
  
  // Action Clients
  actionlib::SimpleActionClient<scout_msgs::NavigationByTargetAction> nav_action_client_;//("/navigation_by_target", true);



  // Main following state
  bool following_enabled_; // TODO enable/disable with event topics
  bool initialise_navigation_; // TODO enable/disable with event topics


  // Configuration

  // Type of navigation. Set by default through parameters. Can be updated dinamically to implement different navigation stategies.
  // TODO change dinamically (state machine?)
  std::string navigation_type_, navigation_stack_;
  double path_minimum_distance_;
  double target_pose_minimum_distance_;
  double person_pose_minimum_distance_;

  // flag used to know when we have received a callback
  bool new_person_position_received_;

  // Position of the person from the Bayesian filter, and of the person in base_link.
  geometry_msgs::PointStamped filtered_person_position_;
  geometry_msgs::PointStamped relative_person_position_;
  
  // Person's path, from the beginning untill the last received person's filtered position
  std::vector<geometry_msgs::PointStamped> complete_person_path_;
  nav_msgs::Path complete_person_trajectory_;
  geometry_msgs::PoseStamped  current_target_pose_;

  // The residual trajectory is always the part of the trajectory from the first pose that may be set as target, to the last pose of the complete trajectory.
  // Once a pose of the residual trajectory is set as target, the poses precedent to that one should not be part of the residual anymore.
  nav_msgs::Path residual_trajectory_;

  // Position in the person's path used as target for navigation
  long unsigned int current_pose_pointer_;
  
  // for storing the arguments that will be read from param server
  std::vector<std::string> script_arguments_;

  // out topics
  //std_msgs::String stop_;
  //std_msgs::String current_robot_position_;

  // position of the head
  double head_pos; 

  tf::TransformListener* listener_;

  bool isthereaPath(geometry_msgs::PoseStamped goal);
  void broadcastPoseToTF(geometry_msgs::PoseStamped p, std::string target_frame);

  //std::vector<geometry_msgs::PoseStamped> personPoses_;

  geometry_msgs::PoseStamped keptPose_;

  geometry_msgs::Point keptPoint_;
  
  geometry_msgs::PoseStamped current_robot_position_;

  geometry_msgs::PoseStamped goal_;
  geometry_msgs::Point pointGoal_;
  
  bool path_;

  //TF stuff
  static tf::TransformBroadcaster br_;
  tf::Transform transform;
  tf::Quaternion q;
  
};

#endif // PEDESTRIAN_DETECTOR_FOLLOWER_H
