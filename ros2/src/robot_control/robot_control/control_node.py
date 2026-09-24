import rclpy
import threading #für nonblocking -> damit die Dinger gleichzeitig laufen
import sys
import termios
import tty

from rclpy.node import Node
from std_msgs.msg import Bool
from std_msgs.msg import Float32
from sensor_msgs.msg import Imu

from robot_interfaces.msg import MovementCommand

def get_key():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)

    try:
        tty.setraw(fd)
        key = sys.stdin.read(1)

    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

    return key

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

        self.forward_allowed = True

        self.show_distance_continuously = False
        self.show_imu_continuously = False

        self.get_logger().info('Control Node gestartet')


    def publish_movement(self, command, value):

        # Vorwärtsfahrt nur erlauben, wenn kein Hindernis erkannt wurde
        if command == "forward" and not self.forward_allowed:
            self.get_logger().warn(
                "Vorwaertsfahrt nicht moeglich - Hindernis erkannt!"
            )
            return

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
        distance = msg.data

        if distance < 7.0:
            self.forward_allowed = False
            # self.get_logger().warn(
            #     f'Hindernis erkannt ({distance:.1f} cm) - Vorwaertsfahrt gesperrt!'
            # )
        else:
            self.forward_allowed = True

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
            print("  remote")
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

        elif user_input == "remote":
            print("Steuerung durch Tastendruck aktiviert:")
            print("w = vorwärts")
            print("s = rückwärts")
            print("a = links")
            print("d = rechts")
            print("q = beenden")

            while rclpy.ok():

                key = get_key()

                if key == "w":
                    node.publish_movement("forward", 5)

                elif key == "s":
                    node.publish_movement("backward", 2)

                elif key == "a":
                    node.publish_movement("circle_left", 4)

                elif key == "d":
                    node.publish_movement("circle_right", 4)

                elif key == "q":
                    print("\nWASD-Steuerung beendet.")
                    break

        else:

            print("Unbekannter Befehl. Tippe 'help'.")


    node.destroy_node()

    rclpy.shutdown()


if __name__ == '__main__':
    main()
