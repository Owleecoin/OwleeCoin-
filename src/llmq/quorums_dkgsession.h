// Copyright (c) 2018-2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_LLMQ_QUORUMS_DKGSESSION_H
#define SYSCOIN_LLMQ_QUORUMS_DKGSESSION_H

#include <llmq/quorums_commitment.h>

#include <batchedlogger.h>
#include <bls/bls.h>
#include <bls/bls_ies.h>
#include <bls/bls_worker.h>

#include <sync.h>

#include <optional>

class UniValue;
class PeerManager;
class CInv;

using CDeterministicMNCPtr = std::shared_ptr<const CDeterministicMN>;

namespace llmq
{

class CFinalCommitment;
class CDKGSession;
class CDKGSessionManager;
class CDKGPendingMessages;

class CDKGContribution
{
public:
    uint256 quorumHash;
    uint256 proTxHash;
    BLSVerificationVectorPtr vvec;
    std::shared_ptr<CBLSIESMultiRecipientObjects<CBLSSecretKey>> contributions;
    CBLSSignature sig;

public:
    template<typename Stream>
    inline void SerializeWithoutSig(Stream& s) const
    {
        s << quorumHash;
        s << proTxHash;
        s << *vvec;
        s << *contributions;
    }
    template<typename Stream>
    inline void Serialize(Stream& s) const
    {
        SerializeWithoutSig(s);
        s << sig;
    }
    template<typename Stream>
    inline void Unserialize(Stream& s)
    {
        std::vector<CBLSPublicKey> tmp1;
        CBLSIESMultiRecipientObjects<CBLSSecretKey> tmp2;

        s >> quorumHash;
        s >> proTxHash;
        s >> tmp1;
        s >> tmp2;
        s >> sig;

        vvec = std::make_shared<std::vector<CBLSPublicKey>>(std::move(tmp1));
        contributions = std::make_shared<CBLSIESMultiRecipientObjects<CBLSSecretKey>>(std::move(tmp2));
    }

    [[nodiscard]] uint256 GetSignHash() const
    {
        CHashWriter hw(SER_GETHASH, 0);
        SerializeWithoutSig(hw);
        hw << CBLSSignature();
        return hw.GetHash();
    }
};

class CDKGComplaint
{
public:
    uint256 quorumHash;
    uint256 proTxHash;
    std::vector<bool> badMembers;
    std::vector<bool> complainForMembers;
    CBLSSignature sig;

public:
    CDKGComplaint() = default;
    explicit CDKGComplaint(const size_t &paramSize) :
            badMembers(paramSize), complainForMembers(paramSize) {};

    SERIALIZE_METHODS(CDKGComplaint, obj)
    {
        READWRITE(
                obj.quorumHash,
                obj.proTxHash,
                DYNBITSET(obj.badMembers),
                DYNBITSET(obj.complainForMembers),
                obj.sig
                );
    }

    [[nodiscard]] uint256 GetSignHash() const
    {
        CDKGComplaint tmp(*this);
        tmp.sig = CBLSSignature();
        return ::SerializeHash(tmp);
    }
};

class CDKGJustification
{
public:
    uint256 quorumHash;
    uint256 proTxHash;
    struct Contribution {
        uint32_t index;
        CBLSSecretKey key;
        SERIALIZE_METHODS(Contribution, obj)
        {
            READWRITE(obj.index, obj.key);
        }
    };
    std::vector<Contribution> contributions;
    CBLSSignature sig;

public:
    SERIALIZE_METHODS(CDKGJustification, obj)
    {
        READWRITE(obj.quorumHash, obj.proTxHash, obj.contributions, obj.sig);
    }

    [[nodiscard]] uint256 GetSignHash() const
    {
        CDKGJustification tmp(*this);
        tmp.sig = CBLSSignature();
        return ::SerializeHash(tmp);
    }
};

// each member commits to a single set of valid members with this message
// then each node aggregate all received premature commitments
// into a single CFinalCommitment, which is only valid if
// enough (>=minSize) premature commitments were aggregated
class CDKGPrematureCommitment
{
public:
    uint256 quorumHash;
    uint256 proTxHash;
    std::vector<bool> validMembers;

    CBLSPublicKey quorumPublicKey;
    uint256 quorumVvecHash;

    CBLSSignature quorumSig; // threshold sig share of quorumHash+validMembers+pubKeyHash+vvecHash
    CBLSSignature sig; // single member sig of quorumHash+validMembers+pubKeyHash+vvecHash

public:
    CDKGPrematureCommitment() = default;
    explicit CDKGPrematureCommitment(const size_t& paramSize) :
            validMembers(paramSize) {};

