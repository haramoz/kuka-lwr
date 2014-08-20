#include "one_task_inverse_kinematics.h"
#include <pluginlib/class_list_macros.h>
#include <kdl_parser/kdl_parser.hpp>
#include <math.h>
#include <Eigen/LU>
#include <misc/pseudo_inversion.h>
#include <misc/skew_symmetric.h>

namespace kuka_controllers 
{
	OneTaskInverseKinematics::OneTaskInverseKinematics() {}
	OneTaskInverseKinematics::~OneTaskInverseKinematics() {}

	bool OneTaskInverseKinematics::init(hardware_interface::EffortJointInterface *robot, ros::NodeHandle &n)
	{
		nh_ = n;

		// get URDF and name of root and tip from the parameter server
		std::string robot_description, root_name, tip_name;

		if (!ros::param::search(n.getNamespace(),"robot_description", robot_description))
		{
		    ROS_ERROR_STREAM("OneTaskInverseKinematics: No robot description (URDF) found on parameter server ("<<n.getNamespace()<<"/robot_description)");
		    return false;
		}
		if (!nh_.getParam("root_name", root_name))
		{
		    ROS_ERROR_STREAM("OneTaskInverseKinematics: No root name found on parameter server ("<<n.getNamespace()<<"/root_name)");
		    return false;
		}
		if (!nh_.getParam("tip_name", tip_name))
		{
		    ROS_ERROR_STREAM("OneTaskInverseKinematics: No tip name found on parameter server ("<<n.getNamespace()<<"/tip_name)");
		    return false;
		}
	 
		// Get the gravity vector (direction and magnitude)
		KDL::Vector gravity_ = KDL::Vector::Zero();
		gravity_(2) = 9.81;

		// Construct an URDF model from the xml string
		std::string xml_string;

		if (n.hasParam(robot_description))
			n.getParam(robot_description.c_str(), xml_string);
		else
		{
		    ROS_ERROR("Parameter %s not set, shutting down node...", robot_description.c_str());
		    n.shutdown();
		    return false;
		}

		if (xml_string.size() == 0)
		{
			ROS_ERROR("Unable to load robot model from parameter %s",robot_description.c_str());
		    n.shutdown();
		    return false;
		}

		ROS_DEBUG("%s content\n%s", robot_description.c_str(), xml_string.c_str());
		
		// Get urdf model out of robot_description
		urdf::Model model;
		if (!model.initString(xml_string))
		{
		    ROS_ERROR("Failed to parse urdf file");
		    n.shutdown();
		    return false;
		}
		ROS_INFO("Successfully parsed urdf file");
		
		KDL::Tree kdl_tree_;
		if (!kdl_parser::treeFromUrdfModel(model, kdl_tree_))
		{
		    ROS_ERROR("Failed to construct kdl tree");
		    n.shutdown();
		    return false;
		}


		// Populate the KDL chain
		if(!kdl_tree_.getChain(root_name, tip_name, kdl_chain_))
		{
		    ROS_ERROR_STREAM("Failed to get KDL chain from tree: ");
		    ROS_ERROR_STREAM("  "<<root_name<<" --> "<<tip_name);
		    ROS_ERROR_STREAM("  Tree has "<<kdl_tree_.getNrOfJoints()<<" joints");
		    ROS_ERROR_STREAM("  Tree has "<<kdl_tree_.getNrOfSegments()<<" segments");
		    ROS_ERROR_STREAM("  The segments are:");

		    KDL::SegmentMap segment_map = kdl_tree_.getSegments();
		    KDL::SegmentMap::iterator it;

		    for( it=segment_map.begin(); it != segment_map.end(); it++ )
		      ROS_ERROR_STREAM( "    "<<(*it).first);

		  	return false;
		}


		ROS_DEBUG("Number of segments: %d", kdl_chain_.getNrOfSegments());
		ROS_DEBUG("Number of joints in chain: %d", kdl_chain_.getNrOfJoints());



		// Get joint handles for all of the joints in the chain
		for(std::vector<KDL::Segment>::const_iterator it = kdl_chain_.segments.begin()+1; it != kdl_chain_.segments.end(); ++it)
		{
		    joint_handles_.push_back(robot->getHandle(it->getJoint().getName()));
		    ROS_DEBUG("%s", it->getJoint().getName().c_str() );
		}

		ROS_DEBUG(" Number of joints in handle = %lu", joint_handles_.size() );

		jnt_to_jac_solver_.reset(new KDL::ChainJntToJacSolver(kdl_chain_));
		id_solver_.reset(new KDL::ChainDynParam(kdl_chain_,gravity_));
		fk_pos_solver_.reset(new KDL::ChainFkSolverPos_recursive(kdl_chain_));
		ik_vel_solver_.reset(new KDL::ChainIkSolverVel_pinv(kdl_chain_));
		ik_pos_solver_.reset(new KDL::ChainIkSolverPos_NR(kdl_chain_,*fk_pos_solver_,*ik_vel_solver_));

		joint_msr_states_.resize(kdl_chain_.getNrOfJoints());
		joint_des_states_.resize(kdl_chain_.getNrOfJoints());
		tau_cmd_.resize(kdl_chain_.getNrOfJoints());
		J_.resize(kdl_chain_.getNrOfJoints());
		PIDs_.resize(kdl_chain_.getNrOfJoints());

		sub_command_ = nh_.subscribe("command_configuration", 1, &OneTaskInverseKinematics::command_configuration, this);
		sub_gains_ = nh_.subscribe("set_gains", 1, &OneTaskInverseKinematics::set_gains, this);

		return true;
	}

