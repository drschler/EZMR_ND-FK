#include <Arduino.h>
#include <micro_ros_platformio.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/float32.h>
#include <cstring>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <sensor_msgs/msg/imu.h>
#include <geometry_msgs/msg/twist.h>

//Paketnummern, die wir selber erstellt haben 
#include <robot_interfaces/msg/movement_command.h>
#include <robot_interfaces/msg/motor_command.h> 

// Definitionen
#define PIEZO_PIN 4
#define LED_PIN 15
#define MOTOR_TX_PIN 17
#define HC_TRIG_PIN 26
#define HC_ECHO_PIN 25
#define I2C_SDA 21
#define I2C_SCL 22

const uint8_t OLED_ADDR = 0x3C;
const uint8_t IMU_ADDR = 0x68;


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);


HardwareSerial MotorSerial(2);

rcl_subscription_t led_subscriber;
std_msgs__msg__Bool led_msg;

rcl_subscription_t motor_subscriber;
robot_interfaces__msg__MotorCommand motor_msg;

rcl_subscription_t movement_subscriber;
robot_interfaces__msg__MovementCommand movement_msg;

rcl_subscription_t cmd_vel_subscriber;
geometry_msgs__msg__Twist cmd_vel_msg;

rcl_publisher_t distance_publisher;
std_msgs__msg__Float32 distance_msg;

rcl_publisher_t imu_publisher;
sensor_msgs__msg__Imu imu_msg;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

char movement_buffer[32];
unsigned long last_distance_measurement = 0;

// Makros
#define RCCHECK(fn) \
  { \
    rcl_ret_t temp_rc = fn; \
    if (temp_rc != RCL_RET_OK) { \
      error_loop(); \
    } \
  }

#define RCSOFTCHECK(fn) \
  { \
    rcl_ret_t temp_rc = fn; \
    if (temp_rc != RCL_RET_OK) {} \
  }

// Fehlerbehandlung
void error_loop()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Jetzt stecken wir in der Error-Schleife!");
  display.println("Wahrscheinlich weil der Micro-ROS-Agent nicht erreichbar ist.");
  display.println("Bitte den Micro-ROS-Agent starten und den ESP32 resetten!");
  display.display();
  while (1)
  {
    delay(100);
  }
  display.clearDisplay();
}

// Funktionen
void show_distance_oled(float distance)
{
    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Abstand");

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(distance, 1);
    display.println(" cm");

    display.display();
}
float measure_distance()
{
    // Trigger zunächst LOW
    digitalWrite(HC_TRIG_PIN, LOW);
    delayMicroseconds(2);

    // 10 µs Triggerimpuls
    digitalWrite(HC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HC_TRIG_PIN, LOW);

    // Dauer des Echoimpulses messen
    unsigned long duration = pulseIn(HC_ECHO_PIN, HIGH, 30000);
    return duration;
}

float measure_distance_cm()
{
    float distance = 0.0f;

    do
    {
        unsigned long duration = measure_distance();

        // Timeout -> keine gültige Messung
        if (duration == 0)
        {
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("ALARM - Fehler bei der Abstandsmessung!");
            display.println("Der hat einen Wackler -> mal abchecken.");
            display.println("Geht erst weiter, wenn der Sensor wieder korrekt misst.");
            display.display();

            delay(100);
            continue;
        }

        // Entfernung in cm umrechnen
        // Entfernung = Laufzeit × Schallgeschwindigkeit / 2
        distance = duration * 0.0343f / 2.0f;

        // Unplausible Messwerte verwerfen
        if (distance < 2.0f) //|| distance > 2000.0f
        {
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("ALARM - Unplausibler Abstand!");
            display.print("Messwert: ");
            display.print(distance, 1);
            display.println(" cm");
            display.println("Messung wird wiederholt.");
            display.display();

            distance = 0.0f;
            delay(100);
        }

    } while (distance == 0.0f);

    // wenn eine gültige Messung vorliegt
    display.clearDisplay();
    show_distance_oled(distance);

    return distance;
}

