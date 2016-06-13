#include "gps_agent_pkg/positioncontroller.h"
#include "gps_agent_pkg/robotplugin.h"
#include "gps_agent_pkg/util.h"

using namespace gps_control;

// Constructor.
PositionController::PositionController(ros::NodeHandle& n, gps::ActuatorType arm, int size)
    : Controller(n, arm, size)
{
    // Initialize PD gains.
    ROS_INFO_STREAM("Initializing position controller");
    pd_gains_p_.resize(size);
    pd_gains_d_.resize(size);
    pd_gains_i_.resize(size);

    // Initialize velocity bounds.
    max_velocities_.resize(size);

    // Initialize integral terms to zero.
    pd_integral_.resize(size);
    i_clamp_.resize(size);

    // Initialize current angle and position.
    current_angles_.resize(size);
    current_angle_velocities_.resize(size);
    current_pose_.resize(size);

    // Initialize target angle and position.
    target_angles_.resize(size);
    target_pose_.resize(size);

    // Initialize joints temporary storage.
    temp_angles_.resize(size);

    // Initialize Jacobian temporary storage.
    temp_jacobian_.resize(6,size);

    // Set initial mode.
    mode_ = gps::NO_CONTROL;

    // Set initial time.
    last_update_time_ = ros::Time(0.0);

    // Set arm.
    arm_ = arm;

    report_waiting = false;
    //initialize the publisher to publish the errors of the joints
    ROS_INFO_STREAM("Initializing position controller ROS subs/pubs");
    error_s0_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_s0", 1));
    error_s1_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_s1", 1));
    error_e0_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_e0", 1));
    error_e1_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_e1", 1));
    error_w0_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_w0", 1));
    error_w1_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_w1", 1));
    error_w2_pub.reset(new realtime_tools::RealtimePublisher<std_msgs::Float32>(n, "/gps/error_w2", 1));
    config_update = false;
    srv_.reset(new dynamic_reconfigure::Server<gps_agent_pkg::JointVelocityPIDConfig>(n));
    callbackType_ = boost::bind(&PositionController::callback,this, _1, _2);
    srv_->setCallback(callbackType_);
}

// Destructor.
PositionController::~PositionController()
{
}

// Update the controller (take an action).
void PositionController::update(RobotPlugin *plugin, ros::Time current_time, boost::scoped_ptr<Sample>& sample, Eigen::VectorXd &torques)
{
    // Get current joint angles.
    plugin->get_joint_encoder_readings(temp_angles_, arm_);

    // Check dimensionality.
    assert(temp_angles_.rows() == torques.rows());
    assert(temp_angles_.rows() == current_angles_.rows());

    // Estimate joint angle velocities.
    double update_time = current_time.toSec() - last_update_time_.toSec();
    if (!last_update_time_.isZero())
    { // Only compute velocities if we have a previous sample.
        current_angle_velocities_ = (temp_angles_ - current_angles_)/update_time;
    }

    // Store new angles.
    current_angles_ = temp_angles_;

    // Update last update time.
    last_update_time_ = current_time;

    // If doing task space control, compute joint positions target.
    if (mode_ == gps::TASK_SPACE)
    {
        ROS_ERROR("Not implemented!");

        // TODO: implement.
        // Get current end effector position.

        // Get current Jacobian.

        // TODO: should also try Jacobian pseudoinverse, it may work a little better.
        // Compute desired joint angle offset using Jacobian transpose method.
        target_angles_ = current_angles_ + temp_jacobian_.transpose() * (target_pose_ - current_pose_);
    }


    // If we're doing any kind of control at all, compute torques now.
    if (mode_ != gps::NO_CONTROL)
    {
        // Compute error.
        temp_angles_ = current_angles_ - target_angles_;
        while(!error_s0_pub->trylock() && !error_s1_pub->trylock() && !error_e0_pub->trylock() && !error_e1_pub->trylock() && !error_w0_pub->trylock() && !error_w1_pub->trylock() && !error_w2_pub->trylock());
        error_s0_pub->msg_.data = temp_angles_(0);
        error_s1_pub->msg_.data = temp_angles_(1);
        error_e0_pub->msg_.data = temp_angles_(2);
        error_e1_pub->msg_.data = temp_angles_(3);
        error_w0_pub->msg_.data = temp_angles_(4);
        error_w1_pub->msg_.data = temp_angles_(5);
        error_w2_pub->msg_.data = temp_angles_(6);
        error_s0_pub->unlockAndPublish();
        error_s1_pub->unlockAndPublish();
        error_e0_pub->unlockAndPublish();
        error_e1_pub->unlockAndPublish();
        error_w0_pub->unlockAndPublish();
        error_w1_pub->unlockAndPublish();
        error_w2_pub->unlockAndPublish();


        if(config_update){
          for(int i=0;i<7;i++){
            ROS_INFO_STREAM("the config_update is :" << config_update);
            ROS_INFO_STREAM("positioncontroller update: pd:pidc:"<< i << "/" << pd_gains_p_.size() << "---" << pd_gains_p_(i) << "---"<< pd_gains_i_(i) << "---"<< pd_gains_d_(i) << "---"<< i_clamp_(i) << "---");
          }
          config_update = false;
        }
        // Add to integral term.
        pd_integral_ += temp_angles_ * update_time;

        // Clamp integral term
        for (int i = 0; i < temp_angles_.rows(); i++){
            if (pd_integral_(i) * pd_gains_i_(i) > i_clamp_(i)) {
                pd_integral_(i) = i_clamp_(i) / pd_gains_i_(i);
            }
            else if (pd_integral_(i) * pd_gains_i_(i) < -i_clamp_(i)) {
                pd_integral_(i) = -i_clamp_(i) / pd_gains_i_(i);
            }
        }

        // Compute torques.
        torques = -((pd_gains_p_.array() * temp_angles_.array()) +
                    (pd_gains_d_.array() * current_angle_velocities_.array()) +
                    (pd_gains_i_.array() * pd_integral_.array())).matrix();

        ros::spinOnce();
    }
    else
    {
        torques = Eigen::VectorXd::Zero(torques.rows());
    }

}

