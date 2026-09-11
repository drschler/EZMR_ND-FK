cmake_minimum_required(VERSION 3.8)
project(robot_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

#generieren des ROS-Datentyps
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/MotorCommand.msg"
  "msg/MovementCommand.msg"
)

ament_package()
