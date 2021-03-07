Installation:
```
sudo apt install python-catkin-tools
sudo apt install ros-melodic-turtlebot3-teleop
export catkinDIR="your catkin directory"
cd catkinDIR/src
git clone git@github.com:payamn/turtlebot3_from_scratch.git
git submodule update --recursive
cd ../
catkin build
source catkinDIR/devel/setup.bash
```
# test
```
roslaunch nuslam slam.launch debug:=True
```
