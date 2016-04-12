#include "gps_agent_pkg/baxter_control/baxterplugin.h"
#include "gps_agent_pkg/positioncontroller.h"
#include "gps_agent_pkg/trialcontroller.h"
#include "gps_agent_pkg/encodersensor.h"
#include "gps_agent_pkg/util.h"

namespace gps_control {

// Plugin constructor.
GPSBaxterPlugin::GPSBaxterPlugin()
{
    // Some basic variable initialization.
    controller_counter_ = 0;
    controller_step_length_ = 50;
}

// Destructor.
GPSBaxterPlugin::~GPSBaxterPlugin()
{
    // Nothing to do here, since all instance variables are destructed automatically.
}

// Initialize the object and store the robot state.
bool GPSBaxterPlugin::init(hardware_interface::EffortJointInterface *robot, ros::NodeHandle& n)
{
    ROS_INFO_STREAM_NAMED("hardware_interface","The GPSBaxterPlugin is initialing");
    // Variables.
    std::string root_name, active_tip_name, passive_tip_name;

    // Store the robot state.
    robot_ = robot;
    ROS_INFO_STREAM_NAMED("hardware_interface","The GPSBaxterPlugin is initialing: check the robot_ is exited or not");

    // Create FK solvers.
    // Get the name of the root.
    if(!n.getParam("root_name", root_name)) {
        ROS_ERROR("Property root_name not found in namespace: '%s'", n.getNamespace().c_str());
        return false;
    }

    // Get active and passive arm end-effector names.
    if(!n.getParam("active_tip_name", active_tip_name)) {
        ROS_ERROR("Property active_tip_name not found in namespace: '%s'", n.getNamespace().c_str());
        return false;
    }
    if(!n.getParam("passive_tip_name", passive_tip_name)) {
        ROS_ERROR("Property passive_tip_name not found in namespace: '%s'", n.getNamespace().c_str());
        return false;
    }

    // Create arm chain.
    if(!arm_chain_.init(n)) {
        ROS_ERROR("Controller could not use the chain from parameter server to urdf model");
        return false;
    }

    // Create KDL chains, solvers, etc.
    // KDL chains.
    // Create passive arm chain.
    arm_chain_.toKDL(root_name, passive_tip_name, passive_arm_fk_chain_);
    // Create active arm chain.
    arm_chain_.toKDL(root_name, active_tip_name, active_arm_fk_chain_);

    // Pose solvers.
    passive_arm_fk_solver_.reset(new KDL::ChainFkSolverPos_recursive(passive_arm_fk_chain_));
    active_arm_fk_solver_.reset(new KDL::ChainFkSolverPos_recursive(active_arm_fk_chain_));

    // Jacobian sovlers.
    passive_arm_jac_solver_.reset(new KDL::ChainJntToJacSolver(passive_arm_fk_chain_));
    active_arm_jac_solver_.reset(new KDL::ChainJntToJacSolver(active_arm_fk_chain_));

    // Pull out joint states.
    int joint_index;

    // Put together joint states for the active arm.
    joint_index = 1;
    while (true)
    {
        // Check if the parameter for this active joint exists.
        std::string joint_name;
        std::string param_name = std::string("/active_arm_joint_name_" + to_string(joint_index));
        if(!n.getParam(param_name.c_str(), joint_name))
            break;

        // Push back the joint state and name.
        active_arm_joint_state_.push_back(robot_->getHandle(joint_name));

        active_arm_joint_names_.push_back(joint_name);

        // Increment joint index.
        joint_index++;
    }
    // Validate that the number of joints in the chain equals the length of the active arm joint state.
    if (active_arm_fk_chain_.getNrOfJoints() != active_arm_joint_state_.size())
    {
        ROS_INFO_STREAM("num_fk_chain: " + to_string(active_arm_fk_chain_.getNrOfJoints()));
        ROS_INFO_STREAM("num_joint_state: " + to_string(active_arm_joint_state_.size()));
        ROS_ERROR("Number of joints in the active arm FK chain does not match the number of joints in the active arm joint state!");
        return false;
    }

    // Put together joint states for the passive arm.
    joint_index = 1;
    while (true)
    {
        // Check if the parameter for this passive joint exists.
        std::string joint_name;
        std::string param_name = std::string("/passive_arm_joint_name_" + to_string(joint_index));
        if(!n.getParam(param_name, joint_name))
            break;

        // Push back the joint state and name.
        passive_arm_joint_state_.push_back(robot_->getHandle(joint_name));
        passive_arm_joint_names_.push_back(joint_name);

        // Increment joint index.
        joint_index++;
    }
    // Validate that the number of joints in the chain equals the length of the active arm joint state.
    if (passive_arm_fk_chain_.getNrOfJoints() != passive_arm_joint_state_.size())
    {
        ROS_INFO_STREAM("num_fk_chain: " + to_string(passive_arm_fk_chain_.getNrOfJoints()));
        ROS_INFO_STREAM("num_joint_state: " + to_string(passive_arm_joint_state_.size()));
        ROS_ERROR("Number of joints in the passive arm FK chain does not match the number of joints in the passive arm joint state!");
        return false;
    }


    /********************************************************************************
    * chang the code to make sure the order of the handle is suitable with the chain*
    *********************************************************************************/
   // Get joint handles for all of the joints in the chain
    /*for(std::vector<KDL::Segment>::const_iterator it = passive_arm_fk_chain_.segments.begin();
        it != passive_arm_fk_chain_.segments.end();
        ++it)
    {
      passive_arm_joint_state_.push_back(robot->getHandle(it->getJoint().getName()));
      passive_arm_joint_names_.push_back(it->getJoint().getName());
    }

    // Get joint handles for all of the joints in the chain
    for(std::vector<KDL::Segment>::const_iterator it = active_arm_fk_chain_.segments.begin();
        it != active_arm_fk_chain_.segments.end();
        ++it)
    {
      active_arm_joint_state_.push_back(robot->getHandle(it->getJoint().getName()));
      active_arm_joint_names_.push_back(it->getJoint().getName());
    }*/



    // Allocate torques array.
    active_arm_torques_.resize(active_arm_fk_chain_.getNrOfJoints());
    passive_arm_torques_.resize(passive_arm_fk_chain_.getNrOfJoints());

    // Initialize ROS subscribers/publishers, sensors, and position controllers.
    // Note that this must be done after the FK solvers are created, because the sensors
    // will ask to use these FK solvers!
    initialize(n);

    // Tell the Baxter controller manager that we initialized everything successfully.
    return true;
}

// This is called by the controller manager before starting the controller.
void GPSBaxterPlugin::starting(const ros::Time& time)
{
    // Get current time.
    last_update_time_ = time;
    controller_counter_ = 0;

    // Reset all the sensors. This is important for sensors that try to keep
    // track of the previous state somehow.
    //for (int sensor = 0; sensor < TotalSensorTypes; sensor++)
    for (int sensor = 0; sensor < 1; sensor++)
    {
        sensors_[sensor]->reset(this,last_update_time_);
    }

    // Reset position controllers.
    passive_arm_controller_->reset(last_update_time_);
    active_arm_controller_->reset(last_update_time_);

    // Reset trial controller, if any.
    if (trial_controller_ != NULL) trial_controller_->reset(last_update_time_);
}

// This is called by the controller manager before stopping the controller.
void GPSBaxterPlugin::stopping(const ros::Time& time)
{
    // Nothing to do here.
}

// This is the main update function called by the realtime thread when the controller is running.
void GPSBaxterPlugin::update(const ros::Time& time, const ros::Duration& period)
{
    // Get current time.
    last_update_time_ = time;

    // Check if this is a controller step based on the current controller frequency.
    controller_counter_++;
    if (controller_counter_ >= controller_step_length_) controller_counter_ = 0;
    bool is_controller_step = (controller_counter_ == 0);

    // Update the sensors and fill in the current step sample.
    update_sensors(last_update_time_,is_controller_step);

    // Update the controllers.
    update_controllers(last_update_time_,is_controller_step);

    // Store the torques.
    for (unsigned i = 0; i < active_arm_joint_state_.size(); i++)
        active_arm_joint_state_[i].setCommand(active_arm_torques_[i]);

    for (unsigned i = 0; i < passive_arm_joint_state_.size(); i++)
        passive_arm_joint_state_[i].setCommand(passive_arm_torques_[i]);
}

// Get current time.
ros::Time GPSBaxterPlugin::get_current_time() const
{
    return last_update_time_;
}

// Get current encoder readings (robot-dependent).
void GPSBaxterPlugin::get_joint_encoder_readings(Eigen::VectorXd &angles, gps::ActuatorType arm) const
{
    if (arm == gps::AUXILIARY_ARM)
    {
        if (angles.rows() != passive_arm_joint_state_.size())
            angles.resize(passive_arm_joint_state_.size());
        for (unsigned i = 0; i < angles.size(); i++)
            angles(i) = passive_arm_joint_state_[i].getPosition();
    }
    else if (arm == gps::TRIAL_ARM)
    {
        if (angles.rows() != active_arm_joint_state_.size())
            angles.resize(active_arm_joint_state_.size());
        for (unsigned i = 0; i < angles.size(); i++)
            angles(i) = active_arm_joint_state_[i].getPosition();
    }
    else
    {
        ROS_ERROR("Unknown ArmType %i requested for joint encoder readings!",arm);
    }
}

}

// Register controller to pluginlib
//PLUGINLIB_EXPORT_CLASS(gps_agent_pkg, GPSBaxterPlugin, gps_control::GPSBaxterPlugin, controller_interface::ControllerBase)
PLUGINLIB_EXPORT_CLASS(gps_control::GPSBaxterPlugin, controller_interface::ControllerBase)

