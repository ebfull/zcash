class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    uint256 hashBestAnchor_;
    std::map<uint256, CCoins> map_;
    std::map<uint256, ZCIncrementalMerkleTree> mapAnchors_;
    std::map<uint256, bool> mapNullifiers_;

public:
    CCoinsViewTest() {
        hashBestAnchor_ = ZCIncrementalMerkleTree::empty_root();
    }

    bool GetAnchorAt(const uint256& rt, ZCIncrementalMerkleTree &tree) const {
        if (rt == ZCIncrementalMerkleTree::empty_root()) {
            ZCIncrementalMerkleTree new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, ZCIncrementalMerkleTree>::const_iterator it = mapAnchors_.find(rt);
        if (it == mapAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetNullifier(const uint256 &nf) const
    {
        std::map<uint256, bool>::const_iterator it = mapNullifiers_.find(nf);

        if (it == mapNullifiers_.end()) {
            return false;
        } else {
            // The map shouldn't contain any false entries.
            assert(it->second);
            return true;
        }
    }

    uint256 GetBestAnchor() const { return hashBestAnchor_; }

    bool GetCoins(const uint256& txid, CCoins& coins) const
    {
        std::map<uint256, CCoins>::const_iterator it = map_.find(txid);
        if (it == map_.end()) {
            return false;
        }
        coins = it->second;
        if (coins.IsPruned() && insecure_rand() % 2 == 0) {
            // Randomly return false in case of an empty entry.
            return false;
        }
        return true;
    }

    bool HaveCoins(const uint256& txid) const
    {
        CCoins coins;
        return GetCoins(txid, coins);
    }

    uint256 GetBestBlock() const { return hashBestBlock_; }

    bool BatchWrite(CCoinsMap& mapCoins,
                    const uint256& hashBlock,
                    const uint256& hashAnchor,
                    CAnchorsMap& mapAnchors,
                    CNullifiersMap& mapNullifiers)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            map_[it->first] = it->second.coins;
            if (it->second.coins.IsPruned() && insecure_rand() % 3 == 0) {
                // Randomly delete empty entries on write.
                map_.erase(it->first);
            }
            mapCoins.erase(it++);
        }
        for (CAnchorsMap::iterator it = mapAnchors.begin(); it != mapAnchors.end(); ) {
            if (it->second.entered) {
                std::map<uint256, ZCIncrementalMerkleTree>::iterator ret =
                    mapAnchors_.insert(std::make_pair(it->first, ZCIncrementalMerkleTree())).first;

                ret->second = it->second.tree;
            } else {
                mapAnchors_.erase(it->first);
            }
            mapAnchors.erase(it++);
        }
        for (CNullifiersMap::iterator it = mapNullifiers.begin(); it != mapNullifiers.end(); ) {
            if (it->second.entered) {
                mapNullifiers_[it->first] = true;
            } else {
                mapNullifiers_.erase(it->first);
            }
            mapNullifiers.erase(it++);
        }
        mapCoins.clear();
        mapAnchors.clear();
        mapNullifiers.clear();
        hashBestBlock_ = hashBlock;
        hashBestAnchor_ = hashAnchor;
        return true;
    }

    bool GetStats(CCoinsStats& stats) const { return false; }
};

