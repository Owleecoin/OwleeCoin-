// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>
// SYSCOIN includes for gen block
#include <chainparams.h>
/*#include <uint256.h>
#include <arith_uint256.h>
#include <streams.h>
#include <time.h>*/
static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}
// This will figure out a valid hash and Nonce if you're
// creating a different genesis block:
/*static void GenerateGenesisBlock(CBlockHeader &genesisBlock, uint256 &phash)
{
    arith_uint256 bnTarget;
    bnTarget.SetCompact(genesisBlock.nBits);
    uint32_t nOnce = 0;
    while (true) {
        genesisBlock.nNonce = nOnce;
        uint256 hash = genesisBlock.GetHash();
        if (UintToArith256(hash) <= bnTarget) {
            phash = hash;
            break;
        }
        nOnce++;
    }
    tfm::format(std::cout,"genesis.nTime = %u \n", genesisBlock.nTime);
    tfm::format(std::cout,"genesis.nNonce = %u \n", genesisBlock.nNonce);
    tfm::format(std::cout,"Generate hash = %s\n", phash.ToString().c_str());
    tfm::format(std::cout,"genesis.hashMerkleRoot = %s\n", genesisBlock.hashMerkleRoot.ToString().c_str());
} */
/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}
void CChainParams::UpdateLLMQTestParams(int size, int threshold) {
    auto& params = consensus.llmqTypeChainLocks;
    params.size = size;
    params.minSize = threshold;
    params.threshold = threshold;
    params.dkgBadVotesThreshold = threshold;
}
// this one is for testing only
static Consensus::LLMQParams llmq_test = {
        .name = "llmq_test",
        .size = 3,
        .minSize = 2,
        .threshold = 2,

        .dkgInterval = 24, // one DKG per hour
        .dkgPhaseBlocks = 2,
        .dkgMiningWindowStart = 10, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 18,
        .dkgBadVotesThreshold = 2,

        .signingActiveQuorumCount = 4, // just a few ones to allow easier testing

        .keepOldConnections = 5,
        .recoveryMembers = 3,
};

