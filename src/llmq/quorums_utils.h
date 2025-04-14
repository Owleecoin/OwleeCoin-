// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_LLMQ_QUORUMS_UTILS_H
#define SYSCOIN_LLMQ_QUORUMS_UTILS_H

#include <consensus/params.h>

#include <set>

#include <vector>
#include <random.h>
#include <sync.h>
#include <dbwrapper.h>
#include <chainparams.h>
#include <util/strencodings.h>

class CBlockIndex;
class CDeterministicMN;
using CDeterministicMNCPtr = std::shared_ptr<const CDeterministicMN>;
class CBLSPublicKey;
class CConnman;
namespace llmq
{


class CLLMQUtils
{
public:
    static bool IsV19Active(const int nHeight);
    static const CBlockIndex* V19ActivationIndex(const CBlockIndex* pindex);
    // includes members which failed DKG
    static std::vector<CDeterministicMNCPtr> GetAllQuorumMembers(const CBlockIndex* pindexQuorum);
    static uint256 BuildCommitmentHash(const uint256& blockHash, const std::vector<bool>& validMembers, const CBLSPublicKey& pubKey, const uint256& vvecHash);
    static uint256 BuildSignHash(const uint256& quorumHash, const uint256& id, const uint256& msgHash);

    // works for sig shares and recovered sigs
    template<typename T>
    static uint256 BuildSignHash(const T& s)
    {
        return BuildSignHash(s.quorumHash, s.id, s.msgHash);
    }

    static bool IsAllMembersConnectedEnabled();
    static bool IsQuorumPoseEnabled();
    static uint256 DeterministicOutboundConnection(const uint256& proTxHash1, const uint256& proTxHash2);
    static std::unordered_set<uint256, StaticSaltedHasher> GetQuorumConnections(const CBlockIndex* pQuorumBaseBlockIndex, const uint256& forMember, bool onlyOutbound);
    static std::unordered_set<uint256, StaticSaltedHasher> GetQuorumRelayMembers(const CBlockIndex* pQuorumBaseBlockIndex, const uint256& forMember, bool onlyOutbound);
    static std::set<size_t> CalcDeterministicWatchConnections(const CBlockIndex *pQuorumBaseBlockIndex, size_t memberCount, size_t connectionCount);

    static bool EnsureQuorumConnections(const CBlockIndex* pQuorumBaseBlockIndex, const uint256& myProTxHash, CConnman& connman);
    static void AddQuorumProbeConnections(const CBlockIndex* pQuorumBaseBlockIndex, const uint256& myProTxHash, CConnman& connman);

    static bool IsQuorumActive(const uint256& quorumHash);
    /// Returns the state of `-watchquorums`
    static bool IsWatchQuorumsEnabled();
    template<typename NodesContainer, typename Continue, typename Callback>
    static void IterateNodesRandom(NodesContainer& nodeStates, Continue&& cont, Callback&& callback, FastRandomContext& rnd)
    {
        std::vector<typename NodesContainer::iterator> rndNodes;
        rndNodes.reserve(nodeStates.size());
        for (auto it = nodeStates.begin(); it != nodeStates.end(); ++it) {
            rndNodes.emplace_back(it);
        }
        if (rndNodes.empty()) {
            return;
        }
        Shuffle(rndNodes.begin(), rndNodes.end(), rnd);

        size_t idx = 0;
        while (!rndNodes.empty() && cont()) {
            auto nodeId = rndNodes[idx]->first;
            auto& ns = rndNodes[idx]->second;

            if (callback(nodeId, ns)) {
                idx = (idx + 1) % rndNodes.size();
            } else {
                rndNodes.erase(rndNodes.begin() + idx);
                if (rndNodes.empty()) {
                    break;
                }
                idx %= rndNodes.size();
            }
        }
    }
    static std::string ToHexStr(const std::vector<bool>& vBits)
    {
        std::vector<uint8_t> vBytes((vBits.size() + 7) / 8);
        for (size_t i = 0; i < vBits.size(); i++) {
            vBytes[i / 8] |= vBits[i] << (i % 8);
        }
        return HexStr(vBytes);
    }

    static std::optional<std::vector<bool>> HexStrToBits(const std::string& hex, size_t expectedBits) {
        const auto vBytes{TryParseHex<uint8_t>(hex)};
        if (!vBytes) {
            return std::nullopt;
        }
        std::vector<bool> vBits(expectedBits);
        for (size_t i = 0; i < vBytes->size(); ++i) {
            for (size_t bit = 0; bit < 8; ++bit) {
                size_t bitIndex = i * 8 + bit;
                if (bitIndex < expectedBits) {
                    vBits[bitIndex] = ((*vBytes)[i] >> bit) & 1;
                }
            }
        }
        return vBits;
    }

};

} // namespace llmq

#endif // SYSCOIN_LLMQ_QUORUMS_UTILS_H