void beep_start(){
    tone(PIEZO_PIN, 523);   // C
    delay(120);

    tone(PIEZO_PIN, 659);   // E
    delay(120);

    tone(PIEZO_PIN, 784);   // G
    delay(120);

    tone(PIEZO_PIN, 1047);  // hohes C
    delay(250);

    noTone(PIEZO_PIN);

    delay(100);

    // Kleiner bescheuerter Abschluss
    tone(PIEZO_PIN, 784);
    delay(80);

    tone(PIEZO_PIN, 1047);
    delay(300);

    noTone(PIEZO_PIN);
}

void alarm_beep()
{
    for (int i = 0; i < 2; i++)
    {
        tone(PIEZO_PIN, 1200);
        delay(120);

        tone(PIEZO_PIN, 1800);
        delay(120);
    }

    noTone(PIEZO_PIN);
}

void send_motor_command(uint8_t id, uint8_t mode, uint8_t dir, uint16_t param)
{
    uint8_t packet[6];

    packet[0] = 0xAA;
    packet[1] = id;
    packet[2] = mode;
    packet[3] = dir;
    packet[4] = (uint8_t)(param & 0xFF);
    packet[5] = (uint8_t)((param >> 8) & 0xFF);

    MotorSerial.write(packet, sizeof(packet));
}

void drive_forward(const uint16_t value)
{
    send_motor_command(1, 0, 1, value);
    send_motor_command(2, 0, 0, value);
}

void drive_forward_safe(uint16_t total_steps)
{
    const uint16_t chunk_size = 5;
    const float ms_per_step = 780.0f;   // erstmal experimentell bestimmen, wie lange ein Schritt dauert um abzuschätzen, wann man nächsten Motorbefehl senden kann
    const float cm_per_step = 3.0f;
    const float target_distance = 7.0f;

    uint16_t driven_steps = 0;

    digitalWrite(LED_PIN, LOW);

    while (driven_steps < total_steps)
    {
        float distance = measure_distance_cm();

        // Reicht der Platz nicht mehr für einen kompletten Chunk?
        if (distance > 0 &&
            distance < (cm_per_step * chunk_size + target_distance))
        {
            // Feinannäherung
            while (distance > target_distance &&
                   driven_steps < total_steps)
            {
                digitalWrite(LED_PIN, HIGH);
                drive_forward(1);
                driven_steps++;

                delay(ms_per_step);

                distance = measure_distance_cm();
            }
            digitalWrite(LED_PIN, LOW);
            alarm_beep();
            break;
        }

        uint16_t remaining = total_steps - driven_steps;
        uint16_t next_chunk = (remaining < chunk_size) ? remaining : chunk_size; //verrückte IF-Nummer in einer Zeile

        drive_forward(next_chunk);

        driven_steps += next_chunk;

        delay(ms_per_step * next_chunk);
    }
}

void drive_backward(const uint16_t value)
{
    send_motor_command(1, 0, 0, value);
    send_motor_command(2, 0, 1, value);
}

void drive_circle_left(const uint16_t value)
{
    send_motor_command(1, 0, 0, value);
    send_motor_command(2, 0, 0, value);
}

void drive_circle_right(const uint16_t value)
{
    send_motor_command(1, 0, 1, value);
    send_motor_command(2, 0, 1, value);
}

void imu_write_register(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(IMU_ADDR);

    Wire.write(reg);      // Register auswählen
    Wire.write(value);    // Wert in Register schreiben

    Wire.endTransmission();
}

uint8_t imu_read_register(uint8_t reg)
{
    Wire.beginTransmission(IMU_ADDR);

    Wire.write(reg);                  // Register auswählen
    Wire.endTransmission(false);      // Verbindung nicht beenden

    Wire.requestFrom(IMU_ADDR, (uint8_t)1);

    if (Wire.available())
    {
        return Wire.read();
    }

    return 0xFF;
}