	void OneTaskInverseKinematics::starting(const ros::Time& time)
	{
		// get joint positions
  		for(int i=0; i < joint_handles_.size(); i++) 
  		{
    		joint_msr_states_.q(i) = joint_handles_[i].getPosition();
    		joint_msr_states_.qdot(i) = joint_handles_[i].getVelocity();
    		joint_des_states_.q(i) = joint_msr_states_.q(i);
    	}

    	Kp = 60;
    	Ki = 1.2;
    	Kd = 10;

    	for (int i = 0; i < PIDs_.size(); i++)
    		PIDs_[i].initPid(Kp,Ki,Kd,0.3,-0.3);
    	ROS_INFO("PIDs gains are: Kp = %f, Ki = %f, Kd = %f",Kp,Ki,Kd);

    	//I_ = Eigen::Matrix<double,7,7>::Identity(7,7);
    	//P_ = Eigen::Matrix<double,7,7>::Zero();

    	// computing forward kinematics
    	fk_pos_solver_->JntToCart(joint_msr_states_.q,x_);

    	//Desired posture 
    	x_des_ = x_;
<<<<<<< HEAD

=======
>>>>>>> 4fbc9ecf0cc2fa0615797e227df6e5485546a834
    	cmd_flag_ = 0;
	}

	void OneTaskInverseKinematics::update(const ros::Time& time, const ros::Duration& period)
	{

		// get joint positions
  		for(int i=0; i < joint_handles_.size(); i++) 
  		{
    		joint_msr_states_.q(i) = joint_handles_[i].getPosition();
    		joint_msr_states_.qdot(i) = joint_handles_[i].getVelocity();
    	}

    	if (cmd_flag_)
    	{
	    	// computing Jacobian
	    	jnt_to_jac_solver_->JntToJac(joint_msr_states_.q,J_);

	    	// computing J_pinv_
	    	pseudo_inverse(J_,J_pinv_);

	    	// computing forward kinematics
	    	fk_pos_solver_->JntToCart(joint_msr_states_.q,x_);

<<<<<<< HEAD
	    	// end-effector position/orientation error
	    	x_err_.vel = x_des_.p - x_.p;
	    	//x_err_.rot = 0.5*(x_des_.M.UnitX()*x_.M.UnitX() + x_des_.M.UnitY()*x_.M.UnitY() + x_des_.M.UnitZ()*x_.M.UnitZ());

	    	// getting quaternion from rotation matrix
	    	x_.M.GetQuaternion(quat_curr_.v(0),quat_curr_.v(1),quat_curr_.v(2),quat_curr_.a);
	    	x_des_.M.GetQuaternion(quat_des_.v(0),quat_des_.v(1),quat_des_.v(2),quat_des_.a);

	    	skew_symmetric(quat_des_.v,skew_);

	    	for (int i = 0; i < skew_.rows(); i++)
	    	{
	    		v_temp_(i) = 0.0;
	    		for (int k = 0; k < skew_.cols(); k++)
	    			v_temp_(i) += skew_(i,k)*(quat_curr_.v(k));
	    	}

	    	x_err_.rot = quat_curr_.a*quat_des_.v - quat_des_.a*quat_curr_.v - v_temp_; 

	    	//for (int i = 0; i < 3; i++)
	    	//	x_err_.rot(i) = 0.0;	// for now, doesn't count orientation error

	    	// computing q_dot
	    	for (int i = 0; i < J_pinv_.rows(); i++)
	    	{
	    		joint_des_states_.qdot(i) = 0.0;
	    		for (int k = 0; k < J_pinv_.cols(); k++)
	    			joint_des_states_.qdot(i) += J_pinv_(i,k)*x_err_(k);
	    	}

	    	// integrating q_dot -> getting q (Euler method)
	    	for (int i = 0; i < joint_handles_.size(); i++)
	    		joint_des_states_.q(i) += period.toSec()*joint_des_states_.qdot(i);

	    	if (Equal(x_,x_des_,0.005))
	    	{
	    		ROS_INFO("On target");
	    		cmd_flag_ = 0;
	    	}
	    }
=======
	    	// end-effector displacement 
	    	x_err_.vel = x_des_.p - x_.p;

	    	for (int i = 0; i < 3; i++)
	    		x_err_.rot(i) = 0.0;	// for now, doesn't count orientation error

	    	// computing q_dot
	    	for (int i = 0; i < J_pinv_.rows(); i++)
	    	{
	    		joint_des_states_.qdot(i) = 0.0;
	    		for (int k = 0; k < J_pinv_.cols(); k++)
	    			joint_des_states_.qdot(i) += J_pinv_(i,k)*(x_err_(k));
	    	}

	    	// integrating q_dot -> getting q (Euler method)
	    	for (int i = 0; i < joint_handles_.size(); i++)
	    		joint_des_states_.q(i) += period.toSec()*joint_des_states_.qdot(i);
>>>>>>> 4fbc9ecf0cc2fa0615797e227df6e5485546a834

	    	if (x_des_.p == x_.p)
	    		cmd_flag_ = 0;
	}
	    
