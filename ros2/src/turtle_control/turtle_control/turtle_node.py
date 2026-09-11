import rclpy
import time
import sys
import termios
import tty
from rclpy.node import Node
from geometry_msgs.msg import Twist

def get_key():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)

    try:
        tty.setraw(fd)
        key = sys.stdin.read(1)

    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

    return key

class TurtleNode(Node):

    def __init__(self):
        super().__init__('turtle_node')

        self.get_logger().info('Turtle Node gestartet')

        self.publisher_ = self.create_publisher(
            Twist,
            '/turtle1/cmd_vel',
            10
        )


    def drive_forward(self, duration):
        msg = Twist()

        msg.linear.x = 2.0
        msg.angular.z = 0.0

        self.publisher_.publish(msg)
        time.sleep(duration)

        self.stop()


    def turn_left(self, duration):
        msg = Twist()

        msg.linear.x = 0.0
        msg.angular.z = 1.57

        self.publisher_.publish(msg)
        time.sleep(duration)

        self.stop()


    def stop(self):
        msg = Twist()

        msg.linear.x = 0.0
        msg.angular.z = 0.0

        self.publisher_.publish(msg)


    def drive_square(self):

        for i in range(4):

            self.get_logger().info(f'Seite {i + 1}')

            self.drive_forward(2.0)
            time.sleep(0.2)

            self.turn_left(1.0)
            time.sleep(0.2)

    def move(self, linear, angular):
        msg = Twist()

        msg.linear.x = linear
        msg.angular.z = angular

        self.publisher_.publish(msg)

def main(args=None):

    rclpy.init(args=args)

    node = TurtleNode()

    print("Steuerung:")
    print("w = vorwärts")
    print("s = rückwärts")
    print("a = links")
    print("d = rechts")
    print("x = stoppen")
    print("q = beenden")

    while rclpy.ok():

        key = get_key()

        if key == "w":
            node.move(2.0, 0.0)

        elif key == "s":
            node.move(-2.0, 0.0)

        elif key == "a":
            node.move(0.0, 1.5)

        elif key == "d":
            node.move(0.0, -1.5)

        elif key == "x":
            node.move(0.0, 0.0)

        elif key == "q":
            node.move(0.0, 0.0)
            break

    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()