int16_t imu_read_16bit(uint8_t reg_high)
{
    Wire.beginTransmission(IMU_ADDR);

    Wire.write(reg_high);
    Wire.endTransmission(false);

    Wire.requestFrom(IMU_ADDR, (uint8_t)2);

    if (Wire.available() >= 2)
    {
        uint8_t high_byte = Wire.read();
        uint8_t low_byte  = Wire.read();

        return (int16_t)((high_byte << 8) | low_byte);
    }

    return 0;
}

void imu_check()
{
  // IMU auslesen
  int16_t accel_x = imu_read_16bit(0x3B);
  int16_t accel_y = imu_read_16bit(0x3D);
  int16_t accel_z = imu_read_16bit(0x3F);

  int16_t gyro_x = imu_read_16bit(0x43);
  int16_t gyro_y = imu_read_16bit(0x45);
  int16_t gyro_z = imu_read_16bit(0x47);


  // Beschleunigung -> m/s²
  imu_msg.linear_acceleration.x =
      (accel_x / 16384.0f) * 9.80665f;

  imu_msg.linear_acceleration.y =
      (accel_y / 16384.0f) * 9.80665f;

  imu_msg.linear_acceleration.z =
      (accel_z / 16384.0f) * 9.80665f;


  // Winkelgeschwindigkeit -> rad/s
  imu_msg.angular_velocity.x =
      (gyro_x / 131.0f) * DEG_TO_RAD;

  imu_msg.angular_velocity.y =
      (gyro_y / 131.0f) * DEG_TO_RAD;

  imu_msg.angular_velocity.z =
      (gyro_z / 131.0f) * DEG_TO_RAD;
}

// Callback-Funktionen
void led_callback(const void * msgin)
{
  //wandeln der msg in bool -> einfache für digitalWrite
  const std_msgs__msg__Bool * msg =
      (const std_msgs__msg__Bool *)msgin;

  digitalWrite(LED_PIN, msg->data ? HIGH : LOW);
}

void motor_callback(const void * msgin)
{
    const robot_interfaces__msg__MotorCommand * msg =
        (const robot_interfaces__msg__MotorCommand *)msgin;

    send_motor_command(
        msg->id,
        msg->mode,
        msg->dir,
        msg->param
    );
}

void movement_callback(const void * msgin)
{
    const robot_interfaces__msg__MovementCommand * msg =
        (const robot_interfaces__msg__MovementCommand *)msgin;


    if (strcmp(msg->command.data, "forward") == 0)
    {
        drive_forward_safe(msg->value);
    }
    else if (strcmp(msg->command.data, "backward") == 0)
    {
        drive_backward(msg->value);
    }
    else if (strcmp(msg->command.data, "circle_right") == 0)
    {
        drive_circle_right(msg->value);
    }
    else if (strcmp(msg->command.data, "circle_left") == 0)
    {
        drive_circle_left(msg->value);
    }
}