    	// set controls for joints
    	for (int i = 0; i < joint_handles_.size(); i++)
    	{
    		tau_cmd_(i) = PIDs_[i].computeCommand(joint_des_states_.q(i) - joint_msr_states_.q(i),period);
		   	joint_handles_[i].setCommand(tau_cmd_(i));
    	}
	}

	void OneTaskInverseKinematics::command_configuration(const kuka_controllers::PoseRPY::ConstPtr &msg)
	{	
		// TODO: read orientation message. (now reads only position)
		KDL::Frame frame_des_;

		switch(msg->id)
		{
			case 0:
			frame_des_ = KDL::Frame(
					KDL::Rotation::RPY(msg->orientation.roll,
						 			  msg->orientation.pitch,
								 	  msg->orientation.yaw),
					KDL::Vector(msg->position.x,
								msg->position.y,
								msg->position.z));
			break;
	
			case 1: // position only
			frame_des_ = KDL::Frame(
				KDL::Vector(msg->position.x,
							msg->position.y,
							msg->position.z));
			break;
		
			case 2: // orientation only
			frame_des_ = KDL::Frame(
				KDL::Rotation::RPY(msg->orientation.roll,
				   	 			   msg->orientation.pitch,
								   msg->orientation.yaw));
			break;

			default:
			ROS_INFO("Wrong message ID");
			return;
		}
		
		x_des_ = frame_des_;
		cmd_flag_ = 1;
	}

	void OneTaskInverseKinematics::set_gains(const std_msgs::Float64MultiArray::ConstPtr &msg)
	{
		if(msg->data.size() == 2)
		{
			for(int i = 0; i < PIDs_.size(); i++)
				PIDs_[i].setGains(msg->data[0],msg->data[1],0.0,0.0,0.0);
			ROS_INFO("New gains set: Kp = %f, Kd = %f",msg->data[0],msg->data[1]);
		}
		else
			ROS_INFO("PIDs gains needed are 2 (Kp and Kd)");
	}
}

PLUGINLIB_EXPORT_CLASS(kuka_controllers::OneTaskInverseKinematics, controller_interface::ControllerBase)