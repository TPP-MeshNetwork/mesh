# MILOS: Mesh Integrated Linear Observation System

![MILOS Logo](mi.svg)

MILOS stands for Mesh Integrated Linear Observation System, which is an experimental setup demonstrating a mesh network interconnected with sensors. This example showcases how to create an IP-capable sub-network using mesh technology. Nodes within this network publish their IP addresses and internal mesh layers to an MQTT broker while utilizing internal communication simultaneously.

## Functionality

This project utilizes an experimental NAT (Network Address Translation) feature on ESP32 to translate addresses/ports from an internal subnet, facilitated by the root node running a DHCP server. The nodes communicate using low-level mesh send/receive APIs to exchange data, such as routing tables from the root to all nodes and event notifications from one node to all others in the mesh. As a demonstration, these events are also published to an MQTT broker on a subscribed topic. This setup allows receiving both internal `mesh_recv()` notifications and MQTT data events.

**Note:** This example is not supported for IPv6-only configurations.

## Core Characteristics

- **Mesh Networking:** Establishes a mesh network infrastructure capable of IP communication.
- **NAT (Network Address Translation):** Experimental feature enabling address/port translation within the internal subnet.
- **Low-Level Mesh API:** Utilizes mesh send/receive APIs for efficient data exchange.
- **Event Publishing:** Demonstrates event notifications from one node to all others in the mesh, published to an MQTT broker.
- **Integration with MQTT:** Allows subscribing to and receiving events via MQTT, alongside internal mesh communications.

## Installation

To run the example, follow these steps:

1. Clone the repository.
2. Install ESP-IDF VScode extension.
3. Compile and deploy the code to your nodes.
4. Configure MQTT broker settings as per your setup.

## Contributing

Contributions are welcome! Please fork the repository and create a pull request with your improvements.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.


