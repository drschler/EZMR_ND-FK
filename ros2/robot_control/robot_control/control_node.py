import rclpy
import threading #für nonblocking

from rclpy.node import Node
from std_msgs.msg import Bool
from std_msgs.msg import Float32
from sensor_msgs.msg import Imu

from robot_interfaces.msg import MovementCommand


class ControlNode(Node):

    def __init__(self):
        super().__init__('control_node')

        self.movement_publisher = self.create_publisher(
            MovementCommand,
            '/movement_cmd',
            10
        )
        self.led_publisher = self.create_publisher(
            Bool,
            '/led_cmd',
            10
        )

        self.distance_subscriber = self.create_subscription(
            Float32,
            '/distance',
            self.distance_callback,
            10
        )

        self.imu_subscriber = self.create_subscription(
            Imu,
            '/imu',
            self.imu_callback,
            10
        )

        self.show_distance_continuously = False
        self.show_imu_continuously = False

        self.get_logger().info('Control Node gestartet')


    def publish_movement(self, command, value):

        msg = MovementCommand()

        msg.command = command
        msg.value = value

        self.movement_publisher.publish(msg)

        self.get_logger().info(
            f"Movement: {command}, value: {value}"
        )

    def publish_led(self, state):

        msg = Bool()
        msg.data = state

        self.led_publisher.publish(msg)

        self.get_logger().info(
            f"LED: {'ON' if state else 'OFF'}"
        )
    
    def distance_callback(self, msg):

        if self.show_distance_continuously:
            print(f"Abstand: {msg.data:.1f} cm")

    def imu_callback(self, msg):

        if self.show_imu_continuously:
            print(
                f"\nIMU ACC [m/s²]: "
                f"X={msg.linear_acceleration.x:.2f} | "
                f"Y={msg.linear_acceleration.y:.2f} | "
                f"Z={msg.linear_acceleration.z:.2f}"
            )

            print(
                f"IMU GYRO [rad/s]: "
                f"X={msg.angular_velocity.x:.2f} | "
                f"Y={msg.angular_velocity.y:.2f} | "
                f"Z={msg.angular_velocity.z:.2f}"
            )

def main(args=None):

    rclpy.init(args=args)

    node = ControlNode()

    spin_thread = threading.Thread(
        target=rclpy.spin,
        args=(node,),
        daemon=True
    )

    spin_thread.start()

    print()
    print("=== Robotersteuerung ===")
    print("Tippe 'help' fuer Hilfe")
    print()

    while rclpy.ok():

        user_input = input("> ").strip()

        parts = user_input.split()

        if user_input == "help":

            print()
            print("Verfuegbare Befehle:")
            print("  forward <Wert>")
            print("  backward <Wert>")
            print("  circle right <Wert>")
            print("  circle left <Wert>")
            print("  led on")
            print("  led off")
            print("  distance on")
            print("  imu on")
            print("  help")
            print("  exit")
            print()


        elif user_input == "exit":

            break


        elif (
            len(parts) == 2
            and parts[0] in ["forward", "backward", "circle"]
        ):

            try:

                value = int(parts[1])

                node.publish_movement(
                    parts[0],
                    value
                )

            except ValueError:

                print("Der Wert muss eine Zahl sein.")

        elif (
            len(parts) == 3
            and parts[0] == "circle"
            and parts[1] in ["left", "right"]
        ):
            try:
                value = int(parts[2])

                command = f"circle_{parts[1]}"

                node.publish_movement(
                    command,
                    value
                )

            except ValueError:
                print("Der Wert muss eine Zahl sein.")

        elif user_input == "led on":

            node.publish_led(True)


        elif user_input == "led off":

            node.publish_led(False)

        elif user_input == "distance on":
            node.show_distance_continuously = True
            print("Fortlaufende Abstandsausgabe aktiviert.")

        elif user_input == "imu on":
            node.show_imu_continuously = True
            print("Fortlaufende IMU-Datenausgabe aktiviert.")

        else:

            print("Unbekannter Befehl. Tippe 'help'.")


    node.destroy_node()

    rclpy.shutdown()


if __name__ == '__main__':
    main()
