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
							set_pulse(port, pulse);
						}
						break;
						
					case PgdRcio::IN:
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
	int32_t mpi;
	std::string mpi_ip;
	std::string mpi_port;
	
	void out_cb(const std_msgs::Int32ConstPtr msg, int32_t port){
		std::cout << rcio_names[port] << " " << msg->data << std::endl;
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