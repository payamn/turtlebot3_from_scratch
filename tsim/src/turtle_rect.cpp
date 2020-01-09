#include "tsim/turtle_rect.h"
#define PI 3.14159265

// taken from https://magiccvs.byu.edu/wiki/#!ros_tutorials/c++_node_class.md

namespace turtle_rect
{

TurtleRect::TurtleRect() :
  nh_(ros::NodeHandle()),             /* This is an initialization list */
  nh_private_(ros::NodeHandle("~"))   /* The Node Handle can access the node and its attributes */
{
  // THIS IS THE CLASS CONSTRUCTOR //

  //***************** RETREIVE PARAMS ***************//
  nh_private_.param<float>("threshold", threshold_, 0.08);
  // This will pull the "threshold" parameter from the ROS server, and store it in the threshold_ variable.
  // If no value is specified on the ROS param server, then the default value of 0.0001 will be applied

  nh_private_.param<int>("x", x_, 3);
  nh_private_.param<int>("y", y_, 2);
  nh_private_.param<int>("width", width_, 4);
  nh_private_.param<int>("height", height_, 5);
  nh_private_.param<int>("trans_vel", trans_vel_, 2);
  nh_private_.param<int>("rot_vel", rot_vel_, 1);
  nh_private_.param<int>("frequency", frequency_, 100.0);

  // print parameters
  ROS_INFO("x: %d", x_);
  ROS_INFO("y: %d", y_);
  ROS_INFO("width: %d", width_);
  ROS_INFO("height: %d", height_);
  ROS_INFO("trans_vel: %d", trans_vel_);
  ROS_INFO("rot_vel: %d", rot_vel_);
  ROS_INFO("frequency: %d", frequency_);

  //***************** CUSTOM SERVER **************//
  traj_reset_server_ = nh_.advertiseService("traj_reset", &TurtleRect::traj_resetCallback, this);
  // trasj_reset service will teleport turtle to bottom left position and resume trajectory

  //***************** NODE HANDLES ***************//
  pose_subscriber_ = nh_.subscribe("turtle1/pose", 1, &TurtleRect::poseCallback, this);
  // This connects the poseCallback function with the reception of a Pose message on the "turtle1/pose" topic
  // ROS will essentially call the poseCallback function every time it receives a message on that topic.
  // 1 is the queue size.
  // 'this' is a class pointer.

  vel_publisher_ = nh_.advertise<geometry_msgs::Twist>("turtle1/cmd_vel", 1);
  // This connects a geometry_msgs::Twist message on the "turtle1/cmd_vel" topic. 1 is the queue size.

  pose_error_publisher_ = nh_.advertise<tsim::PoseError>("pose_error", 1);

  pen_client_ = nh_.serviceClient<turtlesim::SetPen>("turtle1/set_pen");

  traj_reset_client = nh_.serviceClient<std_srvs::Empty>("traj_reset");

  // setup pen parameters
  pen_srv_.request.r = 255;
  pen_srv_.request.g = 255;
  pen_srv_.request.b = 255;
  pen_srv_.request.width = 1;
  // off at first
  pen_srv_.request.off = 1;

  tele_client_ = nh_.serviceClient<turtlesim::TeleportAbsolute>("turtle1/teleport_absolute");

  // setup teleport paramters
  tele_srv_.request.x = x_;
  tele_srv_.request.y = y_;
  tele_srv_.request.theta = 0;

  // Remove pen, teleport, and re-add pen. Make sure services are available first.
  // http://docs.ros.org/electric/api/roscpp/html/namespaceros_1_1service.html
  ros::service::waitForService("turtle1/set_pen", -1);
  ros::service::waitForService("turtle1/teleport_absolute", -1);
  ros::service::waitForService("traj_reset", -1);
  
  // turn pen off
  pen_client_.call(pen_srv_);
  // teleport
  tele_client_.call(tele_srv_);
  // turn pen on
  pen_srv_.request.off = 0;
  pen_client_.call(pen_srv_);

  // sleep
  // ros::Duration(1).sleep();

  // predicted turtle pose
  x_o_ = 3;
  y_o_ = 2;
  head_o_ = 0;

}

bool TurtleRect::traj_resetCallback(std_srvs::Empty::Request&, std_srvs::Empty::Response&)
{
  // turn pen off
  pen_srv_.request.off = 1;
  pen_client_.call(pen_srv_);
  // teleport
  tele_client_.call(tele_srv_);
  // turn pen on
  pen_srv_.request.off = 0;
  pen_client_.call(pen_srv_);

  // Reset state machine
  count_vertex_ = 1;
  done_flag_ = false;
  lin_ang_flag_ = true;

  ROS_INFO("TURTLE RESET");

  ros::Duration(0.5).sleep();
}

void TurtleRect::poseCallback(const turtlesim::PoseConstPtr &msg)
// This function runs every time we get a turtlesim::Pose message on the "turtle1/pose" topic.
// We generally use the const <message>ConstPtr &msg syntax to prevent our node from accidentally
// changing the message, in the case that another node is also listening to it.
{
  ROS_DEBUG("READING POSE");
  x_pos_ = msg->x;
  y_pos_ = msg->y;
  head_ = msg->theta;

  // ROS_INFO("HEADING: %f", head_);
}

void TurtleRect::move(const float &goal_x, const float &goal_y, const float &goal_head)
{
  ROS_DEBUG("MOVE LOOP");

  switch(lin_ang_flag_)
  {
    case true:
      ROS_DEBUG("CHECK ANG");
      // Check if need to move lin or ang
      if (abs(goal_head - head_) <= threshold_ / 3.0)
      {
        lin_ang_flag_ = false;
        ROS_DEBUG("VAL: %f", goal_head - head_ - threshold_);
        ROS_DEBUG("THRESH: %f", threshold_ / 3.0);
      } else {
        ROS_DEBUG("ANG");
        ROS_DEBUG("head: %f \t goal_head: %f", head_, goal_head);
        // Move Angularly
        twist_.linear.x = 0;
        twist_.linear.y = 0;
        twist_.linear.z = 0;
        twist_.angular.x = 0;
        twist_.angular.y = 0;
        twist_.angular.z = rot_vel_;

        // Predict turtle pos assuming perfect commands
        // and publish to "pose_error" topic as PoseError msg
        this->predict();

        // Publish
        vel_publisher_.publish(twist_);
        // sleep to make sure pose syncs
        // ros::Duration(0.5).sleep();
      }

      break;

    case false:
      ROS_DEBUG("CHECK LIN");
      // Check if need to move lin or ang
      if (abs(goal_y - y_pos_) <= threshold_ \
          && abs(goal_x - x_pos_) <= threshold_)
      {
        lin_ang_flag_ = true;
      } else {
        ROS_DEBUG("LIN");
        ROS_INFO("test y: %d", abs(goal_y - y_pos_) <= threshold_);
        ROS_INFO("test x: %d", abs(goal_x - x_pos_) <= threshold_);
        ROS_INFO("TEST y: %f", abs(goal_y - y_pos_));
        ROS_INFO("TEST x: %f", abs(goal_x - x_pos_));
        ROS_INFO("THRESH: %f", abs(threshold_));

        ROS_DEBUG("x: %f \t goal_x: %f", x_pos_, goal_x);
        ROS_DEBUG("y: %f \t goal_y: %f", y_pos_, goal_y);
        // Move Linearly
        twist_.linear.x = trans_vel_;
        twist_.linear.y = 0;
        twist_.linear.z = 0;
        twist_.angular.x = 0;
        twist_.angular.y = 0;
        twist_.angular.z = 0;

        // Predict turtle pos assuming perfect commands
        // and publish to "pose_error" topic as PoseError msg
        this->predict();

        // Publish
        vel_publisher_.publish(twist_);
        // sleep to make sure pose syncs
        // ros::Duration(0.5).sleep();
      }

      break;
  }

}

void TurtleRect::predict()
{
  head_o_ += twist_.angular.z * 1 / frequency_;
  x_o_ += twist_.linear.x * cos(head_o_) * 1 / frequency_;
  y_o_ += twist_.linear.x * sin(head_o_) * 1 / frequency_;

  // wrap to 0-2pi
  theta_error_ = abs(head_o_ - head_) - 2 * PI * floor(abs(head_o_ - head_) / (2 * PI));
  if (theta_error_ > 6.0)
  {
    theta_error_ -= 6.0;
  }
  x_error_ = abs(x_o_ - x_pos_);
  y_error_ = abs(y_o_ - y_pos_);

  pose_error_.x_error = x_error_;
  pose_error_.y_error = y_error_;
  pose_error_.theta_error = theta_error_;

  pose_error_publisher_.publish(pose_error_);
}

void TurtleRect::control()
{
  while (ros::ok())
  {
  
  ros::Rate rate(frequency_);

  ROS_DEBUG("CONTROL LOOP");

  // decalre goal position variables
  float goal_x = 0;
  float goal_y = 0;
  float goal_head = 0;

  switch(count_vertex_)
  {

    case 0:
      // vertex 1
      ROS_DEBUG("CASE0");
      goal_head = - PI / 2;
      goal_x = x_;
      goal_y = y_;

      // function to move (pass in goals), move fcn decides if lin or ang
      // updates vertex done flag
      // 'this' is a pointer to the TurtleRect class
      this->move(goal_x, goal_y, goal_head);

      if (abs(goal_x - x_pos_) <= threshold_ \
        && abs(goal_y - y_pos_) <= threshold_ \
        && abs(goal_head - head_) <= threshold_ / 3.0)
      {
        done_flag_ = true;
      }

      if (done_flag_)
      {
        count_vertex_ = 1;
        done_flag_ = false;
        lin_ang_flag_ = true;
      }
      break;

    case 1:
      // vertex 2
    ROS_DEBUG("CASE1");
      goal_head = 0;
      goal_x = x_ + width_;
      goal_y = y_;

      this->move(goal_x, goal_y, goal_head);

      if (abs(goal_x - x_pos_) <= threshold_ \
        && abs(goal_y - y_pos_) <= threshold_ \
        && abs(goal_head - head_) <= threshold_ / 3.0)
      {
        done_flag_ = true;
      }

      if (done_flag_)
      {
        count_vertex_ = 2;
        done_flag_ = false;
        lin_ang_flag_ = true;
      }
      break;

    case 2:
      // vertex 3
    ROS_DEBUG("CASE2");
      goal_head = PI / 2.0;
      goal_x = x_ + width_;
      goal_y = y_ + height_;

      this->move(goal_x, goal_y, goal_head);

      if (abs(goal_x - x_pos_) <= threshold_ \
        && abs(goal_y - y_pos_) <= threshold_ \
        && abs(goal_head - head_) <= threshold_ / 3.0)
      {
        done_flag_ = true;
      }

      if (done_flag_)
      {
        count_vertex_ = 3;
        done_flag_ = false;
        lin_ang_flag_ = true;
      }
      break;

    case 3:
      // vertex 4
    ROS_DEBUG("CASE3");
      goal_head = PI;
      goal_x = x_;
      goal_y = y_ + height_;

      this->move(goal_x, goal_y, goal_head);

      if (abs(goal_x - x_pos_) <= threshold_ \
        && abs(goal_y - y_pos_) <= threshold_ \
        && abs(goal_head - head_) <= threshold_ / 3.0)
      {
        done_flag_ = true;
      }

      if (done_flag_)
      {
        count_vertex_ = 0;
        done_flag_ = false;
        lin_ang_flag_ = true;
      }
      break;

    default:
      // reset count_vertex to zero by default
      count_vertex_ = 0;

  }

  ros::spinOnce();
  rate.sleep();

}
}

} // namespace turtle_rect