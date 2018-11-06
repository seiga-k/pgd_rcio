# pgd_rcio package

This is a RC Input / Output interface for ROS with Raspberry PI.

The node does not need to run on Raspberry PI.
It is possible to access remote Raspberry PI via pigpiod.
So, the Raspberry PI does not need to run the ROS.

# Dependencies

This package use [pigpiod](http://abyz.me.uk/rpi/pigpio/pigpiod.html) accessing to IO on Raspberry PI.

If Raspberry PI run raspbian, it can install by apt.

# parameters

## Basic setting

- ```PigpiodIP``` : IP address of Raspberry PI.  
default : localhost

- ```PigpiodPort``` : Port number of pigpiod.  
default : 8888

## Pin settings

Pin setting is recomended to use yaml format.
It could setting as below.

```yaml
gimbal_pitch:
    port: 05
    dir: 0
    default: 1500
elevator:
    port: 16
    dir: 1
```

Top level hash string is a topic name of port for subscribe / publish.
Configuration for the port are written in nested level.
Configuration has three parameters.

- ```port``` : Port number of GPIO on Raspberry PI.
- ```dir``` : Direction of the port.  
0 for output. 1 for intput.
- ```default``` : Default output pulse width for output mode.  
pgd_rcio will output this value until the topic receive.

Thus, the example set a port 5 as output and subscribe ```~gimbal_pitch``` topic, and set a port 16 as output and publish to ```~elevator```.

# topics

Output pin subscribe a topic name as setting.
Input pin publish a topic name as setting.

Both topic type are std_msgs::Int32.
Unit of the value is micro second.
So, 1500 is a center value of typical RC PWM.

# Usage

## On Raspberry PI

### Wiring

Connect a signal pin of RC servo / ESC / RC receiver to Raspberry PI's any GPIO pin.

Plase check your RC receiver's output signal voltage.
Some one can connect directly to Raspberry PI but the other one can't.

### Run pigpiod

```
sudo pigpiod
```

## On ROS PC

### Write configuration

pgd_rcio must configuration befor use.
Configuration done by ROS parameters.

### launch

```
roslaunch pgd_rcio rcio.launch
```