// Configure the controller.
void PositionController::configure_controller(OptionsMap &options)
{
    // This sets the target position.
    // This sets the mode
    ROS_INFO_STREAM("Received controller configuration");
    // needs to report when finished
    report_waiting = true;
    mode_ = (gps::PositionControlMode) boost::get<int>(options["mode"]);
    if (mode_ != gps::NO_CONTROL){
        Eigen::VectorXd data = boost::get<Eigen::VectorXd>(options["data"]);
        // Eigen::MatrixXd pd_gains = boost::get<Eigen::MatrixXd>(options["pd_gains"]);
        // for(int i=0; i<pd_gains.rows(); i++){
        //     pd_gains_p_(i) = pd_gains(i, 0);
        //     pd_gains_i_(i) = pd_gains(i, 1);
        //     pd_gains_d_(i) = pd_gains(i, 2);
        //     i_clamp_(i) = pd_gains(i, 3);
        // }
        if(mode_ == gps::JOINT_SPACE){
            target_angles_ = data;
        }else{
            ROS_ERROR("Unimplemented position control mode!");
        }
    }
}

// Check if controller is finished with its current task.
bool PositionController::is_finished() const
{
    // Check whether we are close enough to the current target.
    if (mode_ == gps::JOINT_SPACE){
        double epspos = 0.185;
        double epsvel = 0.01;
        double error = (current_angles_ - target_angles_).norm();
        double vel = current_angle_velocities_.norm();
        return (error < epspos && vel < epsvel);
    }
    else if (mode_ == gps::NO_CONTROL){
        return true;
    }
}

// Reset the controller -- this is typically called when the controller is turned on.
void PositionController::reset(ros::Time time)
{
    // Clear the integral term.
    pd_integral_.fill(0.0);

    // Clear update time.
    last_update_time_ = ros::Time(0.0);
}

void PositionController::callback(gps_agent_pkg::JointVelocityPIDConfig &config, uint32_t level){
    ROS_INFO_STREAM("update_config: " << config_update);
    update_pid(config);
}

void PositionController::update_pid(gps_agent_pkg::JointVelocityPIDConfig &config){
    pd_gains_i_ << config.s0_i,config.s1_i, config.e0_i,config.e1_i, config.w0_i, config.w1_i, config.w2_i;
    pd_gains_p_ << config.s0_p,config.s1_p, config.e0_p,config.e1_p, config.w0_p, config.w1_p, config.w2_p;
    pd_gains_d_ << config.s0_d,config.s1_d, config.e0_d,config.e1_d, config.w0_d, config.w1_d, config.w2_d;
    i_clamp_ << config.s0_c,config.s1_c, config.e0_c,config.e1_c, config.w0_c, config.w1_c, config.w2_c;
    for(int i=0;i<7;i++){
      ROS_INFO_STREAM("pd:pidc:"<< i << "/" << pd_gains_p_.size() << "---" << pd_gains_p_(i) << "---"<< pd_gains_i_(i) << "---"<< pd_gains_d_(i) << "---"<< i_clamp_(i) << "---");
    }
    ROS_INFO_STREAM("update the config");
    config_update = true;
    ROS_INFO_STREAM("update the config: " << config_update);
}

