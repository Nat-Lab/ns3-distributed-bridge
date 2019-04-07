#ifndef DISTRIBUTED_BRIDGE_H
#define DISTRIBUTED_BRIDGE_H

#include "dist-types.h"

#include "ns3/net-device.h"
#include "ns3/event-id.h"
#include "ns3/mac48-address.h"
#include "ns3/unix-fd-reader.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/uinteger.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/ethernet-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/traced-callback.h"
#include "ns3/channel.h"

#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ns3 {

class DistributedBridgeFdReader : public FdReader {
private:
    FdReader::Data DoRead ();
};

class DistributedBridge : public NetDevice {
public:
    static TypeId GetTypeId (void);

    DistributedBridge();
    virtual ~DistributedBridge();

    void SetServer (const char *addr, in_port_t port, uint8_t channel_id);
    void SetServer (const Ipv4Address addr, in_port_t port, uint8_t channel_id);
    void SetServerAddress (const Ipv4Address addr);
    Ipv4Address GetServerAddress (void) const;
    void SetServerPort (in_port_t port);
    in_port_t GetServerPort (void) const;
    void SetServerChannel (uint8_t channel_id);
    uint8_t GetServerChannel (void) const;
    bool ConnectServer ();
    void DisconnetServer ();
    bool IsServerConnected (void) const;

    Ptr<NetDevice> GetBridgedNetDevice (void);
    void SetBridgedNetDevice (Ptr<NetDevice> dev);

    // inherited
    virtual void SetIfIndex (const uint32_t index);
    virtual uint32_t GetIfIndex (void) const;
    virtual Ptr<Channel> GetChannel (void) const;
    virtual void SetAddress (Address address);
    virtual Address GetAddress (void) const;
    virtual bool SetMtu (const uint16_t mtu);
    virtual uint16_t GetMtu (void) const;
    virtual bool IsLinkUp (void) const;
    virtual void AddLinkChangeCallback (Callback<void> callback);
    virtual bool IsBroadcast (void) const;
    virtual Address GetBroadcast (void) const;
    virtual bool IsMulticast (void) const;
    virtual Address GetMulticast (Ipv4Address multicastGroup) const;
    virtual bool IsPointToPoint (void) const;
    virtual bool IsBridge (void) const;
    virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
    virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
    virtual Ptr<Node> GetNode (void) const;
    virtual void SetNode (Ptr<Node> node);
    virtual bool NeedsArp (void) const;
    virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
    virtual void SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb);
    virtual bool SupportsSendFrom (void) const;
    virtual Address GetMulticast (Ipv6Address addr) const;

protected:
    virtual void DoDispose (void);
    bool ReceiveFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, Address const &src, Address const &dst, PacketType packetType);
    bool DiscardFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, Address const &src);

private:
    void ReadCallback (uint8_t *buf, ssize_t len);
    void ForwardToBridgedDevice (uint8_t *buf, ssize_t len);
    void NotifyLinkUp (void);

    Ptr<NetDevice> m_br_dev;
    Ptr<Node> m_node;
    Ptr<DistributedBridgeFdReader> m_reader;

    int m_fd;
    struct sockaddr_in m_server_addr;
    struct payload_t m_pl;

    uint8_t m_channel_id;
    uint32_t m_ifindex;
    uint16_t m_mtu;
    uint32_t m_nodeid;

    Mac48Address m_addr;
    
    bool m_isup;
    bool m_connected;
    
    NetDevice::ReceiveCallback m_rcb;
    NetDevice::PromiscReceiveCallback m_prcb;

    TracedCallback<> m_lccbs;
};

}

#endif /* DISTRIBUTED_BRIDGE_H */

