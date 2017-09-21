#include "test_common.h"

#include <net/Address.h>

#include "../MakeCnt.h"
#include "../NetConfig.h"

void test_make_cnt() {
    using namespace std;
    using namespace net;

    {
        // Test make/parse containers
        string username = "user1", nonce = "sdfsfdsfs", pass_hash = "asdasd";
        Address local_addr = { 1, 2, 3, 4, 5555 };

        {
            // Handshake
            VarContainer cnt = MakeHandshakeCnt(username, local_addr);

            string _username;
            Address _local_addr;
            assert(ParseHandshakeCnt(cnt, _username, _local_addr));
            assert(_username == username);
            assert(_local_addr == local_addr);

            cnt = MakeHandshakeRespCnt(1, "123456");

            string nonce;
            uint32_t auth_res;
            assert(ParseHandshakeRespCnt(cnt, auth_res, nonce));
            assert(auth_res == 1);
            assert(nonce == "123456");
        }

        {
            // Auth1
            VarContainer cnt = MakeAuthorizationCnt(username, nonce, pass_hash, false);

            string _username, _nonce, _pass_hash;
            uint32_t _auth_op;
            assert(ParseAuthorizationCnt(cnt, _username, _auth_op, _nonce, _pass_hash));
            assert(_username == username);
            assert(_auth_op == OP_AUTHORIZE);
            assert(_nonce == nonce);
            assert(_pass_hash == pass_hash);
        }
    }
}
