// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstate.h>

#include <consensus/params.h>
#include <node/blockstorage.h>
#include <validation.h>
// SYSCOIN
#include <services/assetconsensus.h>
#include <evo/evodb.h>
#include <evo/deterministicmns.h>
#include <llmq/quorums_init.h>
#include <governance/governance.h>

namespace node {
std::optional<ChainstateLoadingError> LoadChainstate(bool fReset,
                                                     ChainstateManager& chainman,
                                                     CConnman& connman,
                                                     BanMan& banman,
                                                     PeerManager& peerman,
                                                     CTxMemPool* mempool,
                                                     bool fPruneMode,
                                                     bool fReindexChainState,
                                                     int64_t nBlockTreeDBCache,
                                                     int64_t nCoinDBCache,
                                                     int64_t nCoinCacheUsage,
                                                     bool block_tree_db_in_memory,
                                                     bool coins_db_in_memory,
                                                     bool fAssetIndex,
                                                     bool fReindexGeth,
                                                     int64_t nEvoDbCache,
                                                     std::function<bool()> shutdown_requested,
                                                     std::function<void()> coins_error_cb)
{
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return fReset || fReindexChainState || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    LOCK(cs_main);
    chainman.InitializeChainstate(mempool);
    chainman.m_total_coinstip_cache = nCoinCacheUsage;
    chainman.m_total_coinsdb_cache = nCoinDBCache;

    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    // SYSCOIN
    if(fAssetIndex) {
        LogPrintf("Asset Index enabled, allowing for an asset aware spending policy\n");
    }
    LogPrintf("Creating LLMQ and asset databases...\n");
    passetdb.reset();
    passetnftdb.reset();
    pnevmtxrootsdb.reset();
    pnevmtxmintdb.reset();
    pblockindexdb.reset();
    pnevmdatadb.reset();
    llmq::DestroyLLMQSystem();
    evoDb.reset();
    evoDb.reset(new CEvoDB(nEvoDbCache, block_tree_db_in_memory, fReindexGeth));
    deterministicMNManager.reset();
    deterministicMNManager.reset(new CDeterministicMNManager(*evoDb));
    governance.reset();
    governance.reset(new CGovernanceManager(chainman));
    llmq::InitLLMQSystem(*evoDb, block_tree_db_in_memory, connman, banman, peerman, chainman, fReindexGeth);
    passetdb.reset(new CAssetDB(nEvoDbCache, block_tree_db_in_memory, fReindexGeth));
    passetnftdb.reset(new CAssetNFTDB(nEvoDbCache, block_tree_db_in_memory, fReindexGeth));
    pnevmtxrootsdb.reset(new CNEVMTxRootsDB(nEvoDbCache, block_tree_db_in_memory, fReindexGeth));
    pnevmtxmintdb.reset(new CNEVMMintedTxDB(nEvoDbCache, block_tree_db_in_memory, fReindexGeth));
    pblockindexdb.reset(new CBlockIndexDB(nEvoDbCache, block_tree_db_in_memory, fReindexGeth));
    pnevmdatadb.reset(new CNEVMDataDB(1000 << 20, block_tree_db_in_memory));
    if (!evoDb->CommitRootTransaction()) {
        return ChainstateLoadingError::ERROR_COMMIT_EVODB;
    }
    if (fReindexGeth && !evoDb->IsEmpty()) {
        // EvoDB processed some blocks earlier but we have no blocks anymore, something is wrong
        return ChainstateLoadingError::ERROR_LOAD_GENESIS_BLOCK_FAILED;
    }
    // new CBlockTreeDB tries to delete the existing file, which
    // fails if it's still open from the previous loop. Close it first:
    pblocktree.reset();
    pblocktree.reset(new CBlockTreeDB(nBlockTreeDBCache, block_tree_db_in_memory, fReset));

    if (fReset) {
        pblocktree->WriteReindexing(true);
        //If we're reindexing in prune mode, wipe away unusable block files and all undo data files
        if (fPruneMode)
            CleanupBlockRevFiles();
    }

    if (shutdown_requested && shutdown_requested()) return ChainstateLoadingError::SHUTDOWN_PROBED;

    // LoadBlockIndex will load m_have_pruned if we've ever removed a
    // block file from disk.
    // Note that it also sets fReindex based on the disk flag!
    // From here on out fReindex and fReset mean something different!
    if (!chainman.LoadBlockIndex()) {
        if (shutdown_requested && shutdown_requested()) return ChainstateLoadingError::SHUTDOWN_PROBED;
        return ChainstateLoadingError::ERROR_LOADING_BLOCK_DB;
    }

    if (!chainman.BlockIndex().empty() &&
            !chainman.m_blockman.LookupBlockIndex(chainman.GetConsensus().hashGenesisBlock)) {
        return ChainstateLoadingError::ERROR_BAD_GENESIS_BLOCK;
    }

    // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
    // in the past, but is now trying to run unpruned.
    if (chainman.m_blockman.m_have_pruned && !fPruneMode) {
        return ChainstateLoadingError::ERROR_PRUNED_NEEDS_REINDEX;
    }

    // At this point blocktree args are consistent with what's on disk.
    // If we're not mid-reindex (based on disk + args), add a genesis block on disk
    // (otherwise we use the one already on disk).
    // This is called again in ThreadImport after the reindex completes.
    if (!fReindex && !chainman.ActiveChainstate().LoadGenesisBlock()) {
        return ChainstateLoadingError::ERROR_LOAD_GENESIS_BLOCK_FAILED;
    }

    // At this point we're either in reindex or we've loaded a useful
    // block tree into BlockIndex()!
    // SYSCOIN
    bool coinsViewEmpty = false;
    for (CChainState* chainstate : chainman.GetAll()) {
        chainstate->InitCoinsDB(
            /*cache_size_bytes=*/nCoinDBCache,
            /*in_memory=*/coins_db_in_memory,
            /*should_wipe=*/fReset || fReindexChainState);

        if (coins_error_cb) {
            chainstate->CoinsErrorCatcher().AddReadErrCallback(coins_error_cb);
        }

        // Refuse to load unsupported database format.
        // This is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
        if (chainstate->CoinsDB().NeedsUpgrade()) {
            return ChainstateLoadingError::ERROR_CHAINSTATE_UPGRADE_FAILED;
        }

        // ReplayBlocks is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
        if (!chainstate->ReplayBlocks()) {
            return ChainstateLoadingError::ERROR_REPLAYBLOCKS_FAILED;
        }

        // The on-disk coinsdb is now in a good state, create the cache
        chainstate->InitCoinsCache(nCoinCacheUsage);
        assert(chainstate->CanFlushToDisk());

        if (!is_coinsview_empty(chainstate)) {
            // LoadChainTip initializes the chain based on CoinsTip()'s best block
            if (!chainstate->LoadChainTip()) {
                return ChainstateLoadingError::ERROR_LOADCHAINTIP_FAILED;
            }
            assert(chainstate->m_chain.Tip() != nullptr);
        }
        // SYSCOIN
        else {
            coinsViewEmpty = true;
        }
    }

    if (!fReset) {
        auto chainstates{chainman.GetAll()};
        if (std::any_of(chainstates.begin(), chainstates.end(),
                        [](const CChainState* cs) EXCLUSIVE_LOCKS_REQUIRED(cs_main) { return cs->NeedsRedownload(); })) {
            return ChainstateLoadingError::ERROR_BLOCKS_WITNESS_INSUFFICIENTLY_VALIDATED;
        }
    }
    // if coinsview is empty we clear all SYS db's overriding anything we did before
    if(coinsViewEmpty && !fReindexGeth) {
        LogPrintf("coinsViewEmpty recreating LLMQ and asset databases\n");
        passetdb.reset();
        passetnftdb.reset();
        pnevmtxrootsdb.reset();
        pnevmtxmintdb.reset();
        pblockindexdb.reset();
        pnevmdatadb.reset();
        llmq::DestroyLLMQSystem();
        evoDb.reset();
        evoDb.reset(new CEvoDB(nEvoDbCache, block_tree_db_in_memory, coinsViewEmpty));
        deterministicMNManager.reset();
        deterministicMNManager.reset(new CDeterministicMNManager(*evoDb));
        llmq::InitLLMQSystem(*evoDb, block_tree_db_in_memory, connman, banman, peerman, chainman, coinsViewEmpty);
        passetdb.reset(new CAssetDB(nCoinDBCache*16, block_tree_db_in_memory, coinsViewEmpty));
        passetnftdb.reset(new CAssetNFTDB(nCoinDBCache*16, block_tree_db_in_memory, coinsViewEmpty));
        pnevmtxrootsdb.reset(new CNEVMTxRootsDB(nCoinDBCache, block_tree_db_in_memory, coinsViewEmpty));
        pnevmtxmintdb.reset(new CNEVMMintedTxDB(nCoinDBCache, block_tree_db_in_memory, coinsViewEmpty));
        pblockindexdb.reset(new CBlockIndexDB(nCoinDBCache, block_tree_db_in_memory, coinsViewEmpty));
        pnevmdatadb.reset(new CNEVMDataDB(1000 << 20, block_tree_db_in_memory));
        if (!evoDb->CommitRootTransaction()) {
            return ChainstateLoadingError::ERROR_COMMIT_EVODB;
        }
        if (!evoDb->IsEmpty()) {
            // EvoDB processed some blocks earlier but we have no blocks anymore, something is wrong
            return ChainstateLoadingError::ERROR_LOAD_GENESIS_BLOCK_FAILED;
        }
    }

    return std::nullopt;
}

std::optional<ChainstateLoadVerifyError> VerifyLoadedChainstate(ChainstateManager& chainman,
                                                                bool fReset,
                                                                bool fReindexChainState,
                                                                int check_blocks,
                                                                int check_level)
{
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return fReset || fReindexChainState || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    LOCK(cs_main);

    for (CChainState* chainstate : chainman.GetAll()) {
        if (!is_coinsview_empty(chainstate)) {
            const CBlockIndex* tip = chainstate->m_chain.Tip();
            if (tip && tip->nTime > GetTime() + MAX_FUTURE_BLOCK_TIME) {
                return ChainstateLoadVerifyError::ERROR_BLOCK_FROM_FUTURE;
            }

            if (!CVerifyDB().VerifyDB(
                    *chainstate, chainman.GetConsensus(), chainstate->CoinsDB(),
                    check_level,
                    check_blocks)) {
                return ChainstateLoadVerifyError::ERROR_CORRUPTED_BLOCK_DB;
            }
            // SYSCOIN
            chainstate->ResetBlockFailureFlags(nullptr);
        }
    }

    return std::nullopt;
}
} // namespace node