static Consensus::LLMQParams llmq400_60 = {
        .name = "llmq_400_60",
        .size = 400,
        .minSize = 300,
        .threshold = 240,

        .dkgInterval = 24 * 12, // one DKG every 12 hours
        .dkgPhaseBlocks = 4,
        .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 28,
        .dkgBadVotesThreshold = 300,

        .signingActiveQuorumCount = 4, // two days worth of LLMQs

        .keepOldConnections = 5,
        .recoveryMembers = 100,
};

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    // SYSCOIN
    CMainParams(const MainNetOptions& opts) {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210240;
        // 35% increase after ~1 year, 100% increase after sr level 2 (~2.5 years)
        consensus.nSeniorityHeight1 = 525600;
        consensus.nSeniorityLevel1 = 0.35;
        consensus.nSeniorityHeight2 = consensus.nSeniorityHeight1*2.5;
        consensus.nSeniorityLevel2 = 1.0;
        consensus.nSuperblockStartBlock = 1;
        consensus.nSuperblockCycle = 17520; // ~(60*24*30)/2.5
        consensus.nSuperblockMaturityWindow = 1728; // ~(60*24*3)/2.5, ~3 days before actual Superblock is emitted
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.nMinMNSubsidySats = 527500000;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256S("0x00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22"), SCRIPT_VERIFY_NONE);
        consensus.script_flag_exceptions.emplace( // Taproot exception
            uint256S("0x0000000000000000000f14c35b2d841e986ab5441de8c585d5ffe55ea1e395ad"), SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS);
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 6 * 60 * 60;
        consensus.nPowTargetSpacing = 2.5 * 60; // Syscoin: 2.5 minute
        consensus.nAuxpowChainId = 16;
        consensus.nAuxpowOldChainId = 4096;
        consensus.nAuxpowStartHeight = 1;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = consensus.nMinerConfirmationWindow;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000036969a93144b782527fde845"); // 1989728

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xe1b6214ca67cf3e98d8d08d4bad95bcd620fc72c6ea98af3e4b10b28d2a462e5"); // 1989728
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 1;
        consensus.nSYSXAsset = 123456;
        consensus.nNEVMChainID = 57;
        consensus.vchSyscoinVaultManager = ParseHex("7904299b3D3dC1b03d1DdEb45E9fDF3576aCBd5f");
        consensus.vchTokenFreezeMethod = ParseHex("0b8914e27c9a6c88836bc5547f82ccf331142c761f84e9f1d36934a6a31eefad");
        consensus.nBridgeStartBlock = 348000;
        consensus.nNEVMStartBlock = 1317500;
        consensus.nNEVMStartTime = 1638791667;
        consensus.nPODAStartBlock = 1586000;
        consensus.nV19StartBlock = 1586000;
        consensus.nNexusStartBlock = 2010345;
        consensus.DIP0003Height = 1004200;
        consensus.DIP0003EnforcementHeight = 1004200;
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xce;
        pchMessageStart[1] = 0xe2;
        pchMessageStart[2] = 0xca;
        pchMessageStart[3] = 0xff;
        nDefaultPort = 8369;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 30;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlock(1559520000, 1372898, 0x1e0fffff, 1, 50 * COIN);

        /*uint256 hash;
        CBlockHeader genesisHeader = genesis.GetBlockHeader();
        GenerateGenesisBlock(genesisHeader, hash);*/
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0000022642db0346b6e01c2a397471f4f12e65d4f4251ec96c1f85367a61a7ab"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vSeeds.emplace_back("seed1.syscoin.org");
        vSeeds.emplace_back("seed2.syscoin.org");
        vSeeds.emplace_back("seed3.syscoin.org");
        vSeeds.emplace_back("seed4.syscoin.org");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,63);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        // SYSCOIN
        bech32_hrp = opts.bech32_hrp;

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        fRequireRoutableExternalIP = true;
        vSporkAddresses = {"sys1qx0zzzjag402apkw4kn8unr0qa0k3pv3258v4sr", "sys1qk2kq7hhp58ycaevzzu5hugh7flxs7qcg8rjjlh", "sys1qm4ka204x3mn46sk6ussrex8um87qkj0r5xakyg"};
        nMinSporkKeys = 2;
        // long living quorum params
        consensus.llmqTypeChainLocks = llmq400_60;
        nLLMQConnectionRetryTimeout = 60;
        nFulfilledRequestExpireTime = 60*60; // fulfilled requests expire in 1 hour
        m_is_mockable_chain = false;

        checkpointData = {
            {
                { 250, uint256S("0x00000c9ec0f9d60ce297bf9f9cbe1f2eb39165a0d3f69c1c55fc3f6680fe45c8")},
                { 5000, uint256S("0xeef3554a3f467bcdc7570f799cecdb262058cecf34d555827c99b5719b1df4f6")},
                { 10000, uint256S("0xe44257e8e027e8a67fd647c54e1bd6976988d75b416affabe3f82fd87a67f5ff")},
                { 40000, uint256S("0x4ad1ec207d62fa91485335feaf890150a0f4cf48c39b11e3dbfc22bdecc29dbc")},
                { 100000, uint256S("0xa54904302fd6fd0ee561cb894f15ad8c21c2601b305ffa9e15ef00df1c50db16")},
                { 150000, uint256S("0x73850eb99a6c32b4bfd67a26a7466ce3d0b4412d4174590c501e567c99f038fd")},
                { 200000, uint256S("0xa28fe36c63acb38065dadf09d74de5fdc1dac6433c204b215b37bab312dfab0d")},
                { 240000, uint256S("0x906918ba0cbfbd6e4e4e00d7d47d08bef3e409f47b59cb5bd3303f5276b88f0f")},
                { 280000, uint256S("0x651375427865345d37a090ca561c1ed135c6b8dafa591a59f2abf1eb26dfd538")},
                { 292956, uint256S("0xae6dca1b9dd7adcb8a11c8ea7f9fe72bb47ff6e4156e1d172e2a8612b18a319d")},
                { 350000, uint256S("0x02501c7feba858c83e005acbf0505a892081288dcf7a8a37bd4fc47d7c24c799")},
                { 390000, uint256S("0x8654451a7ed5286ba5c830cdf6e65cbbd7a77f650216541bfbe50af04933741b")},
                { 391285, uint256S("76d13e8f08c2b7027251484078f734f91c485727031be6b4c21c42d5e103d0ad")},
                { 419800, uint256S("4c332acd53ca99ab78fb80a3dacffe234674674e0b682350c492d7fe839d128e")},
                { 600000, uint256S("de2321b2a3b927450835590111bbbc9220d49df865117a33fb3c4687aedbbe9c")},
                { 700000, uint256S("f5d72e57625c2af8cd5147e1e029e8353fea22fde1fcea06f67149d0af1fbf09")},
                { 800000, uint256S("54bf4bd4b5c7d36323fed4b649e75e0ce4902261533d13a15c861fa2ab3c7362")},
                { 998000, uint256S("e9599cf8d6462f63f17a8ec790803cf77028a380a1de84a976039914a45f5abb")},
                { 1213640, uint256S("bd9ff6428a7cc472d3813bbee6fb3ae1a9992b8b034deca1249487a4a1b8e51a")},
                { 1400000, uint256S("ca0067113d48a87eaed88c1410cacfe07441e191487383b79bf7069a678ede4a")},
                { 1576166, uint256S("4b8519c2193265fe269e88361787339504dda66b4efa85613c661a431ad1624c")},
                { 1586970, uint256S("5c5a43bece78786ee261458dc300323cec0485b61d6b33a65d624aadf9a1d35b")},
                { 1632040, uint256S("12a436d9fa797ab570d01af510ee0f7ee1fb61361f0bb23e8418014a5f981f72")},
                { 1989728, uint256S("e1b6214ca67cf3e98d8d08d4bad95bcd620fc72c6ea98af3e4b10b28d2a462e5")},
            }
        };

        m_assumeutxo_data = {
            // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats at block 1989724
            .nTime    = 1740943658,
            .nTxCount = 3307941,
            .dTxRate  = 0.01824913153720413
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nSeniorityHeight1 = 60;
        consensus.bTestnet = true;
        consensus.nSeniorityLevel1 = 0.35;
        consensus.nSeniorityHeight2 = consensus.nSeniorityHeight1*2.5;
        consensus.nSeniorityLevel2 = 1.0;
        consensus.nSuperblockStartBlock = 1;
        consensus.nSuperblockCycle = 60;
        consensus.nSuperblockMaturityWindow = 20;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMinMNSubsidySats = 527500000;

        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.SegwitHeight = 0;
        consensus.CSVHeight = 1;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 6 * 60 * 60;
        consensus.nPowTargetSpacing = 2.5 * 60; // Syscoin: 2.5 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = consensus.nMinerConfirmationWindow;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105"), SCRIPT_VERIFY_NONE);
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000000000002413744a0ef5"); // 1023140

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000002da4aa86462e1c60bbd7d28b89229592bb82828ff487d88a4996c6e0e2"); // 1023140
        consensus.nAuxpowStartHeight = 1;
        consensus.nAuxpowChainId = 8;
        consensus.nAuxpowOldChainId = 4096;
        consensus.fStrictChainId = false;
        consensus.nLegacyBlocksBefore = 1;
        consensus.nSYSXAsset = 123456;
        consensus.nNEVMChainID = 5700;
        consensus.vchSyscoinVaultManager = ParseHex("7904299b3D3dC1b03d1DdEb45E9fDF3576aCBd5f");
        consensus.vchTokenFreezeMethod = ParseHex("0b8914e27c9a6c88836bc5547f82ccf331142c761f84e9f1d36934a6a31eefad");
        consensus.nBridgeStartBlock = 1000;
        consensus.nNEVMStartBlock = 840000;
        consensus.nNEVMStartTime = 1632775675;
        consensus.nPODAStartBlock = 1022500;
        consensus.nV19StartBlock = 1063000;
        consensus.nNexusStartBlock = 1505000;
        consensus.DIP0003Height = 545000;
        consensus.DIP0003EnforcementHeight = 545000;
        pchMessageStart[0] = 0xce;
        pchMessageStart[1] = 0xe2;
        pchMessageStart[2] = 0xca;
        pchMessageStart[3] = 0xfe;
        nDefaultPort = 18369;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 30;
        m_assumed_chain_state_size = 2;
        genesis = CreateGenesisBlock(1576000000, 297648, 0x1e0fffff, 1, 50 * COIN);
        /*uint256 hash;
        CBlockHeader genesisHeader = genesis.GetBlockHeader();
        GenerateGenesisBlock(genesisHeader, hash);*/
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0000066e1a6b9cfeac8295dce0cc8d9170690a74bc4878cf8a0b412554f5c222"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("testseed1.syscoin.org");
        vSeeds.emplace_back("testseed2.syscoin.org");
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tsys";

        // vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        fRequireRoutableExternalIP = true;

        // privKey: cU52TqHDWJg6HoL3keZHBvrJgsCLsduRvDFkPyZ5EmeMwoEHshiT
        vSporkAddresses = {"TCGpumHyMXC5BmfkaAQXwB7Bf4kbkhM9BX", "tsys1qgmafz3mqa7glqy92r549w8qmq5535uc2e8ahjm", "tsys1q68gu0fhcchr27w08sjdxwt3rtgwef0nyh9zwk0"};
        nMinSporkKeys = 2;
        // long living quorum params
        consensus.llmqTypeChainLocks = llmq400_60;
        nLLMQConnectionRetryTimeout = 60;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        m_is_mockable_chain = false;
        checkpointData = {
            {
                {360, uint256S("0x00000c04c5926f539074420b40088d4b099d748d07795df891ca391799b6e54c")},
                {250000, uint256S("0x00000131e97a4cb713338f33b8fa6573c85f1772e4dd7d510ca2281cc0be86e2")},
                {534114, uint256S("0x0000013d53482bd69c5403f344643668619f77302910e57ffe7b1d375e73cc91")},
                {838467, uint256S("0x0000003243223caf052c7e5e6710fae794dbdc10949a594550f073dbf5755bd4")},
                {900000, uint256S("0x000000071b620e50257980306f48a8f8f331dbf385c52b8a1bea11331d020e5e")},
                {1000000, uint256S("0x000000236997f1bbd8b2d0d8ecf982cce3f5ec4ace44cc7853a26fffa366b6ab")},
                {1020000, uint256S("0x00000029c0b3acda1d389c7d980a93315a8d74ccfe299621ac895358393e2f46")},
                {1023125, uint256S("0x0000002b308601b4b68bc4ab58f434252bc6fc07c147b14e6ccc996e5a6af219")},
                {1023126, uint256S("0x000003820d73f238c939b9c4f87ae1ad6851e346153620a5140c3d4d0a8cb442")},
                {1023140, uint256S("0x0000002da4aa86462e1c60bbd7d28b89229592bb82828ff487d88a4996c6e0e2")},
            }
        };

        m_assumeutxo_data = {
            // TODO to be specified in a future patch.
        };
        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 0000000000000037a8cd3e06cd5edbfe9dd1dbcc5dacab279376ef7cfc2b4c75
            .nTime    = 1669101140,
            .nTxCount = 1043445,
            .dTxRate  = 0.001586750190549993
        };
    }
};
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vSeeds.clear();

        if (!options.challenge) {
            bin = ParseHex("512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae");
            vSeeds.emplace_back("seed.signet.bitcoin.sprovoost.nl.");

            // Hardcoded nodes can be removed once there are more DNS seeds
            vSeeds.emplace_back("178.128.221.177");
            vSeeds.emplace_back("v7ajjeirttkbnt32wpy3c6w3emwnfr3fkla7hpxcfokr3ysd3kqtzmqd.onion:38333");

            consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000001291fc22898");
            consensus.defaultAssumeValid = uint256S("0x000000d1a0e224fa4679d2fb2187ba55431c284fa1b74cbc8cfda866fd4d2c09"); // 105495
            m_assumed_blockchain_size = 1;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000d1a0e224fa4679d2fb2187ba55431c284fa1b74cbc8cfda866fd4d2c09
                .nTime    = 1661702566,
                .nTxCount = 1903567,
                .dTxRate  = 0.02336701143027275,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("00000377ae000000000000000000000000000000000000000000000000000000");
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 38333;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1598918400, 52613770, 0x1e0377ae, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nSeniorityHeight1 = 60;
        consensus.nSeniorityLevel1 = 0.35;
        consensus.nSeniorityHeight2 = consensus.nSeniorityHeight1*2.5;
        consensus.nSeniorityLevel2 = 1.0;
        consensus.nSuperblockStartBlock = 1;
        consensus.nSuperblockCycle = 10;
        consensus.nSuperblockMaturityWindow = 5;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMinMNSubsidySats = 527500000;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 6 * 60 * 60;
        consensus.nPowTargetSpacing = 2.5 * 60; // Syscoin: 2.5 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};
        consensus.nAuxpowStartHeight = 0;
        consensus.nAuxpowChainId = 16;
        consensus.nAuxpowOldChainId = 4096;
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 0;
        consensus.nSYSXAsset = 123456;
        consensus.nNEVMChainID = 5700;
        consensus.vchSyscoinVaultManager = ParseHex("7904299b3D3dC1b03d1DdEb45E9fDF3576aCBd5f");
        consensus.vchTokenFreezeMethod = ParseHex("0b8914e27c9a6c88836bc5547f82ccf331142c761f84e9f1d36934a6a31eefad");
        consensus.nBridgeStartBlock = 0;
        consensus.nNEVMStartBlock = opts.nevmstartblock;
        consensus.nNEVMStartTime = 0;
        consensus.nPODAStartBlock = 0;
        consensus.nNexusStartBlock = opts.dip3startblock;
        consensus.nV19StartBlock = opts.v19startblock;
        consensus.DIP0003Height = opts.dip3startblock;
        consensus.DIP0003EnforcementHeight = opts.dip3enforcement;

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }
        /*uint256 hash;
        CBlockHeader genesisHeader = genesis.GetBlockHeader();
        GenerateGenesisBlock(genesisHeader, hash);*/
        genesis = CreateGenesisBlock(1553040331, 3, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x28a2c2d251f46fac05ade79085cbcb2ae4ec67ea24f1f1c7b40a348c00521194"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        fRequireRoutableExternalIP = false;
        m_is_mockable_chain = true;
        // privKey: cVpF924EspNh8KjYsfhgY96mmxvT6DgdWiTYMtMjuM74hJaU5psW
        vSporkAddresses = {"mjTkW3DjgyZck4KbiRusZsqTgaYTxdSz6z"};
        nMinSporkKeys = 1;
        // long living quorum params
        consensus.llmqTypeChainLocks = llmq_test;
        nLLMQConnectionRetryTimeout = 1; // must be lower then the LLMQ signing session timeout so that tests have control over failing behavior
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes

        checkpointData = {
            {
                {0, uint256S("0x28a2c2d251f46fac05ade79085cbcb2ae4ec67ea24f1f1c7b40a348c00521194")},
            }
        };

         m_assumeutxo_data = {
            {
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256S("0x6657b736d4fe4db0cbc796789e812d5dba7f5c143764b1b6905612f1830609d1")},
                .nChainTx = 111,
                .blockhash = uint256S("0x07fbf5f448734557e1f33b6919c0e6b93828f0eef1a5938519d8b18d6bfd7510")
            },
            {
                .height = 200,
                .hash_serialized = AssumeutxoHash{uint256S("0x51c8d11d8b5c1de51543c579736e786aa2736206d1e11e627568029ce092cf62")},
                .nChainTx = 201,
                .blockhash = uint256S("0x4714f69f1351cec30da58e04dfb6e8435684fd0b500ed80fee44876a2dc41bab")
            }
            /*{
                // For use by test/functional/feature_assumeutxo.py
                .height = 299,
                .hash_serialized = AssumeutxoHash{uint256S("0xa4bf3407ccb2cc0145c49ebba8fa91199f8a3903daf0883875941497d2493c27")},
                .nChainTx = 334,
                .blockhash = uint256S("0x3bb7ce5eba0be48939b7a521ac1ba9316afee2c7bada3a0cca24188e6d7d96c0")
            },*/
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}
// SYSCOIN
std::unique_ptr<const CChainParams> CChainParams::Main(const MainNetOptions& options)
{
    return std::make_unique<const CMainParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}
