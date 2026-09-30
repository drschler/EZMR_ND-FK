# EZMR – Mobiler Roboter mit ROS 2

Projekt im Rahmen des Moduls **Echtzeitsysteme und mobile Robotik** an der HTWK Leipzig.

Das Repository enthält die im Rahmen des Projekts erstellten Programme zur Steuerung eines mobilen Roboters mit **ROS 2 Jazzy** sowie die zugehörigen Programmieraufgaben.

## Struktur

- `esp32/` – PlatformIO-Projekt für den ESP32 des Roboters
- `ros2/` – ROS-2-Workspace
  - `robot_interfaces` – eigene Nachrichtentypen
  - `robot_control` – Steuerung des Roboters
  - `turtle_control` – TurtleSim- und `/cmd_vel`-Steuerung
- `programming_tasks/`
  - `ipc/` – IPC-Programmieraufgaben
  - `socket/` – Socket-Programmieraufgaben

## ROS-2-Workspace

```bash
cd ros2
source /opt/ros/jazzy/setup.bash
colcon build
source install/setup.bash
```

## Autoren

Nick Dröschel  
Florian Kropf

HTWK Leipzig – Elektrotechnik und Informationstechnik