    [[nodiscard]] int CountValidMembers() const
    {
        return int(std::count(validMembers.begin(), validMembers.end(), true));
    }

public:
    SERIALIZE_METHODS(CDKGPrematureCommitment, obj)
    {
        READWRITE(
                obj.quorumHash,
                obj.proTxHash,
                DYNBITSET(obj.validMembers),
                obj.quorumPublicKey,
                obj.quorumVvecHash,
                obj.quorumSig,
                obj.sig
                );
    }

    [[nodiscard]] uint256 GetSignHash() const
    {
        return BuildCommitmentHash( quorumHash, validMembers, quorumPublicKey, quorumVvecHash);
    }
};

class CDKGMember
{
public:
    CDKGMember(const CDeterministicMNCPtr& _dmn, size_t _idx);

    CDeterministicMNCPtr dmn;
    size_t idx;
    CBLSId id;

    std::set<uint256> contributions;
    std::set<uint256> complaints;
    std::set<uint256> justifications;
    std::set<uint256> prematureCommitments;

    std::set<uint256> badMemberVotes;
    std::set<uint256> complaintsFromOthers;

    bool bad{false};
    bool badConnection{false};
    bool weComplain{false};
    bool someoneComplain{false};
};

class DKGError {
public:
    enum type {
        COMPLAIN_LIE = 0,
        COMMIT_OMIT,
        COMMIT_LIE,
        CONTRIBUTION_OMIT,
        CONTRIBUTION_LIE,
        JUSTIFY_OMIT,
        JUSTIFY_LIE,
        _COUNT
    };
    static constexpr DKGError::type from_string(std::string_view in) {
        if (in == "complain-lie") return COMPLAIN_LIE;
        if (in == "commit-omit") return COMMIT_OMIT;
        if (in == "commit-lie") return COMMIT_LIE;
        if (in == "contribution-omit") return CONTRIBUTION_OMIT;
        if (in == "contribution-lie") return CONTRIBUTION_LIE;
        if (in == "justify-lie") return JUSTIFY_LIE;
        if (in == "justify-omit") return JUSTIFY_OMIT;
        return _COUNT;
    }
};

class CDKGLogger : public CBatchedLogger
{
public:
    CDKGLogger(const CDKGSession& _quorumDkg, std::string_view _func, int source_line);
};

/**
 * The DKG session is a single instance of the DKG process. It is owned and called by CDKGSessionHandler, which passes
 * received DKG messages to the session. The session is not persistent and will loose it's state (the whole object is
 * discarded) when it finishes (after the mining phase) or is aborted.
 *
 * When incoming contributions are received and the verification vector is valid, it is passed to CDKGSessionManager
 * which will store it in the evo DB. Secret key contributions which are meant for the local member are also passed
 * to CDKGSessionManager to store them in the evo DB. If verification of the SK contribution initially fails, it is
 * not passed to CDKGSessionManager. If the justification phase later gives a valid SK contribution from the same
 * member, it is then passed to CDKGSessionManager and after this handled the same way.
 *
 * The contributions stored by CDKGSessionManager are then later loaded by the quorum instances and used for signing
 * sessions, but only if the local node is a member of the quorum.
 */
class CDKGSession
{
    friend class CDKGSessionHandler;
    friend class CDKGSessionManager;
    friend class CDKGLogger;
    friend class CConnman;

private:

    CBLSWorker& blsWorker;
    CBLSWorkerCache cache;
    CDKGSessionManager& dkgManager;

    const CBlockIndex* m_quorum_base_block_index{nullptr};
    bool m_use_legacy_bls;

private:
    std::vector<std::unique_ptr<CDKGMember>> members;
    std::map<uint256, size_t> membersMap;
    std::unordered_set<uint256, StaticSaltedHasher> relayMembers;
    BLSVerificationVectorPtr vvecContribution;
    std::vector<CBLSSecretKey> skContributions;

    std::vector<CBLSId> memberIds;
    std::vector<BLSVerificationVectorPtr> receivedVvecs;
    // these are not necessarily verified yet. Only trust in what was written to the DB
    std::vector<CBLSSecretKey> receivedSkContributions;

    uint256 myProTxHash;
    CBLSId myId;
    std::optional<size_t> myIdx;

    // all indexed by msg hash
    // we expect to only receive a single vvec and contribution per member, but we must also be able to relay
    // conflicting messages as otherwise an attacker might be able to broadcast conflicting (valid+invalid) messages
    // and thus split the quorum. Such members are later removed from the quorum.
    mutable Mutex invCs;
    std::map<uint256, CDKGContribution> contributions GUARDED_BY(invCs);
    std::map<uint256, CDKGComplaint> complaints GUARDED_BY(invCs);
    std::map<uint256, CDKGJustification> justifications GUARDED_BY(invCs);
    std::map<uint256, CDKGPrematureCommitment> prematureCommitments GUARDED_BY(invCs);

