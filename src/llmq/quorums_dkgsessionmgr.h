// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_LLMQ_QUORUMS_DKGSESSIONMGR_H
#define SYSCOIN_LLMQ_QUORUMS_DKGSESSIONMGR_H

#include <llmq/quorums_dkgsessionhandler.h>
#include <llmq/quorums_dkgsession.h>
#include <bls/bls.h>
#include <bls/bls_worker.h>
class CConnman;
class UniValue;
class CDBWrapper;
class PeerManager;
class CBlockIndex;
class ChainstateManager;
namespace llmq
{

class CDKGSessionManager
{
    static constexpr int64_t MAX_CONTRIBUTION_CACHE_TIME = 60 * 1000;

private:
    std::unique_ptr<CDBWrapper> db{nullptr};
    std::unique_ptr<CDKGSessionHandler> dkgSessionHandler{nullptr};

    mutable Mutex contributionsCacheCs;
    struct ContributionsCacheKey {
        uint256 quorumHash;
        uint256 proTxHash;
        bool operator<(const ContributionsCacheKey& r) const
        {
            if (quorumHash != r.quorumHash) return quorumHash < r.quorumHash;
            return proTxHash < r.proTxHash;
        }
    };
    struct ContributionsCacheEntry {
        int64_t entryTime;
        BLSVerificationVectorPtr vvec;
        CBLSSecretKey skContribution;
    };
    mutable std::map<ContributionsCacheKey, ContributionsCacheEntry> contributionsCache GUARDED_BY(contributionsCacheCs);

public:
    CConnman& connman;
    PeerManager& peerman;
    CDKGSessionManager(CBLSWorker& _blsWorker, CConnman &connman, PeerManager& peerman, ChainstateManager& chainman, bool unitTests, bool fWipe);
   ~CDKGSessionManager() = default;

    void StartThreads();
    void StopThreads();

    void UpdatedBlockTip(const CBlockIndex *pindexNew, bool fInitialDownload) EXCLUSIVE_LOCKS_REQUIRED(!contributionsCacheCs);

    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv);
    bool AlreadyHave(const uint256& hash) const;
    bool GetContribution(const uint256& hash, CDKGContribution& ret) const;
    bool GetComplaint(const uint256& hash, CDKGComplaint& ret) const;
    bool GetJustification(const uint256& hash, CDKGJustification& ret) const;
    bool GetPrematureCommitment(const uint256& hash, CDKGPrematureCommitment& ret) const;

    // Verified contributions are written while in the DKG
    void WriteVerifiedVvecContribution(const uint256& hashQuorum, const uint256& proTxHash, const BLSVerificationVectorPtr& vvec);
    void WriteVerifiedSkContribution(const uint256& hashQuorum, const uint256& proTxHash, const CBLSSecretKey& skContribution);
    bool GetVerifiedContributions(const CBlockIndex* pQuorumBaseBlockIndex, const std::vector<bool>& validMembers, std::vector<uint16_t>& memberIndexesRet, std::vector<BLSVerificationVectorPtr>& vvecsRet, std::vector<CBLSSecretKey>& skContributionsRet) const EXCLUSIVE_LOCKS_REQUIRED(!contributionsCacheCs);
    void CleanupOldContributions(ChainstateManager& chainstate) const;
private:
    void CleanupCache() const EXCLUSIVE_LOCKS_REQUIRED(!contributionsCacheCs);
};

bool IsQuorumDKGEnabled();

extern CDKGSessionManager* quorumDKGSessionManager;

} // namespace llmq

#endif // SYSCOIN_LLMQ_QUORUMS_DKGSESSIONMGR_H
