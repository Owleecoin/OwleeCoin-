#!/usr/bin/env python3
# Copyright (c) 2015-2020 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import time
from test_framework.test_framework import DashTestFramework
from test_framework.util import force_finish_mnsync, assert_raises_rpc_error, assert_equal, wait_until_helper_internal
from test_framework.p2p import (
  P2PInterface,
)
from test_framework.messages import (
    msg_qsigshare,
    CSigShare
)

'''
feature_llmqsigning.py

Checks LLMQs signing sessions

'''

class LLMQSigningTest(DashTestFramework):
    def set_test_params(self):
        self.set_dash_test_params(6, 5, fast_dip3_enforcement=True)
        self.set_dash_llmq_test_params(5, 3)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_bdb()

    def add_options(self, parser):
        self.add_wallet_options(parser)
        parser.add_argument("--spork21", dest="spork21", default=False, action="store_true",
                        help="Test with spork21 enabled")

    def run_test(self):
        self.sync_blocks(self.nodes, timeout=60*5)
        for i in range(len(self.nodes)):
            force_finish_mnsync(self.nodes[i])
        self.nodes[0].spork("SPORK_17_QUORUM_DKG_ENABLED", 0)
        if self.options.spork21:
            self.nodes[0].spork("SPORK_21_QUORUM_ALL_CONNECTED", 0)
        self.wait_for_sporks_same()

        self.mine_quorum()
        if self.options.spork21:
            assert self.mninfo[0].node.getconnectioncount() == 5
        id = "0000000000000000000000000000000000000000000000000000000000000001"
        msgHash = self.generate(self.nodes[0], 5)[-1]
        msgHashConflict = self.generate(self.nodes[0], 5)[-1]

        def check_sigs(hasrecsigs, isconflicting1, isconflicting2):
            for mn in self.mninfo:
                if mn.node.quorum_hasrecsig(id, msgHash) != hasrecsigs:
                    return False
                if mn.node.quorum_isconflicting(id, msgHash) != isconflicting1:
                    return False
                if mn.node.quorum_isconflicting(id, msgHashConflict) != isconflicting2:
                    return False
            return True

        def wait_for_sigs(hasrecsigs, isconflicting1, isconflicting2, timeout):
            t = time.time()
            while time.time() - t < timeout:
                if check_sigs(hasrecsigs, isconflicting1, isconflicting2):
                    return
                self.bump_mocktime(2)
                time.sleep(1)
            raise AssertionError("wait_for_sigs timed out")

        def assert_sigs_nochange(hasrecsigs, isconflicting1, isconflicting2, timeout):
            t = time.time()
            while time.time() - t < timeout:
                assert check_sigs(hasrecsigs, isconflicting1, isconflicting2)
                time.sleep(0.1)

        # Initial state
        wait_for_sigs(False, False, False, 1)
        # Sign first share without any optional parameter, should not result in recovered sig
        self.mninfo[0].node.quorum_sign(id, msgHash)

        assert_sigs_nochange(False, False, False, 3)
        # Sign second share and test optional quorumHash parameter, should not result in recovered sig

        # 1. Providing an invalid quorum hash should fail and cause no changes for sigs
        assert not self.mninfo[1].node.quorum_sign(id, msgHash, msgHash)
        assert_sigs_nochange(False, False, False, 3)
        # 2. Providing a valid quorum hash should succeed and cause no changes for sigss
        quorumHash = self.mninfo[1].node.quorum_selectquorum(id)["quorumHash"]
        assert self.mninfo[1].node.quorum_sign(id, msgHash, quorumHash)
        assert_sigs_nochange(False, False, False, 3)
        # Sign third share and test optional submit parameter if spork21 is enabled, should result in recovered sig
        # and conflict for msgHashConflict
        if self.options.spork21:
            # 1. Providing an invalid quorum hash and set submit=false, should throw an error
            assert_raises_rpc_error(-8, 'quorum not found', self.mninfo[2].node.quorum_sign, id, msgHash, id, False)
            # 2. Providing a valid quorum hash and set submit=false, should return a valid sigShare object
            sig_share_rpc_1 = self.mninfo[2].node.quorum_sign(id, msgHash, quorumHash, False)
            sig_share_rpc_2 = self.mninfo[2].node.quorum_sign(id, msgHash, "", False)
            assert_equal(sig_share_rpc_1, sig_share_rpc_2)
            assert_sigs_nochange(False, False, False, 3)
            # 3. Sending the sig share received from RPC to the recovery member through P2P interface, should result
            # in a recovered sig
            sig_share = CSigShare()
            sig_share.quorumHash = int(sig_share_rpc_1["quorumHash"], 16)
            sig_share.quorumMember = int(sig_share_rpc_1["quorumMember"])
            sig_share.id = int(sig_share_rpc_1["id"], 16)
            sig_share.msgHash = int(sig_share_rpc_1["msgHash"], 16)
            sig_share.sigShare = bytes.fromhex(sig_share_rpc_1["signature"])
            for i in range(len(self.mninfo)):
                assert self.mninfo[i].node.getconnectioncount() == 5
            # Get the current recovery member of the quorum
            q = self.nodes[0].quorum_selectquorum(id)
            mn = self.get_mninfo(q['recoveryMembers'][0])
            # Open a P2P connection to it
            p2p_interface = mn.node.add_p2p_connection(P2PInterface())
            # Send the last required QSIGSHARE message to the recovery member
            p2p_interface.send_message(msg_qsigshare([sig_share]))
        else:
            # If spork21 is not enabled just sign regularly
            self.mninfo[2].node.quorum_sign(id, msgHash)

        wait_for_sigs(True, False, True, 15)

        self.bump_mocktime(5)
        wait_for_sigs(True, False, True, 15)
        if self.options.spork21:
            mn.node.disconnect_p2ps()
        # Test `quorum verify` rpc
        node = self.mninfo[0].node
        recsig = node.quorum_getrecsig(id, msgHash)
        # Find quorum automatically
        height = node.getblockcount()
        height_bad = node.getblockheader(recsig["quorumHash"])["height"]
        hash_bad = node.getblockhash(0)
        assert node.quorum_verify(id, msgHash, recsig["sig"])
        assert node.quorum_verify(id, msgHash, recsig["sig"], "", height)
        assert not node.quorum_verify(id, msgHashConflict, recsig["sig"])
        # will find the right quorum based on latching to dkgInterval from height_bad passed in to ScanQuorums
        assert node.quorum_verify(id, msgHash, recsig["sig"], "", height_bad)
        # Use specific quorum
        assert node.quorum_verify(id, msgHash, recsig["sig"], recsig["quorumHash"])
        assert not node.quorum_verify(id, msgHashConflict, recsig["sig"], recsig["quorumHash"])
        assert_raises_rpc_error(-8, "quorum not found", node.quorum_verify, id, msgHash, recsig["sig"], hash_bad)


        # Mine one more quorum, so that we have 2 active ones, nothing should change
        self.mine_quorum()
        assert_sigs_nochange(True, False, True, 3)

        # Create a recovered sig for the oldest quorum i.e. the active quorum which will be moved
        # out of the active set when a new quorum appears
        request_id = 2
        oldest_quorum_hash = node.quorum_list()["quorums"][-1]
        # Search for a request id which selects the last active quorum
        while True:
            selected_hash = node.quorum_selectquorum("%064x" % request_id)["quorumHash"]
            if selected_hash == oldest_quorum_hash:
                break
            else:
                request_id += 1
        # Produce the recovered signature
        id = "%064x" % request_id
        for mn in self.mninfo:
            mn.node.quorum_sign(id, msgHash)
        # And mine a quorum to move the quorum which signed out of the active set
        self.mine_quorum()
        # Verify the recovered sig. This triggers the "signHeight + dkgInterval" verification
        recsig = node.quorum_getrecsig(id, msgHash)
        assert node.quorum_verify(id, msgHash, recsig["sig"], "", node.getblockcount())

        recsig_time = self.mocktime

        # Mine 4 more quorums, so that the one used for the the recovered sig should become inactive, nothing should change
        self.mine_quorum()
        self.mine_quorum()
        self.mine_quorum()
        self.mine_quorum()
        assert_sigs_nochange(True, False, True, 3)

        # fast forward until 0.5 days before cleanup is expected, recovered sig should still be valid
        self.bump_mocktime(recsig_time + int(60 * 60 * 24 * 6.5) - self.mocktime)
        for i in range(len(self.nodes)):
            force_finish_mnsync(self.nodes[i])
        self.generate(self.nodes[0], 5)
        self.sync_blocks()
        self.bump_mocktime(5)
        # Cleanup starts every 5 seconds
        wait_for_sigs(True, False, True, 15)
        # fast forward 1 day, recovered sig should not be valid anymore
        self.bump_mocktime(int(60 * 60 * 24 * 1))
        for i in range(len(self.nodes)):
            force_finish_mnsync(self.nodes[i])
        self.generate(self.nodes[0], 5)
        self.sync_blocks()
        self.bump_mocktime(5)
        # Cleanup starts every 5 seconds
        wait_for_sigs(False, False, False, 15)
        for i in range(len(self.nodes)):
            force_finish_mnsync(self.nodes[i])
        for i in range(2):
            self.mninfo[i].node.quorum_sign(id, msgHashConflict)
        for i in range(2, 5):
            self.mninfo[i].node.quorum_sign(id, msgHash)
        self.generate(self.nodes[0], 5)
        self.sync_blocks()
        self.bump_mocktime(5)
        wait_for_sigs(True, False, True, 15)

        if self.options.spork21:
            id = "%064x" % (request_id + 1)

            # Isolate the node that is responsible for the recovery of a signature and assert that recovery fails
            q = self.nodes[0].quorum_selectquorum(id)
            mn = self.get_mninfo(q['recoveryMembers'][0])
            mn.node.setnetworkactive(False)
            self.wait_until(lambda: mn.node.getconnectioncount() == 0)
            for i in range(4):
                self.mninfo[i].node.quorum_sign(id, msgHash)
            assert_sigs_nochange(False, False, False, 3)
            # Need to re-connect so that it later gets the recovered sig
            mn.node.setnetworkactive(True)
            self.connect_nodes(mn.node.index, 0)
            force_finish_mnsync(mn.node)
            # Make sure intra-quorum connections were also restored
            self.bump_mocktime(1)  # need this to bypass quorum connection retry timeout
            wait_until_helper_internal(lambda: mn.node.getconnectioncount() == 5, timeout=10)
            mn.node.ping()
            self.wait_until(lambda: all('pingwait' not in peer for peer in mn.node.getpeerinfo()))
            self.generate_helper(self.nodes[0], 5)
            self.sync_blocks()
            self.bump_mocktime(5)
            wait_for_sigs(True, False, True, 15)

if __name__ == '__main__':
    LLMQSigningTest().main()
