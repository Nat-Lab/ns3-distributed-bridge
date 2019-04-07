#include "distributed-bridge.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("DistributedBridge");

FdReader::Data DistributedBridgeFdReader::DoRead () {
    payload_t *pl = new payload_t;

    ssize_t len = read (m_fd, &pl->payload_len, 2); // read header
    if (len <= 0) {
        NS_LOG_WARN ("DistributedBridgeFdReader::DoRead: read returned <= 0: " << len);
        std::free (pl);
        len = 0;
        pl = 0;
        return FdReader::Data ((uint8_t *) pl, len);
    }

    len = read(m_fd, &pl->payload, pl->payload_len);
    ssize_t left = pl->payload_len - len;

    while (left > 0) {
        NS_LOG_INFO ("DistributedBridgeFdReader::DoRead: imcomplete packet: " << left << "more bytes to read.");
        len = read(m_fd, (&pl->payload) + len, left);
        if (len < 0) {
            NS_LOG_WARN ("DistributedBridgeFdReader::DoRead: while completing packet, read() returned < 0: " << len);
            std::free (pl);
            len = 0;
            pl = 0;
            return FdReader::Data ((uint8_t *) pl, len);
        }
        left -= len;
    }

    NS_ASSERT_MSG(left == 0, "left != 0, internal error.");

    return FdReader::Data ((uint8_t *) pl, pl->payload_len + 2);
}

NS_OBJECT_ENSURE_REGISTERED(DistributedBridge);

TypeId DistributedBridge::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::DistributedBridge")
        .SetParent<NetDevice> ()
        .AddConstructor<DistributedBridge> ()
        .AddAttribute ("Mtu", "L2 MTU", UintegerValue (0), MakeUintegerAccessor(&DistributedBridge::SetMtu, &DistributedBridge::GetMtu), MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("Server", "Address of distribution server", Ipv4AddressValue ("127.0.0.1"), MakeIpv4AddressAccessor(&DistributedBridge::SetServerAddress, &DistributedBridge::GetServerAddress), MakeIpv4AddressChecker())
        .AddAttribute ("ServerPort", "Port of distribution server", UintegerValue (2672), MakeUintegerAccessor(&DistributedBridge::SetServerPort, &DistributedBridge::GetServerPort), MakeUintegerChecker<in_port_t> ())
        .AddAttribute ("ChannelID", "ID of channel to join", UintegerValue (0), MakeUintegerAccessor(&DistributedBridge::SetServerChannel, &DistributedBridge::GetServerChannel), MakeUintegerChecker<uint8_t> ());

    return tid;
}

DistributedBridge::DistributedBridge () : m_start_events (), m_stop_events () {
    m_node = 0;
    m_ifindex = 0;
    m_fd = -1;
    m_reader = 0;
    m_isup = false;
    m_connected = false;
    memset(&m_server_addr, 0, sizeof(struct sockaddr_in));
    memset(&m_pl, 0, sizeof(payload_t));
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(2672);
    inet_pton(AF_INET, "127.0.0.1", &m_server_addr.sin_addr);
}

DistributedBridge::~DistributedBridge () {
    DisconnetServer ();
    m_br_dev = 0;
}

void DistributedBridge::DoDispose () {
    NetDevice::Dispose();
}

void DistributedBridge::SetServer (const char *addr, in_port_t port, uint8_t channel_id) {
    m_channel_id = channel_id;
    m_server_addr.sin_port = htons(port);
    inet_pton(AF_INET, addr, &m_server_addr.sin_addr);
}

void DistributedBridge::SetServer (const Ipv4Address addr, in_port_t port, uint8_t channel_id) {
    m_channel_id = channel_id;
    m_server_addr.sin_port = htons(port);
    m_server_addr.sin_addr.s_addr = htonl(addr.Get());
}

void DistributedBridge::SetServerAddress (Ipv4Address addr) {
    m_server_addr.sin_addr.s_addr = htonl(addr.Get());
}

Ipv4Address DistributedBridge::GetServerAddress (void) const {
    return Ipv4Address (ntohl(m_server_addr.sin_addr.s_addr));
}

void DistributedBridge::SetServerPort (in_port_t port) {
    m_server_addr.sin_port = htons(port);
}

in_port_t DistributedBridge::GetServerPort (void) const {
    return ntohs(m_server_addr.sin_port);
}

void DistributedBridge::SetServerChannel (uint8_t channel_id) {
    m_channel_id = channel_id;
}

uint8_t DistributedBridge::GetServerChannel (void) const {
    return m_channel_id;
}

