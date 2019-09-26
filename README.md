ns3-distributed-bridge
---

`ns3-distributed-bridge` is a special bridge NetDevice that allows you to bridge a simulated network in ns3 with remote networks. The remote networks can be another ns3 simulation or a regular Linux host/bridge.

### Install

To install `ns3-distributed-bridge`, clone `ns3-distributed-bridge` as `distributed-bridge` in `src/` folder of ns3, and reconfigure ns3.

```
$ git clone https://github.com/Nat-Lab/ns3-distributed-bridge src/distributed-bridge
$ ./waf configure
```

### Usage

To use `ns3-distributed-bridge`, you need to start a distribution server. You can find the server here: <https://github.com/Nat-Lab/bridge-distribute>.

Once you have the server started, you can bridge your simulated network to the distribution server. Bridging can be easily done with the `DistributedBridgeHelper`. The helper's usage is similar to TapBridge, except you don't need an IP address on the bridged device.

First, construct the helper object with parameters `(const char *server_addr, in_port_t port, uint8_t network_id)`, where: 

- `server_addr`: the IP address string in the dotted notation of the dist-server.
- `port`: the port number of dist-server.
- `network_id`: the ID of the network to bridge. Consider it as VLAN ID. Networks with the same ID will be bridge together by dist-server.

An example of bridging a CSMA network with only one node to the server is shown below:

```c++
// Helpers
CsmaHelper csma;
Ipv4AddressHelper addr;
InternetStackHelper inet;

// == dist-bridge configuration ==
// server:      127.0.0.1
// server port: 2672
// network id:  1
DistributedBridgeHelper dist ("127.0.0.1", 2672, 1);

// Create Nodes
Ptr<Node> client_node = CreateObject<Node> ();
Ptr<Node> br_node = CreateObject<Node> ();

// Install internet stack
inet.Install(NodeContainer (client_node, br_node));

// Assign address for client. Note that no address is needed on dist-bridge
// node
addr.SetBase("10.0.0.0", "255.255.255.0");
addr.Assign(dev_client);

// Install dist-bridge
dist.Install(br_node, dev_br);
```

### License

UNLICENSE