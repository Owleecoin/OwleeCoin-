// Copyright (c) 2014-2020 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <masternode/masternodeutils.h>
#include <evo/deterministicmns.h>

#include <init.h>
#include <masternode/masternodesync.h>
#include <net.h>
#include <validation.h>
#include <shutdown.h>
#include <logging.h>
struct CompareScoreMN
{
    bool operator()(const std::pair<arith_uint256, const CDeterministicMNCPtr&>& t1,
                    const std::pair<arith_uint256, const CDeterministicMNCPtr&>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->collateralOutpoint < t2.second->collateralOutpoint);
    }
};

void CMasternodeUtils::ProcessMasternodeConnections(CConnman& connman)
{
    // Don't disconnect masternode connections when we have less then the desired amount of outbound nodes
    size_t nonMasternodeCount = 0;
    connman.ForEachNode(AllNodes, [&](CNode* pnode) {
        if ((!pnode->IsInboundConn() &&
            !pnode->IsFeelerConn() &&
            !pnode->IsManualConn() &&
            !pnode->IsMasternodeConnection() &&
            !pnode->m_masternode_probe_connection) ||
            // treat unverified MNs as non-MNs here
            pnode->GetVerifiedProRegTxHash().IsNull()) {
            nonMasternodeCount++;
        }
    });
    if (nonMasternodeCount < connman.GetMaxOutboundNodeCount()) {
        return;
    }

    connman.ForEachNode(AllNodes, [&](CNode* pnode) {
        if (pnode->m_masternode_probe_connection) {
            // we're not disconnecting masternode probes for at least PROBE_WAIT_INTERVAL seconds
            if (GetTime<std::chrono::seconds>() - pnode->m_connected < PROBE_WAIT_INTERVAL) return;
        } else {
            // we're only disconnecting m_masternode_connection connections
            if (!pnode->IsMasternodeConnection()) return;
            if (!pnode->GetVerifiedProRegTxHash().IsNull()) {
                // keep _verified_ LLMQ connections
                if (connman.IsMasternodeQuorumNode(pnode)) {
                    return;
                }
                // keep _verified_ LLMQ relay connections
                if (connman.IsMasternodeQuorumRelayMember(pnode->GetVerifiedProRegTxHash())) {
                    return;
                }
                // keep _verified_ inbound connections
                if (pnode->IsInboundConn()) {
                    return;
                }
            } else if (GetTime<std::chrono::seconds>() - pnode->m_connected < PROBE_WAIT_INTERVAL) {
                // non-verified, give it some time to verify itself
                return;
            } else if (pnode->qwatch) {
                // keep watching nodes
                return;
            }
        }
        if (fLogIPs) {
            LogPrint(BCLog::NET_NETCONN, "Closing Masternode connection: peer=%d, addr=%s\n", pnode->GetId(), pnode->addr.ToStringAddr());
        } else {
            LogPrint(BCLog::NET_NETCONN, "Closing Masternode connection: peer=%d\n", pnode->GetId());
        }
        pnode->fDisconnect = true;
    });

}

void CMasternodeUtils::DoMaintenance(CConnman& connman)
{
    if(!masternodeSync.IsBlockchainSynced() || ShutdownRequested())
        return;

    static unsigned int nTick = 0;

    nTick++;

    if(nTick % 60 == 0) {
        ProcessMasternodeConnections(connman);
    }
}