void cmd_vel_callback(const void * msgin)
{
    const geometry_msgs__msg__Twist * msg =
        (const geometry_msgs__msg__Twist *)msgin;

    if (msg->linear.x > 0.0)
    {
        drive_forward_safe(5);
    }
    else if (msg->linear.x < 0.0)
    {
        drive_backward(5);
    }
    else if (msg->angular.z > 0.0)
    {
        drive_circle_left(5);
    }
    else if (msg->angular.z < 0.0)
    {
        drive_circle_right(5);
    }
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIEZO_PIN, OUTPUT);
  beep_start();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(HC_TRIG_PIN, OUTPUT);
  pinMode(HC_ECHO_PIN, INPUT);
  digitalWrite(HC_TRIG_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("ROS Roboter");
  display.setCursor(0, 20);
  display.println("Jetzt gehts los!");
  display.display();

  // IMU aufwecken
  imu_write_register(0x6B, 0x00); 
  // Accelerometer: ±2 g
  imu_write_register(0x1C, 0x00);
  // Gyroskop: ±250 °/s
  imu_write_register(0x1B, 0x00);

  // für serielle Bespielung -> in .ini noch: board_microros_transport = serial 
  // set_microros_serial_transports(Serial);

  // für Wifi Bespielung
//   IPAddress agent_ip(192, 168, 188, 111); //Adresse des Micro-ROS-Agenten im lokalen Netzwerk
  IPAddress agent_ip(172, 20, 10, 2);

//   char ssid[] = "FRITZ!Box 3272";
//   char psk[]  = "45647999863570256502";
  char ssid[] = "iPhone von Nick";
  char psk[]  = "12345678";

  set_microros_wifi_transports(
      ssid,
      psk,
      agent_ip,
      8888
  );

  MotorSerial.begin(9600, SERIAL_8N1, -1, MOTOR_TX_PIN);

  movement_msg.command.data = movement_buffer;
  movement_msg.command.size = 0;
  movement_msg.command.capacity = sizeof(movement_buffer);

  delay(2000);

  allocator = rcl_get_default_allocator();

  // Init micro-ROS
  RCCHECK(
    rclc_support_init(
      &support,
      0,
      NULL,
      &allocator
    )
  );

  // Create node
  RCCHECK(
    rclc_node_init_default(
      &node,
      "robot_node",
      "",
      &support
    )
  );

  // Create Subscriber
  RCCHECK(
    rclc_subscription_init_default(
      &led_subscriber,
      &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
      "led_cmd"
    )
  );

  RCCHECK(rclc_subscription_init_default(
    &motor_subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(robot_interfaces, msg, MotorCommand),
    "motor_cmd")
  );

RCCHECK(rclc_subscription_init_default(
    &movement_subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(robot_interfaces, msg, MovementCommand),
    "movement_cmd")
  );

RCCHECK(rclc_subscription_init_default(
    &cmd_vel_subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
    "cmd_vel")
  );

  // Create executor
  RCCHECK(
    rclc_executor_init(
      &executor,
      &support.context,
      4, // number of subscriptions
      &allocator
    )
  );

  // Add subscription to executor -> wenn neue Daten kommen, wird led_callback aufgerufen
  RCCHECK(
    rclc_executor_add_subscription(
      &executor,
      &led_subscriber,
      &led_msg,
      &led_callback,
      ON_NEW_DATA
    )
  );

  // Add motor subscription to executor -> wenn neue Daten kommen, wird motor_callback aufgerufen
  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &motor_subscriber,
      &motor_msg,
      &motor_callback,
      ON_NEW_DATA
    )
  );

  // Add movement subscription to executor -> wenn neue Daten kommen, wird movement_callback aufgerufen
  RCCHECK(rclc_executor_add_subscription(
      &executor,
      &movement_subscriber,
      &movement_msg,
      &movement_callback,
      ON_NEW_DATA
    )
  );

// Add cmd_vel subscription to executor
RCCHECK(
    rclc_executor_add_subscription(
        &executor,
        &cmd_vel_subscriber,
        &cmd_vel_msg,
        &cmd_vel_callback,
        ON_NEW_DATA
    )
);

  // Create publisher
  RCCHECK(
    rclc_publisher_init_default(
        &distance_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
        "distance"
    )
  );

  RCCHECK(
    rclc_publisher_init_default(
        &imu_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
        "imu"
    )
  );
}

void loop()
{
  // Spin executor
  RCSOFTCHECK(
    rclc_executor_spin_some(
      &executor,
      RCL_MS_TO_NS(100)
    )
  );

  // Alle 250 ms Abstand messen
  if (millis() - last_distance_measurement >= 250)
  {
    last_distance_measurement = millis();

    distance_msg.data = measure_distance_cm();

    RCSOFTCHECK(
        rcl_publish(
            &distance_publisher,
            &distance_msg,
            NULL
        )
    );

    imu_check();

    RCSOFTCHECK(
      rcl_publish(
          &imu_publisher,
          &imu_msg,
          NULL
      )
    );

    //Testen mit Abstand und piepen dann
    // if (distance_msg.data > 0 && distance_msg.data < 7.0)
    // {
    //   tone(PIEZO_PIN, 1000);
    // }
    // else
    // {
    //   noTone(PIEZO_PIN);
    // }
  }
  delay(10);
}