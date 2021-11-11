/**
 * This node:
 * - receives movement data from pilot and transforms it into byte array and publishes it
 * - receives byte array from protocol_bridge, parses it and publishes it
 */

#include "../include/hardware_bridge.h"

void HardwareBridge::HardwareBridge() {

    inputMessageSubscriber = this->create_subscription<std_msgs::msg::UInt8MultiArrayConstPtr>(INPUT_PARCEL_TOPIC,
                                                                                               1000,
                                                                                               std::bind(
                                                                                                       &MinimalSubscriber::inputMessage_callback,
                                                                                                       this, _1));

}

void HardwareBridge::inputMessage_callback(const std_msgs::UInt8MultiArrayConstPtr msg) {
    std::vector<uint8_t> received_vector;
    for(int i = 0; i < ResponseMessage::length; i++) {
        received_vector.push_back(msg->data[i]);
    }
    bool ok = responseMessage.parseVector(received_vector);
    if (ok) {
        NODELET_DEBUG("Received depth: %f", responseMessage.depth);
        depthMessage.data = std::abs(static_cast<int>(responseMessage.depth * 100.0f)); // Convert metres to centimetres
        // TODO: Test yaw obtaining
        yawMessage.data = static_cast<int>(responseMessage.yaw * 100.0f);
    }
    else
        NODELET_ERROR("Wrong checksum");
}

void hardware_bridge::onInit() {
    // Initializing nodelet and parameters
    NODELET_INFO("Initializing nodelet: hardware_brige");
    ros::NodeHandle& nodeHandle = getNodeHandle();
    // ROS publishers
    outputMessagePublisher = nodeHandle.advertise<std_msgs::UInt8MultiArray>(OUTPUT_PARCEL_TOPIC, 1000);
    depthPublisher = nodeHandle.advertise<std_msgs::UInt32>(DEPTH_PUBLISH_TOPIC, 1000);
    yawPublisher = nodeHandle.advertise<std_msgs::Int32>(YAW_PUBLISH_TOPIC, 20);
    // ROS subscribers
    inputMessageSubscriber = nodeHandle.subscribe(INPUT_PARCEL_TOPIC, 1000,
            &hardware_bridge::inputMessage_callback, this);
    // ROS services
    lagAndMarchService = nodeHandle.advertiseService(SET_LAG_AND_MARCH_SERVICE,
            &hardware_bridge::lagAndMarchCallback, this);
    depthService = nodeHandle.advertiseService(SET_DEPTH_SERVICE, &hardware_bridge::depthCallback, this);
    yawService = nodeHandle.advertiseService(SET_YAW_SERVICE, &hardware_bridge::yawCallback, this);
    imuService = nodeHandle.advertiseService(SET_IMU_ENABLED_SERVICE, &hardware_bridge::imuCallback, this);
    stabilizationService = nodeHandle.advertiseService(SET_STABILIZATION_SERVICE,
            &hardware_bridge::stabilizationCallback, this);
    deviceActionService = nodeHandle.advertiseService(SET_DEVICE_SERVICE,
            &hardware_bridge::deviceActionCallback, this);
    // Output message container
    outputMessage.layout.dim.push_back(std_msgs::MultiArrayDimension());
    outputMessage.layout.dim[0].size = RequestMessage::length;
    outputMessage.layout.dim[0].stride = RequestMessage::length;
    outputMessage.layout.dim[0].label = "outputMessage";
    // Initializing timer for publishing messages. Callback interval: 0.05 ms
    publishingTimer = nodeHandle.createTimer(ros::Duration(0.05),
            &hardware_bridge::timerCallback, this);
}

bool hardware_bridge::lagAndMarchCallback(stingray_drivers_msgs::SetLagAndMarch::Request& lagAndMarchRequest,
                                          stingray_drivers_msgs::SetLagAndMarch::Response& lagAndMarchResponse) {

    if (lagStabilizationEnabled) {
      requestMessage.march = static_cast<int16_t> (0.0);
      requestMessage.lag = static_cast<int16_t> (0.0);
      requestMessage.lag_error = static_cast<int16_t> (lagAndMarchRequest.lag);
    } else {
      requestMessage.march = static_cast<int16_t> (lagAndMarchRequest.march);
      requestMessage.lag = static_cast<int16_t> (lagAndMarchRequest.lag);
    }

    isReady = true;
    lagAndMarchResponse.success = true;
    return true;
}

