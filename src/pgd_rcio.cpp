#include <ros/ros.h>
#include <ros/console.h>
#include <ros/xmlrpc_manager.h>
#include <std_msgs/Int32.h>

#include <iostream>
#include <string>
#include <map>
#include <numeric>
#include <regex>

#include <boost/bind.hpp>
#include <pigpiod_if2.h>

class PgdRcio{
public:
	PgdRcio():
	nh("~")
	{
		XmlRpc::XmlRpcValue params;
		if(!nh.getParam(nh.getNamespace(), params)){
			ROS_ERROR("rcio must need valid parameters");
			ros::shutdown();
		}
		nh.param<std::string>("PigpiodIP", mpi_ip, "localhost");
		nh.param<std::string>("PigpiodPort", mpi_port, "8888");
		mpi = pigpio_start(const_cast<char*>(mpi_ip.c_str()), const_cast<char*>(mpi_port.c_str()));
		if(mpi < 0){
			ROS_ERROR("Pigpiod Error : %s", pigpio_error(mpi));
			ros::shutdown();
		}
		try{
			for(auto&& param : params){
				if(param.second.hasMember("dir") && param.second.hasMember("port")){
					std::cout << param.first << " " << param.second << std::endl;
					int32_t port(param.second["port"]);
					if(rcio_names.count(port) > 0){
						ROS_ERROR("Multiple port definition!!!");
						throw std::invalid_argument("Port number is duplicated");
					}
					std::string name(std::regex_replace(param.first, std::regex(R"(\W)"), "_"));
					if(name != param.first){
						ROS_WARN("Port Name is renamed to %s", name.c_str());
					}
					rcio_names[port] = name;
					DIR dir(static_cast<DIR>(static_cast<int>(param.second["dir"])));
					switch(dir){
					case PgdRcio::OUT:
						{
							sub_rcouts[port] = nh.subscribe<std_msgs::Int32>(name, 1, boost::bind(&PgdRcio::out_cb, this, _1, port));
							int ret = set_mode(mpi, port, PI_OUTPUT);
							if(ret < 0){
								ROS_ERROR("Pigpiod Error : %s", pigpio_error(ret));
								ros::shutdown();
							}
							int32_t pulse(1500);
							if(param.second.hasMember("default")){
								pulse = static_cast<int>(param.second["default"]);
							}
							std::cout << "Set port " << port << " as output" << std::endl;
							rcio_default_pulses[port] = pulse;
							set_pulse(port, pulse);
						}
						break;
						
					case PgdRcio::IN:
						{
							pub_rcins[port] = nh.advertise<std_msgs::Int32>(name, 1);
							int ret = set_pull_up_down(mpi, port, PI_PUD_DOWN);
							if(ret < 0){
								ROS_ERROR("Pigpiod Error. Failed to set pull up / down : %s", pigpio_error(ret));
								ros::shutdown();
							}
							ret = set_mode(mpi, port, PI_INPUT);
							if(ret < 0){
								ROS_ERROR("Pigpiod Error. Failed to set pin mode : %s", pigpio_error(ret));
								ros::shutdown();
							}
							ret = callback_ex(mpi, port, EITHER_EDGE, PgdRcio::in_cb, reinterpret_cast<void*>(this));
							if(ret < 0){
								ROS_ERROR("Pigpiod Error. Failed to set callback : %s", pigpio_error(ret));
								ros::shutdown();
							}
							std::cout << "Set port " << port << " as input" << std::endl;
						}
						break;
					}
				}
			}
		}
		catch(std::invalid_argument& e){
			ROS_ERROR("Parameter parse error : %s", e.what());
			ros::shutdown();
		}
		catch(XmlRpc::XmlRpcException& e){
			ROS_ERROR("Parameter parse error. Code %d : %s", e.getCode(), e.getMessage().c_str());
			ros::shutdown();
		}
		if(rcio_names.size() == 0){
			ROS_ERROR("No configurable ports.");
			ros::shutdown();
		}
	}
	
	~PgdRcio(){
		if(mpi >= 0){
			for(auto sub : sub_rcouts){
				set_pulse(sub.first, rcio_default_pulses[sub.first]);
			}
			pigpio_stop(mpi);
		}
	}
	
private:
	enum DIR{
		OUT = 0,
		IN = 1
	};
	ros::NodeHandle nh;
	std::map<int32_t, ros::Subscriber> sub_rcouts;
	std::map<int32_t, ros::Publisher> pub_rcins;
	std::map<int32_t, std::string> rcio_names;
	std::map<int32_t, int32_t> rcio_default_pulses;
	std::map<int32_t, uint32_t> rcio_rising_tick;
	std::map<int32_t, uint32_t> rcio_falling_tick;
	int32_t mpi;
	std::string mpi_ip;
	std::string mpi_port;
	
	void out_cb(const std_msgs::Int32ConstPtr msg, int32_t port){
		//std::cout << rcio_names[port] << " " << msg->data << std::endl;
		set_pulse(port, msg->data);
	}

	static void in_cb(int pi, unsigned int user_gpio, unsigned int level, unsigned int tick, void* userdata){
		PgdRcio* obj = reinterpret_cast<PgdRcio*>(userdata);
		obj->in_cb_proc(pi, user_gpio, level, tick);
	}

	void in_cb_proc(int pi, unsigned int user_gpio, unsigned int level, unsigned int tick){
		if(level != 0){
			rcio_rising_tick[user_gpio] = tick;
		}else{
			rcio_falling_tick[user_gpio] = tick;
			std_msgs::Int32 msg;
			msg.data = rcio_falling_tick[user_gpio] - rcio_rising_tick[user_gpio];
			pub_rcins[user_gpio].publish(msg);
		}
	}
	
	int32_t set_pulse(int32_t port, int32_t pulsewidth){
		if(pulsewidth < 500){
			pulsewidth = 500;
		}else if(pulsewidth > 2500){
			pulsewidth = 2500;
		}
		int ret = set_servo_pulsewidth(mpi, port, pulsewidth);
		if(ret < 0){
			ROS_ERROR("Pigpiod Error : %s", pigpio_error(ret));
		}
		return pulsewidth;
	}
};

int main(int argc, char** argv){
	ros::init(argc, argv, "pigpiod rc io interface");
	PgdRcio rcio;
	ros::spin();
	return 0;
}