    mutable Mutex cs_pending;
    std::vector<size_t> pendingContributionVerifications GUARDED_BY(cs_pending);

    // filled by ReceivePrematureCommitment and used by FinalizeCommitments
    std::set<uint256> validCommitments GUARDED_BY(invCs);

public:
    CDKGSession(CBLSWorker& _blsWorker, CDKGSessionManager& _dkgManager) :
        blsWorker(_blsWorker), cache(_blsWorker), dkgManager(_dkgManager) {}

    bool Init(const CBlockIndex* pQuorumBaseBlockIndex, const std::vector<CDeterministicMNCPtr>& mns, const uint256& _myProTxHash);

    [[nodiscard]] std::optional<size_t> GetMyMemberIndex() const { return myIdx; }

    /**
     * The following sets of methods are for the first 4 phases handled in the session. The flow of message calls
     * is identical for all phases:
     * 1. Execute local action (e.g. create/send own contributions)
     * 2. PreVerify incoming messages for this phase. Preverification means that everything from the message is checked
     *    that does not require too much resources for verification. This specifically excludes all CPU intensive BLS
     *    operations.
     * 3. CDKGSessionHandler will collect pre verified messages in batches and perform batched BLS signature verification
     *    on these.
     * 4. ReceiveMessage is called for each pre verified message with a valid signature. ReceiveMessage is also
     *    responsible for further verification of validity (e.g. validate vvecs and SK contributions).
     */

    // Phase 1: contribution
    void Contribute(CDKGPendingMessages& pendingMessages);
    void SendContributions(CDKGPendingMessages& pendingMessages);
    bool PreVerifyMessage(const CDKGContribution& qc, bool& retBan) const;
    void ReceiveMessage(const uint256& hash, const CDKGContribution& qc) EXCLUSIVE_LOCKS_REQUIRED(!invCs, !cs_pending);
    void VerifyPendingContributions() EXCLUSIVE_LOCKS_REQUIRED(cs_pending);

    // Phase 2: complaint
    void VerifyAndComplain(CDKGPendingMessages& pendingMessages) EXCLUSIVE_LOCKS_REQUIRED(!cs_pending);
    void VerifyConnectionAndMinProtoVersions() const;
    void SendComplaint(CDKGPendingMessages& pendingMessages);
    bool PreVerifyMessage(const CDKGComplaint& qc, bool& retBan) const;
    void ReceiveMessage(const uint256& hash, const CDKGComplaint& qc) EXCLUSIVE_LOCKS_REQUIRED(!invCs, !cs_pending);

    // Phase 3: justification
    void VerifyAndJustify(CDKGPendingMessages& pendingMessages) EXCLUSIVE_LOCKS_REQUIRED(!invCs);
    void SendJustification(CDKGPendingMessages& pendingMessages, const std::set<uint256>& forMembers);
    bool PreVerifyMessage(const CDKGJustification& qj, bool& retBan) const;
    void ReceiveMessage(const uint256& hash, const CDKGJustification& qj) EXCLUSIVE_LOCKS_REQUIRED(!invCs);

    // Phase 4: commit
    void VerifyAndCommit(CDKGPendingMessages& pendingMessages);
    void SendCommitment(CDKGPendingMessages& pendingMessages);
    bool PreVerifyMessage(const CDKGPrematureCommitment& qc, bool& retBan) const;
    void ReceiveMessage(const uint256& hash, const CDKGPrematureCommitment& qc) EXCLUSIVE_LOCKS_REQUIRED(!invCs);

    // Phase 5: aggregate/finalize
    std::vector<CFinalCommitment> FinalizeCommitments() EXCLUSIVE_LOCKS_REQUIRED(!invCs);

    [[nodiscard]] bool AreWeMember() const { return !myProTxHash.IsNull(); }
    void MarkBadMember(size_t idx);

    void RelayOtherInvToParticipants(const CInv& inv, PeerManager& peerman) const;

public:
    [[nodiscard]] CDKGMember* GetMember(const uint256& proTxHash) const;

private:
    [[nodiscard]] bool ShouldSimulateError(DKGError::type type) const;
};

void SetSimulatedDKGErrorRate(DKGError::type type, double rate);
double GetSimulatedErrorRate(DKGError::type type);

} // namespace llmq

#endif // SYSCOIN_LLMQ_QUORUMS_DKGSESSION_H