bool hardware_bridge::depthCallback(stingray_drivers_msgs::SetInt32::Request& depthRequest,
                                    stingray_drivers_msgs::SetInt32::Response& depthResponse) {
    if (!depthStabilizationEnabled) {
        depthResponse.success = false;
        depthResponse.message = "Depth stabilization is not enabled";
        return true;
    }
    NODELET_DEBUG("Setting depth to %d", depthRequest.value);
    requestMessage.depth	= -(static_cast<int16_t> (depthRequest.value * 100)); // For low-level stabilization purposes
    NODELET_DEBUG("Sending to STM32 depth value: %d", requestMessage.depth);

    isReady = true;
    depthResponse.success = true;
    return true;
}

bool hardware_bridge::yawCallback(stingray_drivers_msgs::SetInt32::Request& yawRequest,
                                  stingray_drivers_msgs::SetInt32::Response& yawResponse) {
    if (!yawStabilizationEnabled) {
        yawResponse.success = false;
        yawResponse.message = "Yaw stabilization is not enabled";
        return true;
    }
    NODELET_DEBUG("Setting depth to %d", yawRequest.value);
    requestMessage.yaw = yawRequest.value;
    NODELET_DEBUG("Sending to STM32 depth value: %d", requestMessage.yaw);

    isReady = true;
    yawResponse.success = true;
    return true;
}

bool hardware_bridge::imuCallback(std_srvs::SetBool::Request& imuRequest,
                                  std_srvs::SetBool::Response& imuResponse) {
    NODELET_DEBUG("Setting SHORE_STABILIZE_IMU_BIT to %d", imuRequest.data);
    setStabilizationState(requestMessage, SHORE_STABILIZE_IMU_BIT, imuRequest.data);

    isReady = true;
    imuResponse.success = true;

    return true;
}

bool hardware_bridge::stabilizationCallback(stingray_drivers_msgs::SetStabilization::Request& stabilizationRequest,
                                            stingray_drivers_msgs::SetStabilization::Response& stabilizationResponse) {
    NODELET_DEBUG("Setting depth stabilization %d", stabilizationRequest.depthStabilization);
    setStabilizationState(requestMessage, SHORE_STABILIZE_DEPTH_BIT, stabilizationRequest.depthStabilization);
    NODELET_DEBUG("Setting yaw stabilization %d", stabilizationRequest.yawStabilization);
    setStabilizationState(requestMessage, SHORE_STABILIZE_YAW_BIT, stabilizationRequest.yawStabilization);
    NODELET_DEBUG("Setting lag stabilization %d", stabilizationRequest.lagStabilization);
    setStabilizationState(requestMessage, SHORE_STABILIZE_LAG_BIT, stabilizationRequest.lagStabilization);
    depthStabilizationEnabled = stabilizationRequest.depthStabilization;
    yawStabilizationEnabled = stabilizationRequest.yawStabilization;
    lagStabilizationEnabled = stabilizationRequest.lagStabilization;

    isReady = true;
    stabilizationResponse.success = true;
    return true;
}

bool hardware_bridge::deviceActionCallback(stingray_drivers_msgs::SetDeviceAction::Request& deviceRequest,
                                           stingray_drivers_msgs::SetDeviceAction::Response& deviceResponse) {
    ROS_INFO("Setting device [%d] action value to %d", deviceRequest.device, deviceRequest.value);
    requestMessage.dev[deviceRequest.device]  = deviceRequest.value;

    isReady = true;
    deviceResponse.success = true;
    return true;
}

/** @brief Timer callback. Make byte array to publish for protocol_node and publishes it
  *
  */
void hardware_bridge::timerCallback(const ros::TimerEvent& event) {
    NODELET_DEBUG("Timer callback");
    if (isReady){
        // Make output message
        std::vector<uint8_t> output_vector = requestMessage.formVector();
        outputMessage.data.clear();
        for(int i=0; i<RequestMessage::length; i++) {
            outputMessage.data.push_back(output_vector[i]);
        }
        // Publish messages
        outputMessagePublisher.publish(outputMessage);
        depthPublisher.publish(depthMessage);
        yawPublisher.publish(yawMessage);
        NODELET_DEBUG("HARDWARE BRIDGE PUBLISH");
    }
    else NODELET_DEBUG("Wait for topic updating");

}

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MinimalSubscriber>());

    std::shared_ptr<rclcpp::Node> node = rclcpp::Node::make_shared("add_three_ints_server");

    rclcpp::Service<tutorial_interfaces::srv::AddThreeInts>::SharedPtr service =
            node->create_service<tutorial_interfaces::srv::AddThreeInts>("add_three_ints", &add);

    RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Ready to add three ints.");

    rclcpp::spin(node);
    rclcpp::shutdown();
}