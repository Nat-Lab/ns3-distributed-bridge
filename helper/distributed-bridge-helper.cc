#include "distributed-bridge-helper.h"

namespace ns3 {

DistributedBridgeHelper::DistributedBridgeHelper () {
    m_factory.SetTypeId ("ns3::DistributedBridge");
}

DistributedBridgeHelper::DistributedBridgeHelper (const char *addr, in_port_t port, uint8_t channel_id) : DistributedBridgeHelper () {
    SetAttribute ("Server", Ipv4AddressValue (addr));
    SetAttribute ("ServerPort", UintegerValue (port));
    SetAttribute ("ChannelID", UintegerValue (channel_id));
}

DistributedBridgeHelper::DistributedBridgeHelper (const Ipv4Address addr, in_port_t port, uint8_t channel_id) : DistributedBridgeHelper () {
    SetAttribute ("Server", Ipv4AddressValue (addr));
    SetAttribute ("ServerPort", UintegerValue (port));
    SetAttribute ("ChannelID", UintegerValue (channel_id));
}

void DistributedBridgeHelper::SetAttribute (std::string n1, const AttributeValue &v1) {
    m_factory.Set (n1, v1);
}

Ptr<NetDevice> DistributedBridgeHelper::Install (Ptr<Node> node, Ptr<NetDevice> nd) {
    Ptr<DistributedBridge> br = m_factory.Create<DistributedBridge> ();
    node->AddDevice (br);
    br->SetBridgedNetDevice (nd);

    return br;
}

Ptr<NetDevice> DistributedBridgeHelper::Install (std::string node_name, Ptr<NetDevice> nd) {
    return Install(Names::Find<Node> (node_name), nd);
}

Ptr<NetDevice> DistributedBridgeHelper::Install (Ptr<Node> node, std::string dev_name) {
    return Install (node, Names::Find<NetDevice> (dev_name));
}

Ptr<NetDevice> DistributedBridgeHelper::Install (std::string node_name, std::string dev_name) {
    return Install (Names::Find<Node> (node_name), Names::Find<NetDevice> (dev_name));
}

}