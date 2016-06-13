cmake_minimum_required(VERSION 2.8.8)
project(gps_agent_pkg)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS cmake_modules std_msgs sensor_msgs pr2_controller_interface
  pr2_mechanism_model pluginlib roscpp roslib rospy pr2_controllers_msgs control_toolbox realtime_tools
  geometry_msgs tf pr2_controller_manager controller_interface controller_manager
  hardware_interface transmission_interface moveit_visual_tools moveit_ros_planning_interface
  moveit_msgs trajectory_msgs baxter_core_msgs joint_limits_interface dynamic_reconfigure driver_base message_generation )
find_package(Boost REQUIRED COMPONENTS thread)

# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#find the caffe include file
# set(CAFFE_INCLUDEDIR /opt/caffe/include /opt/caffe/distribute/include)
# set(CAFFE_LINK_LIBRARY /opt/caffe/build/lib)
## System dependencies are found with CMake's conventions
find_package(Eigen REQUIRED)
find_package(Caffe REQUIRED)

## Set compiler optimization flags
set(CMAKE_CXX_FLAGS "-DNDEBUG -O3 -fopenmp -Wno-deprecated -Wenum-compare")
set(USE_CAFFE 1)

## Uncomment this if the package has a setup.py. This macro ensures
## modules and global scripts declared therein get installed
## See http://ros.org/doc/api/catkin/html/user_guide/setup_dot_py.html
# catkin_python_setup()

################################################
## Declare ROS messages, services and actions ##
################################################

## To declare and build messages, services or actions from within this
## package, follow these steps:
## * Let MSG_DEP_SET be the set of packages whose message types you use in
##   your messages/services/actions (e.g. std_msgs, actionlib_msgs, ...).
## * In the file package.xml:
##   * add a build_depend and a run_depend tag for each package in MSG_DEP_SET
##   * If MSG_DEP_SET isn't empty the following dependencies might have been
##     pulled in transitively but can be declared for certainty nonetheless:
##     * add a build_depend tag for "message_generation"
##     * add a run_depend tag for "message_runtime"
## * In this file (CMakeLists.txt):
##   * add "message_generation" and every package in MSG_DEP_SET to
##     find_package(catkin REQUIRED COMPONENTS ...)
##   * add "message_runtime" and every package in MSG_DEP_SET to
##     catkin_package(CATKIN_DEPENDS ...)
##   * uncomment the add_*_files sections below as needed
##     and list every .msg/.srv/.action file to be processed
##   * uncomment the generate_messages entry below
##   * add every package in MSG_DEP_SET to generate_messages(DEPENDENCIES ...)

## Generate messages in the 'msg' folder
add_message_files(FILES CaffeParams.msg ControllerParams.msg DataRequest.msg
  DataType.msg LinGaussParams.msg PositionCommand.msg RelaxCommand.msg
  SampleResult.msg TfActionCommand.msg TfObsData.msg TfParams.msg
  TrialCommand.msg)

## Generate services in the 'srv' folder
# add_service_files(FILES GlobalControl.srv)

## Generate added messages and services with any dependencies listed here
generate_messages(DEPENDENCIES geometry_msgs sensor_msgs std_msgs)
generate_dynamic_reconfigure_options(cfg/JointVelocityPID.cfg)
include_directories(${dynamic_reconfigure_PACKAGE_PATH}/cmake/cfgbuild.cmake)
###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if you package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(

 INCLUDE_DIRS include
 LIBRARIES
  gps_agent_lib
 CATKIN_DEPENDS
  std_msgs
  sensor_msgs
  pr2_controller_interface
  pr2_mechanism_model
  pluginlib
  roscpp
  roslib
  rospy
  pr2_controllers_msgs
  control_toolbox
  realtime_tools
  geometry_msgs
  tf
  pr2_controller_manager
  controller_interface
  controller_manager
  hardware_interface
  transmission_interface
  moveit_visual_tools
  moveit_ros_planning_interface
  moveit_msgs
  trajectory_msgs
  baxter_core_msgs
  joint_limits_interface
  dynamic_reconfigure
  driver_base
  message_runtime
 DEPENDS
  Eigen
  caffe
  protobuf
  boost
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(include ${catkin_INCLUDE_DIRS} ${Caffe_INCLUDE_DIRS}
  # ${Boost_INCLUDE_DIRS} ${CUDA_INCLUDE_DIRS} ${CAFFE_EXTRA_PATH})
  ${Boost_INCLUDE_DIRS} ${CUDA_INCLUDE_DIRS})
# add_definitions(-DCAFFE_GPU)
add_definitions(${Caffe_DEFINITIONS})
find_package(CUDA 5.5 REQUIRED)
# add_definitions(-DCPU_ONLY)


## Declare a cpp library
add_library(gps_agent_lib
              src/robotplugin.cpp
              src/pr2plugin.cpp
              src/sample.cpp
              src/sensor.cpp
              src/neuralnetwork.cpp
              src/tfcontroller.cpp
              src/controller.cpp
              src/lingausscontroller.cpp
              src/camerasensor.cpp
              src/positioncontroller.cpp
              src/trialcontroller.cpp
              src/encodersensor.cpp
              src/encoderfilter.cpp
              src/util.cpp
              src/neuralnetworkcaffe.cpp
              src/caffenncontroller.cpp
              src/baxter_control/arm_hardware_interface.cpp
              src/baxter_control/arm_simulator_interface.cpp
              src/baxter_control/baxter_hardware_interface.cpp
              src/baxter_control/baxter_to_csv.cpp
              src/baxter_control/baxter_utilities.cpp
              src/baxter_control/baxterplugin.cpp
              src/baxter_control/chain.cpp)
target_link_libraries(gps_agent_lib ${catkin_LIBRARIES} ${Caffe_LIBRARIES} protobuf ${Boost_LIBRARIES})

add_library(baxter_utilities src/baxter_control/baxter_utilities.cpp)
target_link_libraries(baxter_utilities ${catkin_LIBRARIES} ${Boost_LIBRARIES})
add_dependencies(baxter_utilities ${catkin_EXPORTED_TARGETS}) # don't build until necessary msgs are finished

add_library(arm_interface src/baxter_control/arm_hardware_interface.cpp
  src/baxter_control/arm_simulator_interface.cpp
  src/baxter_control/chain.cpp)
target_link_libraries(arm_interface ${catkin_LIBRARIES} ${Boost_LIBRARIES})
add_dependencies(arm_interface ${catkin_EXPORTED_TARGETS}) # don't build until necessary msgs are finished

## Declare a cpp executable
add_executable(baxter_hardware_interface src/baxter_control/baxter_hardware_interface.cpp)
target_link_libraries(baxter_hardware_interface
  baxter_utilities
  arm_interface
  ${catkin_LIBRARIES}
  ${Boost_LIBRARIES}
)
add_dependencies(baxter_hardware_interface ${catkin_EXPORTED_TARGETS} ${PROJECT_NAME}_gencfg) # don't build until necessary msgs are finish



## Add cmake target dependencies of the executable/library
## as an example, message headers may need to be generated before nodes
# add_dependencies(grasp_localizer_node grasp_localizer_generate_messages_cpp)

# link_directories(${CAFFE_LINK_LIBRARY})
## Specify libraries to link a library or executable target against

#############
## Install ##
#############

## Mark cpp header files for installation
## install launch files