bool DistributedBridge::ConnectServer () {
    if (m_connected) {
        NS_LOG_WARN("DistributedBridge::ConnectServer: already connected.");
        return false;
    }

    if (m_fd > 0) {
        NS_FATAL_ERROR("DistributedBridge::ConnectServer: m_fd > 0, but m_connected false.");
        return false;
    }

    m_nodeid = GetNode ()->GetId ();

    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    NS_ABORT_MSG_IF(m_fd < 0, "socket() failed.");

    int nodelay = 1;
    int sso_ret = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
    if (sso_ret < 0) {
        NS_LOG_WARN("DistributedBridge::ConnectServer: setsockopt() failed: " << strerror(errno));
        close(m_fd);
        m_fd = -1;
        return false;
    }

    int conn_ret = connect(m_fd, (struct sockaddr *) &m_server_addr, sizeof(struct sockaddr_in));
    if (conn_ret < 0) {
        NS_LOG_WARN("DistributedBridge::ConnectServer: connect() failed: " << strerror(errno));
        close(m_fd);
        m_fd = -1;
        return false;
    }

    handshake_msg_t hs;
    hs.id = m_channel_id;
    ssize_t w_len = write(m_fd, &hs, sizeof(handshake_msg_t));
    if (w_len < 0) {
        NS_LOG_WARN("DistributedBridge::ConnectServer: handshake write() failed: " << strerror(errno));
        close(m_fd);
        m_fd = -1;
        return false;
    }
    if ((size_t) w_len != sizeof(handshake_msg_t)) {
        NS_LOG_WARN("DistributedBridge::ConnectServer: handshake write() return value != sizeof(handshake_msg_t).");
        close(m_fd);
        m_fd = -1;
        return false;
    }

    m_reader = Create<DistributedBridgeFdReader> ();
    m_reader->Start(m_fd, MakeCallback (&DistributedBridge::ReadCallback, this));

    NotifyLinkUp ();

    NS_LOG_WARN("DistributedBridge::ConnectServer: connected.");

    m_connected = true;
    return true;
}

void DistributedBridge::DisconnetServer () {
    if (!m_connected) return;

    if (m_reader) {
        m_reader->Stop();
        m_reader->Unref();
    }

    if (m_fd > 0) {
        close(m_fd);
        m_fd = -1;
    }

    m_connected = false;

    NS_LOG_WARN("DistributedBridge::DisconnetServer: disconnected.");
}

bool DistributedBridge::IsServerConnected (void) const {
    return m_connected;
}

void DistributedBridge::ReadCallback (uint8_t *buf, ssize_t len) {
    NS_ASSERT_MSG (buf != 0, "invalid buffer.");

    if (len < 2) {
        NS_LOG_WARN ("DistributedBridge::ReadCallback: invalid packet (size too small).");
        std::free (buf);
        return;
    }

    payload_t *pl = (payload_t *) buf;
    if (len - 2 < pl->payload_len) {
        NS_LOG_WARN ("DistributedBridge::ReadCallback: invalid packet (payload len=" << pl->payload_len << ", but buf_size=" << len << ").");
        std::free (buf);
        return;
    }

    NS_LOG_INFO ("DistributedBridge::ReadCallback: scheduling forward on node " << m_nodeid);
    Simulator::ScheduleWithContext (m_nodeid, Seconds (0.), MakeEvent (&DistributedBridge::ForwardToBridgedDevice, this, buf + 2, len - 2));
}

void DistributedBridge::ForwardToBridgedDevice (uint8_t *buf, ssize_t len) {
    Ptr<Packet> packet = Create<Packet> (reinterpret_cast<const uint8_t *> (buf), len);
    std::free (buf - 2);
    buf = 0;
    EthernetHeader eth (false);
    if (packet->GetSize() < eth.GetSerializedSize()) {
        packet->Unref();
        return;
    }
    packet->RemoveHeader(eth);
    Address src = eth.GetSource();
    Address dst = eth.GetDestination();
    uint16_t ethtype = eth.GetLengthType();
    if (ethtype <= 1500) {
        LlcSnapHeader llc;
        if (packet->GetSize() < llc.GetSerializedSize()) {
            packet->Unref();
            return;
        }
        packet->RemoveHeader(llc);
        ethtype = llc.GetType();
    }
    NS_LOG_INFO("DistributedBridge::ForwardToBridgedDevice:" << src << " > " << dst);
    m_br_dev->SendFrom(packet, src, dst, ethtype);
}

Ptr<NetDevice> DistributedBridge::GetBridgedNetDevice () {
    return m_br_dev;
}

