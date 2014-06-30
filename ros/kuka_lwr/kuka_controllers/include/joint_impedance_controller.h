#ifndef KUKA_CONTROLLERS__JOINT_IMPEDANCE_CONTROLLER_H
#define KUKA_CONTROLLERS__JOINT_IMPEDANCE_CONTROLLER_H

#include <ros/ros.h>
#include <tf/transform_datatypes.h>
//#include <ros/node_handle.h>
#include <hardware_interface/joint_command_interface.h>
#include <realtime_tools/realtime_publisher.h>
#include <controller_interface/controller.h>
#include <std_msgs/Float64.h>
#include <std_msgs/Float64MultiArray.h>

// #include <Eigen/Core>
// #include <Eigen/Geometry>
// #include <eigen_conversions/eigen_kdl.h>
// #include <eigen_conversions/eigen_msg.h>

#include <kdl/chain.hpp>
#include <kdl/chainfksolver.hpp>
#include <kdl/frames.hpp>
#include <kdl/chainjnttojacsolver.hpp>
#include <kdl/chainidsolver_recursive_newton_euler.hpp> //to be tested
#include <kdl/chaindynparam.hpp> //this to compute the gravity verctor

#include <vector>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/condition.hpp>


/* Control mode 30 in kuka robot

 tau_cmd = K*(q_des - q_msr) + D*dotq_msr + tau_des + f(q,dotq,ddotd)

*/

namespace kuka_controllers
{

	class JointImpedanceController: public controller_interface::Controller<hardware_interface::EffortJointInterface>
	{
	public:
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
	    JointImpedanceController();
	    ~JointImpedanceController();

	    bool init(hardware_interface::EffortJointInterface *robot, ros::NodeHandle &n);
	    void starting(const ros::Time& time);
	    void update(const ros::Time& time, const ros::Duration& period);
	    void commandConfiguration(const std_msgs::Float64MultiArray::ConstPtr &msg);
	    void setGains(const std_msgs::Float64MultiArray::ConstPtr &msg);
	private:

	enum { Joints = 7 };
	ros::NodeHandle nh_;
	ros::Subscriber sub_gains_;
	ros::Subscriber sub_posture_;

	KDL::Chain kdl_chain_;
	KDL::JntArrayVel dotq_msr_;
	KDL::JntArray q_msr_, q_des_;
	KDL::JntArray tau_des_, tau_cmd_, tau_gravity_;
	KDL::JntArray K_, D_;

	boost::scoped_ptr<KDL::ChainIdSolver_RNE> id_solver_;
	boost::scoped_ptr<KDL::ChainDynParam> id_solver_gravity_;

	std::vector<hardware_interface::JointHandle> joint_handles_;

	};

}; //namespace

#endif
