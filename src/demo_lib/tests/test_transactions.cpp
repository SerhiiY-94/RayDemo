#include "test_common.h"

#include <tuple>

#include "../TransactionManager.cpp"

void test_transactions() {
    std::tuple<std::string, net::Packet, net::Address> outgoing_packets[] = {
        std::make_tuple("111", net::Packet{}, net::Address{ 1, 2, 3, 4, 30000 }),
        std::make_tuple("222", net::Packet{}, net::Address{ 1, 2, 3, 5, 30000 }),
        std::make_tuple("333", net::Packet{}, net::Address{ 1, 2, 3, 6, 30000 })
    };

    auto find_out_pack = [&outgoing_packets](const char *id, const net::Packet *p, const net::Address &addr) {
        return std::find_if(std::begin(outgoing_packets), std::end(outgoing_packets),
        [id, p, addr](const std::tuple<std::string, net::Packet, net::Address> &pack) {
            return std::get<0>(pack) == id &&
                   std::get<1>(pack) == *p &&
                   std::get<2>(pack) == addr;
        });
    };

    TransactionManager t;

    for (const auto &p : outgoing_packets) {
        t.PutRequest(std::get<0>(p), std::get<1>(p), std::get<2>(p), 500, 30000);
    }

    for (const auto &p : outgoing_packets) {
        const char *id;
        std::tie(id, std::ignore, std::ignore) = t.GetRequest(std::get<0>(p), std::get<2>(p));

        // should keep all packets
        assert(id);
    }

    // skip to retransmit stage
    t.Update(500);

    {
        // check retransmit for packet 0
        const char *id = nullptr;
        const net::Packet *p = nullptr;
        net::Address addr;

        std::tie(id, p, addr) = t.GetPackForRetransmit();

        auto it = find_out_pack(id, p, addr);
        assert(it != std::end(outgoing_packets));
    }

    {
        // check ack for packet 0
        std::string _id = std::get<0>(outgoing_packets[0]);
        net::Address _addr = std::get<2>(outgoing_packets[0]);
        t.Answer(_id, _addr);

        const char *id = nullptr;
        std::tie(id, std::ignore, std::ignore) = t.GetRequest(_id, _addr);
        assert(id == nullptr);
    }

    {
        // check resransmit for packet 1
        const char *id = nullptr;
        const net::Packet *p = nullptr;
        net::Address addr;

        std::tie(id, p, addr) = t.GetPackForRetransmit();

        auto it = find_out_pack(id, p, addr);
        assert(it != std::end(outgoing_packets));
    }

    {
        // check ack for packet 1
        std::string _id = std::get<0>(outgoing_packets[0]);
        net::Address _addr = std::get<2>(outgoing_packets[0]);
        t.Answer(_id, _addr);

        const char *id = nullptr;
        std::tie(id, std::ignore, std::ignore) = t.GetRequest(_id, _addr);
        assert(id == nullptr);
    }

    // skip to timeout
    t.Update(30000);

    {
        // check if packet 2 is timed out
        std::string _id = std::get<0>(outgoing_packets[2]);
        net::Address _addr = std::get<2>(outgoing_packets[2]);

        const char *id = nullptr;
        std::tie(id, std::ignore, std::ignore) = t.GetRequest(_id, _addr);
        assert(id == nullptr);
    }
}