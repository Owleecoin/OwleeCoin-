// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/quorums_dkgsessionmgr.h>
#include <llmq/quorums_debug.h>
#include <llmq/quorums_utils.h>

#include <evo/deterministicmns.h>

#include <chainparams.h>
#include <net_processing.h>
#include <spork.h>
#include <validation.h>
#include <common/args.h>
namespace llmq
{

CDKGSessionManager* quorumDKGSessionManager;

static const std::string DB_VVEC = "qdkg_V";
static const std::string DB_SKCONTRIB = "qdkg_S";

CDKGSessionManager::CDKGSessionManager(CBLSWorker& blsWorker, CConnman &_connman, PeerManager& _peerman, ChainstateManager& _chainman, bool unitTests, bool fWipe) :
    connman(_connman),
    peerman(_peerman)
{
    db = std::make_unique<CDBWrapper>(DBParams{
        .path = gArgs.GetDataDirNet() / "llmq/dkgdb",
        // SYSCOIN use 64MB cache for vvecs
        .cache_bytes = static_cast<size_t>(1 << 26),
        .memory_only = unitTests,
        .wipe_data = fWipe});
    dkgSessionHandler = std::make_unique<CDKGSessionHandler>(
        blsWorker, 
        *this, 
        _peerman, 
        _chainman
    );
}

void CDKGSessionManager::StartThreads()
{
    if (!fMasternodeMode && !CLLMQUtils::IsWatchQuorumsEnabled()) {
        // Regular nodes do not care about any DKG internals, bail out
        return;
    }
    dkgSessionHandler->StartThread();
}

void CDKGSessionManager::StopThreads()
{
    if (!fMasternodeMode && !CLLMQUtils::IsWatchQuorumsEnabled()) {
        // Regular nodes do not care about any DKG internals, bail out
        return;
    }
    dkgSessionHandler->StopThread();
}

void CDKGSessionManager::UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload)
{
    CleanupCache();

    if (fInitialDownload)
        return;
    if (!deterministicMNManager || !deterministicMNManager->IsDIP3Enforced(pindexNew->nHeight))
        return;
    if (!IsQuorumDKGEnabled())
        return;

    dkgSessionHandler->UpdatedBlockTip(pindexNew);
    
}

void CDKGSessionManager::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    if (!IsQuorumDKGEnabled())
        return;

    if (strCommand != NetMsgType::QCONTRIB
        && strCommand != NetMsgType::QCOMPLAINT
        && strCommand != NetMsgType::QJUSTIFICATION
        && strCommand != NetMsgType::QPCOMMITMENT
        && strCommand != NetMsgType::QWATCH) {
        return;
    }
    PeerRef peer = peerman.GetPeerRef(pfrom->GetId());
    if (strCommand == NetMsgType::QWATCH) {
        if (!fMasternodeMode) {
            // non-masternodes should never receive this
            if(peer)
                peerman.Misbehaving(*peer, 10, "Non-MN cannot recv qwatch");
            return;
        }
        pfrom->qwatch = true;
        return;
    }
    if ((!fMasternodeMode && !llmq::CLLMQUtils::IsWatchQuorumsEnabled())) {
        // regular non-watching nodes should never receive any of these
        if(peer)
            peerman.Misbehaving(*peer, 10, "Non-watcher cannot recv qwatch");
        return;
    }
    if (vRecv.empty()) {
        if(peer)
            peerman.Misbehaving(*peer, 100, "invalid recv size for DKG session");
        return;
    }

    dkgSessionHandler->ProcessMessage(pfrom, strCommand, vRecv);
}

bool CDKGSessionManager::AlreadyHave(const uint256& hash) const
{
    if (!IsQuorumDKGEnabled())
        return false;
    
    
    if (dkgSessionHandler->pendingContributions.HasSeen(hash)
        || dkgSessionHandler->pendingComplaints.HasSeen(hash)
        || dkgSessionHandler->pendingJustifications.HasSeen(hash)
        || dkgSessionHandler->pendingPrematureCommitments.HasSeen(hash)) {
        return true;
    }
    
    return false;
}

bool CDKGSessionManager::GetContribution(const uint256& hash, CDKGContribution& ret) const
{
    if (!IsQuorumDKGEnabled())
        return false;

    
    LOCK(dkgSessionHandler->cs_phase_qhash);
    if (dkgSessionHandler->phase < QuorumPhase_Initialized || dkgSessionHandler->phase > QuorumPhase_Contribute) {
        return false;
    }
    if(dkgSessionHandler->GetContribution(hash, ret)) {
        return true;
    }
    return false;
}

bool CDKGSessionManager::GetComplaint(const uint256& hash, CDKGComplaint& ret) const
{
    if (!IsQuorumDKGEnabled())
        return false;

    
    LOCK(dkgSessionHandler->cs_phase_qhash);
    if (dkgSessionHandler->phase < QuorumPhase_Contribute || dkgSessionHandler->phase > QuorumPhase_Complain) {
        return false;
    }
    if(dkgSessionHandler->GetComplaint(hash, ret)) {
        return true;
    }
    return false;
}

bool CDKGSessionManager::GetJustification(const uint256& hash, CDKGJustification& ret) const
{
    if (!IsQuorumDKGEnabled())
        return false;

    
    LOCK(dkgSessionHandler->cs_phase_qhash);
    if (dkgSessionHandler->phase < QuorumPhase_Complain || dkgSessionHandler->phase > QuorumPhase_Justify) {
        return false;
    }
    if(dkgSessionHandler->GetJustification(hash, ret)) {
        return true;
    }
    return false;
}

