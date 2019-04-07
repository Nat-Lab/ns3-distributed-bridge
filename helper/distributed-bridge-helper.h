#ifndef DISTRIBUTED_BRIDGE_HELPER_H
#define DISTRIBUTED_BRIDGE_HELPER_H

#include "ns3/distributed-bridge.h"
#include "ns3/object-factory.h"
#include "ns3/names.h"
#include <string>

namespace ns3 {

class Node;
class AttributeValue;

class DistributedBridgeHelper {
public:
    DistributedBridgeHelper ();
    DistributedBridgeHelper (const char *addr, in_port_t port, uint8_t channel_id);
    DistributedBridgeHelper (const Ipv4Address addr, in_port_t port, uint8_t channel_id);

    void SetAttribute (std::string n1, const AttributeValue &v1);
    Ptr<NetDevice> Install (Ptr<Node> node, Ptr<NetDevice> nd);
    Ptr<NetDevice> Install (std::string node_name, Ptr<NetDevice> nd);
    Ptr<NetDevice> Install (Ptr<Node> node, std::string dev_name);
    Ptr<NetDevice> Install (std::string node_name, std::string dev_name);

private:
    ObjectFactory m_factory;
};

}

#endif /* DISTRIBUTED_BRIDGE_HELPER_H */