void DistributedBridge::SetBridgedNetDevice (Ptr<NetDevice> dev) {
    NS_ASSERT_MSG (m_br_dev == 0, "DistributedBridge::SetBridgedDevice: already bridged.");
    NS_ASSERT_MSG (m_node != 0, "DistributedBridge::SetBridgedDevice: not assoc w/ any node.");
    NS_ASSERT_MSG (dev != this, "DistributedBridge::SetBridgedDevice: can't bridge self.");
    NS_ASSERT_MSG (dev->GetNode() == m_node, "DistributedBridge::SetBridgedDevice: bridged netdevice not assoc w/ bridge node.");
    NS_ABORT_MSG_UNLESS(Mac48Address::IsMatchingType (dev->GetAddress ()), "invalid bridge device, EUI48 not supported.");
    NS_ABORT_MSG_UNLESS(dev->SupportsSendFrom (), "invalid bridge device, SendFrom not supported.");

    dev->SetPromiscReceiveCallback (MakeCallback (&DistributedBridge::ReceiveFromBridgedDevice, this));
    dev->SetReceiveCallback (MakeCallback (&DistributedBridge::DiscardFromBridgedDevice, this));
    m_br_dev = dev;
}

bool DistributedBridge::DiscardFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &src) {
    NS_LOG_INFO("DistributedBridge: send pkt from br-dev to void.");
    return true;
}

bool DistributedBridge::ReceiveFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, Address const &src, Address const &dst, PacketType packetType) {
    if (!m_connected) {
        NS_LOG_WARN("DistributedBridge::ReceiveFromBridgedDevice: server not connected.");
        return false;
    }

    Mac48Address from = Mac48Address::ConvertFrom (src);
    Mac48Address to = Mac48Address::ConvertFrom (dst);

    Ptr<Packet> pkt = packet->Copy ();
    EthernetHeader eth (false);
    eth.SetSource (from);
    eth.SetDestination (to);
    eth.SetLengthType (protocol);
    pkt->AddHeader (eth);

    NS_ASSERT_MSG (pkt->GetSize () <= 65536, "pkt too big.");
    pkt->CopyData(m_pl.payload, pkt->GetSize());
    m_pl.payload_len = pkt->GetSize();
    NS_LOG_INFO("DistributedBridge: forwarding traffic to server.");
    ssize_t w_len = write (m_fd, &m_pl, pkt->GetSize() + 2);
    NS_ABORT_MSG_IF(w_len < 0, "write() failed");
    NS_ABORT_MSG_IF(w_len != pkt->GetSize() + 2, "wrote != pkt_size");

    return true;
}

void DistributedBridge::SetIfIndex (const uint32_t index) {
    m_ifindex = index;
}

uint32_t DistributedBridge::GetIfIndex (void) const {
    return m_ifindex;
}

Ptr<Channel> DistributedBridge::GetChannel (void) const {
    return 0;
}

void DistributedBridge::SetAddress (Address address) {
    m_addr = Mac48Address::ConvertFrom (address);
}

Address DistributedBridge::GetAddress (void) const {
    return m_addr;
}

bool DistributedBridge::SetMtu (const uint16_t mtu) {
    m_mtu = mtu;
    return true;
}

uint16_t DistributedBridge::GetMtu (void) const {
    return m_mtu;
}

void DistributedBridge::NotifyLinkUp (void) {
    if (!m_isup) {
        m_isup = true;
        m_lccbs ();
    }
}

bool DistributedBridge::IsLinkUp (void) const {
    return m_isup;
}

void DistributedBridge::AddLinkChangeCallback (Callback<void> callback) {
    m_lccbs.ConnectWithoutContext (callback);
}

bool DistributedBridge::IsBroadcast (void) const {
    return true;
}

Address DistributedBridge::GetBroadcast (void) const {
    return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool DistributedBridge::IsMulticast (void) const {
    return true;
}

Address DistributedBridge::GetMulticast (Ipv4Address mg) const {
    Mac48Address multicast = Mac48Address::GetMulticast (mg);
    return multicast;
}

bool DistributedBridge::IsPointToPoint (void) const {
    return false;
}

bool DistributedBridge::IsBridge (void) const {
    return false;
}

bool DistributedBridge::Send (Ptr<Packet> packet, const Address& dst, uint16_t protocol) {
    NS_LOG_WARN("DistributedBridge::Send: do not call Send() on bridge, call on bridged device instead.");
    return false;
}

bool DistributedBridge::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dst, uint16_t protocol) {
    NS_LOG_WARN("DistributedBridge::SendFrom: do not call SendFrom() on bridge, call on bridged device instead.");
    return false;
}

void DistributedBridge::SetNode (Ptr<Node> node) {
    m_node = node;
}

Ptr<Node> DistributedBridge::GetNode (void) const {
    return m_node;
}

bool DistributedBridge::NeedsArp (void) const {
    return true;
} 

void DistributedBridge::SetReceiveCallback (NetDevice::ReceiveCallback cb) {
    return;
}

void DistributedBridge::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb) {
    return;
}

bool DistributedBridge::SupportsSendFrom (void) const {
    return true;
}

Address DistributedBridge::GetMulticast (Ipv6Address addr) const {
    return Mac48Address::GetMulticast (addr);
}

}