bool CDKGSessionManager::GetPrematureCommitment(const uint256& hash, CDKGPrematureCommitment& ret) const
{
    if (!IsQuorumDKGEnabled())
        return false;



    LOCK(dkgSessionHandler->cs_phase_qhash);
    if (dkgSessionHandler->phase < QuorumPhase_Justify || dkgSessionHandler->phase > QuorumPhase_Commit) {
        return false;
    }
    if(dkgSessionHandler->GetPrematureCommitment(hash, ret)) {
        return true;
    }
    return false;
}

void CDKGSessionManager::WriteVerifiedVvecContribution(const uint256& hashQuorum, const uint256& proTxHash, const BLSVerificationVectorPtr& vvec)
{
    db->Write(std::make_tuple(DB_VVEC, hashQuorum, proTxHash), *vvec);
}

void CDKGSessionManager::WriteVerifiedSkContribution(const uint256& hashQuorum, const uint256& proTxHash, const CBLSSecretKey& skContribution)
{
    db->Write(std::make_tuple(DB_SKCONTRIB, hashQuorum, proTxHash), skContribution);
}

bool CDKGSessionManager::GetVerifiedContributions(const CBlockIndex* pQuorumBaseBlockIndex, const std::vector<bool>& validMembers, std::vector<uint16_t>& memberIndexesRet, std::vector<BLSVerificationVectorPtr>& vvecsRet, std::vector<CBLSSecretKey>& skContributionsRet) const
{
    auto members = CLLMQUtils::GetAllQuorumMembers(pQuorumBaseBlockIndex);

    memberIndexesRet.clear();
    vvecsRet.clear();
    skContributionsRet.clear();
    memberIndexesRet.reserve(members.size());
    vvecsRet.reserve(members.size());
    skContributionsRet.reserve(members.size());
    // NOTE: the `cs_main` should not be locked under scope of `contributionsCacheCs`
    LOCK(contributionsCacheCs);
    for (size_t i = 0; i < members.size(); i++) {
        if (validMembers[i]) {
            const uint256& proTxHash = members[i]->proTxHash;
            ContributionsCacheKey cacheKey = {pQuorumBaseBlockIndex->GetBlockHash(), proTxHash};
            auto it = contributionsCache.find(cacheKey);
            if (it == contributionsCache.end()) {
                auto vvecPtr = std::make_shared<std::vector<CBLSPublicKey>>();
                CBLSSecretKey skContribution;
                if (!db->Read(std::make_tuple(DB_VVEC, pQuorumBaseBlockIndex->GetBlockHash(), proTxHash), *vvecPtr)) {
                    return false;
                }
                db->Read(std::make_tuple(DB_SKCONTRIB, pQuorumBaseBlockIndex->GetBlockHash(), proTxHash), skContribution);

                it = contributionsCache.emplace(cacheKey, ContributionsCacheEntry{TicksSinceEpoch<std::chrono::milliseconds>(SystemClock::now()), vvecPtr, skContribution}).first;
            }

            memberIndexesRet.emplace_back(i);
            vvecsRet.emplace_back(it->second.vvec);
            skContributionsRet.emplace_back(it->second.skContribution);
        }
    }
    return true;
}

void CDKGSessionManager::CleanupCache() const
{
    LOCK(contributionsCacheCs);
    auto curTime = TicksSinceEpoch<std::chrono::milliseconds>(SystemClock::now());
    for (auto it = contributionsCache.begin(); it != contributionsCache.end(); ) {
        if (curTime - it->second.entryTime > MAX_CONTRIBUTION_CACHE_TIME) {
            it = contributionsCache.erase(it);
        } else {
            ++it;
        }
    }
}

void CDKGSessionManager::CleanupOldContributions(ChainstateManager& chainstate) const
{
    if (db->IsEmpty()) {
        return;
    }

    const auto prefixes = {DB_VVEC, DB_SKCONTRIB};

    LogPrint(BCLog::LLMQ, "CDKGSessionManager::%s -- looking for old entries\n", __func__);
    auto &params = Params().GetConsensus().llmqTypeChainLocks;
    CDBBatch batch(*db);
    size_t cnt_old{0}, cnt_all{0};
    for (const auto& prefix : prefixes) {
        std::unique_ptr<CDBIterator> pcursor(db->NewIterator());
        auto start = std::make_tuple(prefix, uint256(), uint256());
        decltype(start) k;

        pcursor->Seek(start);
        LOCK(cs_main);
        while (pcursor->Valid()) {
            if (!pcursor->GetKey(k) || std::get<0>(k) != prefix) {
                break;
            }
            cnt_all++;
            const CBlockIndex* pindexQuorum = chainstate.m_blockman.LookupBlockIndex(std::get<1>(k));
            if (pindexQuorum == nullptr || chainstate.ActiveHeight() - pindexQuorum->nHeight > params.max_store_depth()) {
                // not found or too old
                batch.Erase(k);
                cnt_old++;
            }
            pcursor->Next();
        }
        pcursor.reset();
    }
    LogPrint(BCLog::LLMQ, "CDKGSessionManager::%s -- found %lld entries\n", __func__, cnt_all);
    if (cnt_old > 0) {
        db->WriteBatch(batch);
        LogPrint(BCLog::LLMQ, "CDKGSessionManager::%s -- removed %lld old entries\n", __func__, cnt_old);
    }
}

bool IsQuorumDKGEnabled()
{
    return sporkManager->IsSporkActive(SPORK_17_QUORUM_DKG_ENABLED);
}

} // namespace llmq
