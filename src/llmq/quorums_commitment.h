// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_LLMQ_QUORUMS_COMMITMENT_H
#define SYSCOIN_LLMQ_QUORUMS_COMMITMENT_H

#include <llmq/quorums_utils.h>

#include <bls/bls.h>

#include <univalue.h>
namespace node {
class BlockManager;
}
namespace llmq
{

// This message is an aggregation of all received premature commitments and only valid if
// enough (>=threshold) premature commitments were aggregated
// This is mined on-chain as part of SYSCOIN_TX_VERSION_MN_QUORUM_COMMITMENT
class CFinalCommitment
{
public:
    static constexpr auto SPECIALTX_TYPE = SYSCOIN_TX_VERSION_MN_REGISTER;

    static constexpr uint16_t LEGACY_BLS_NON_INDEXED_QUORUM_VERSION = 1;
    static constexpr uint16_t BASIC_BLS_NON_INDEXED_QUORUM_VERSION = 3;

    uint16_t nVersion{LEGACY_BLS_NON_INDEXED_QUORUM_VERSION};
    uint256 quorumHash;
    std::vector<bool> signers;
    std::vector<bool> validMembers;

    CBLSPublicKey quorumPublicKey;
    uint256 quorumVvecHash;

    CBLSSignature quorumSig; // recovered threshold sig of blockHash+validMembers+pubKeyHash+vvecHash
    CBLSSignature membersSig; // aggregated member sig of blockHash+validMembers+pubKeyHash+vvecHash

public:
    CFinalCommitment() = default;
    CFinalCommitment(const uint256& _quorumHash);

    int CountSigners() const
    {
        return (int)std::count(signers.begin(), signers.end(), true);
    }
    int CountValidMembers() const
    {
        return (int)std::count(validMembers.begin(), validMembers.end(), true);
    }

    bool Verify(const CBlockIndex* pQuorumBaseBlockIndex, bool checkSigs) const;
    bool VerifyNull() const;
    bool VerifySizes() const;
    [[nodiscard]] static constexpr uint16_t GetVersion(const bool is_basic_scheme_active)
    {
        return is_basic_scheme_active ? BASIC_BLS_NON_INDEXED_QUORUM_VERSION : LEGACY_BLS_NON_INDEXED_QUORUM_VERSION;
    }

public:
    SERIALIZE_METHODS(CFinalCommitment, obj)
    {
        READWRITE(
                obj.nVersion,
                obj.quorumHash
        );
        READWRITE(
                DYNBITSET(obj.signers),
                DYNBITSET(obj.validMembers),
                CBLSPublicKeyVersionWrapper(const_cast<CBLSPublicKey&>(obj.quorumPublicKey), (obj.nVersion == LEGACY_BLS_NON_INDEXED_QUORUM_VERSION)),
                obj.quorumVvecHash,
                CBLSSignatureVersionWrapper(const_cast<CBLSSignature&>(obj.quorumSig), (obj.nVersion == LEGACY_BLS_NON_INDEXED_QUORUM_VERSION)),
                CBLSSignatureVersionWrapper(const_cast<CBLSSignature&>(obj.membersSig), (obj.nVersion == LEGACY_BLS_NON_INDEXED_QUORUM_VERSION))
        );
    }
public:
    bool IsNull() const
    {
        if (std::count(signers.begin(), signers.end(), true) ||
            std::count(validMembers.begin(), validMembers.end(), true)) {
            return false;
        }
        if (quorumPublicKey.IsValid() ||
            !quorumVvecHash.IsNull() ||
            membersSig.IsValid() ||
            quorumSig.IsValid()) {
            return false;
        }
        return true;
    }

    void ToJson(UniValue& obj) const
    {
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("quorumHash", quorumHash.ToString());
        obj.pushKV("signersCount", CountSigners());
        obj.pushKV("signers", CLLMQUtils::ToHexStr(signers));
        obj.pushKV("validMembersCount", CountValidMembers());
        obj.pushKV("validMembers", CLLMQUtils::ToHexStr(validMembers));
        obj.pushKV("quorumPublicKey", quorumPublicKey.ToString(nVersion == LEGACY_BLS_NON_INDEXED_QUORUM_VERSION));
        obj.pushKV("quorumVvecHash", quorumVvecHash.ToString());
        obj.pushKV("quorumSig", quorumSig.ToString(nVersion == LEGACY_BLS_NON_INDEXED_QUORUM_VERSION));
        obj.pushKV("membersSig", membersSig.ToString(nVersion == LEGACY_BLS_NON_INDEXED_QUORUM_VERSION));
    }
};
using CFinalCommitmentPtr = std::unique_ptr<CFinalCommitment>;

class CFinalCommitmentTxPayload
{
public:
    static constexpr uint16_t CURRENT_VERSION = 2;
    uint16_t nVersion{CURRENT_VERSION};
    uint32_t nHeight{0};
    CFinalCommitment commitment;

public:
    SERIALIZE_METHODS(CFinalCommitmentTxPayload, obj) {
        READWRITE(obj.nVersion, obj.nHeight, obj.commitment);
    }   

    void ToJson(UniValue& obj) const
    {
        UniValue qcObj;
        commitment.ToJson(qcObj);
        obj.pushKV("version", nVersion);
        obj.pushKV("height", nHeight);
        obj.pushKV("commitment", qcObj);
    }
    inline bool IsNull() const {return nHeight == 0;}
};

} // namespace llmq

#endif // SYSCOIN_LLMQ_QUORUMS_COMMITMENT_H
