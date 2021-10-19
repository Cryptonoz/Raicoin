#pragma once
#include <unordered_map>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/blocks.hpp>
#include <rai/secure/util.hpp>
#include <rai/secure/store.hpp>

namespace rai
{
class Ledger;

class RepWeightOpration
{
public:
    RepWeightOpration(bool, const rai::Account&, const rai::Amount&);

private:
    friend class rai::Ledger;

    bool add_;
    rai::Account representative_;
    rai::Amount weight_;
};

class Transaction
{
public:
    Transaction(rai::ErrorCode&, rai::Ledger&, bool);
    Transaction(const rai::Transaction&) = delete;
    ~Transaction();
    rai::Transaction& operator=(const rai::Transaction&) = delete;
    void Abort();

private:
    friend class rai::Ledger;

    rai::Ledger& ledger_;
    bool write_;
    bool aborted_;
    rai::MdbTransaction mdb_transaction_;
    std::vector<rai::RepWeightOpration> rep_weight_operations_;

};

class Iterator
{
public:
    Iterator(rai::StoreIterator&&);
    Iterator(const rai::Iterator&) = delete;
    Iterator(rai::Iterator&&);

    rai::Iterator& operator++();
    bool operator==(const rai::Iterator&) const;
    bool operator!=(const rai::Iterator&) const;

private:
    friend class rai::Ledger;

    rai::StoreIterator store_it_;

};

class AccountInfo
{
public:
    AccountInfo();
    AccountInfo(rai::BlockType, const rai::BlockHash&); // first block
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    bool Confirmed(uint64_t) const;
    bool Valid() const;
    bool Restricted() const;

    rai::BlockType type_;
    uint16_t forks_;
    uint64_t head_height_;
    uint64_t tail_height_;
    uint64_t confirmed_height_;
    rai::BlockHash head_;
    rai::BlockHash tail_;
};

class AliasInfo
{
public:
    AliasInfo();
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t head_;
    uint64_t name_valid_;
    uint64_t dns_valid_;
    std::vector<uint8_t> name_;
    std::vector<uint8_t> dns_;
};

class AliasBlock
{
public:
    AliasBlock();
    AliasBlock(uint64_t, const rai::BlockHash&, int32_t);
    AliasBlock(uint64_t, const rai::BlockHash&, int32_t, uint8_t,
               const std::vector<uint8_t>&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t previous_;
    rai::BlockHash hash_;
    int32_t status_; // rai::ErrorCode
    uint8_t op_;
    std::vector<uint8_t> value_;
};

class AliasIndex
{
public:
    AliasIndex() = default;
    AliasIndex(const rai::Prefix&, const rai::Account&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Prefix prefix_;
    rai::Account account_;
};

class ReceivableInfo
{
public:
    ReceivableInfo() = default;
    ReceivableInfo(const rai::Account&, const rai::Amount&, uint64_t);
    bool operator>(const rai::ReceivableInfo&) const;
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account source_;
    rai::Amount amount_;
    uint64_t timestamp_;
};

class RewardableInfo
{
public:
    RewardableInfo() = default;
    RewardableInfo(const rai::Account&, const rai::Amount&, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    
    rai::Account source_;
    rai::Amount amount_;
    uint64_t valid_timestamp_;
};

class WalletInfo
{
public:
    WalletInfo();
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint32_t version_;
    uint32_t index_;
    uint32_t selected_account_id_;
    rai::uint256_union salt_;
    rai::uint256_union key_;
    rai::uint256_union seed_;
    rai::uint256_union check_;
};

class WalletAccountInfo
{
public:
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint32_t index_;
    rai::PrivateKey private_key_;
    rai::PublicKey public_key_;
};

enum class MetaKey : uint32_t
{
    VERSION            = 0,
    SELECTED_WALLET_ID = 1,
};

typedef std::multimap<rai::ReceivableInfo, rai::BlockHash,
                      std::greater<rai::ReceivableInfo>>
    ReceivableInfos;

typedef std::multimap<rai::ReceivableInfo,
                      std::pair<rai::Account, rai::BlockHash>,
                      std::greater<rai::ReceivableInfo>>
    ReceivableInfosAll;

enum class ReceivableInfosType
{
    CONFIRMED     = 0,
    NOT_CONFIRMED = 1,
    ALL           = 2
};

class RichListEntry
{
public:
    rai::Account account_;
    rai::Amount balance_;
};

typedef boost::multi_index_container<
    rai::RichListEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_non_unique<
            boost::multi_index::member<rai::RichListEntry, rai::Amount,
                                       &rai::RichListEntry::balance_>,
            std::greater<rai::Amount>>,
        boost::multi_index::hashed_unique<boost::multi_index::member<
            rai::RichListEntry, rai::Account, &rai::RichListEntry::account_>>>>
    RichList;

class DelegatorListEntry
{
public:
    rai::Account account_;
    rai::Account rep_;
    rai::Amount weight_;
    rai::BlockType type_;
};

typedef boost::multi_index_container<
    rai::DelegatorListEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
            boost::multi_index::member<rai::DelegatorListEntry, rai::Account,
                                       &rai::DelegatorListEntry::account_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::member<rai::DelegatorListEntry, rai::Account,
                                       &rai::DelegatorListEntry::rep_>>>>
    DelegatorList;

enum class LedgerType : uint32_t
{
    INVALID = 0,
    NODE    = 1,
    WALLET  = 2,
    APP     = 3,
};

class Ledger
{
public:
    Ledger(rai::ErrorCode&, rai::Store&, rai::LedgerType, bool = false,
           bool = false);

    bool AccountInfoPut(rai::Transaction&, const rai::Account&,
                        const rai::AccountInfo&);
    bool AccountInfoGet(rai::Transaction&, const rai::Account&,
                        rai::AccountInfo&) const;
    bool AccountInfoGet(const rai::Iterator&, rai::Account&,
                        rai::AccountInfo&) const;
    bool AccountInfoDel(rai::Transaction&, const rai::Account&);
    rai::Iterator AccountInfoBegin(rai::Transaction&);
    rai::Iterator AccountInfoEnd(rai::Transaction&);
    bool AccountCount(rai::Transaction&, size_t&) const;
    bool NextAccountInfo(rai::Transaction&, rai::Account&,
                         rai::AccountInfo&) const;
    bool AliasInfoPut(rai::Transaction&, const rai::Account&,
                      const rai::AliasInfo&);
    bool AliasInfoGet(rai::Transaction&, const rai::Account&,
                      rai::AliasInfo&) const;
    bool AliasBlockPut(rai::Transaction&, const rai::Account&, uint64_t,
                       const rai::AliasBlock&);
    bool AliasBlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                       rai::AliasBlock&) const;
    bool AliasIndexPut(rai::Transaction&, const rai::AliasIndex&);
    bool AliasIndexDel(rai::Transaction&, const rai::AliasIndex&);
    bool AliasIndexGet(rai::Transaction&, rai::AliasIndex&) const;
    bool AliasIndexGet(const rai::Iterator&, rai::AliasIndex&) const;
    rai::Iterator AliasIndexLowerBound(rai::Transaction&,
                                       const rai::AliasIndex&);
    rai::Iterator AliasIndexUpperBound(rai::Transaction&,
                                       const rai::AliasIndex&);
    rai::Iterator AliasIndexLowerBound(rai::Transaction&, const rai::Prefix&);
    rai::Iterator AliasIndexUpperBound(rai::Transaction&, const rai::Prefix&,
                                       size_t = 64);
    bool AliasDnsIndexPut(rai::Transaction&, const rai::AliasIndex&);
    bool AliasDnsIndexDel(rai::Transaction&, const rai::AliasIndex&);
    bool AliasDnsIndexGet(rai::Transaction&, rai::AliasIndex&) const;
    bool AliasDnsIndexGet(const rai::Iterator&, rai::AliasIndex&) const;
    rai::Iterator AliasDnsIndexLowerBound(rai::Transaction&,
                                          const rai::AliasIndex&);
    rai::Iterator AliasDnsIndexUpperBound(rai::Transaction&,
                                          const rai::AliasIndex&);
    rai::Iterator AliasDnsIndexLowerBound(rai::Transaction&,
                                          const rai::Prefix&);
    rai::Iterator AliasDnsIndexUpperBound(rai::Transaction&, const rai::Prefix&,
                                          size_t = 64);
    bool BlockPut(rai::Transaction&, const rai::BlockHash&, const rai::Block&);
    bool BlockPut(rai::Transaction&, const rai::BlockHash&, const rai::Block&,
                  const rai::BlockHash&);
    bool BlockGet(rai::Transaction&, const rai::BlockHash&,
                  std::shared_ptr<rai::Block>&) const;
    bool BlockGet(rai::Transaction&, const rai::BlockHash&,
                  std::shared_ptr<rai::Block>&, rai::BlockHash&) const;
    bool BlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                  std::shared_ptr<rai::Block>&) const;
    bool BlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                  std::shared_ptr<rai::Block>&, rai::BlockHash&) const;
    bool BlockDel(rai::Transaction&, const rai::BlockHash&);
    bool BlockCount(rai::Transaction&, size_t&) const;
    bool BlockExists(rai::Transaction&, const rai::BlockHash&) const;
    bool BlockSuccessorSet(rai::Transaction&, const rai::BlockHash&,
                           const rai::BlockHash&);
    bool BlockSuccessorGet(rai::Transaction&, const rai::BlockHash&,
                           rai::BlockHash&) const;
    bool Empty(rai::Transaction&) const;
    bool ForkPut(rai::Transaction&, const rai::Account&, uint64_t,
                 const rai::Block&, const rai::Block&);
    bool ForkGet(rai::Transaction&, const rai::Account&, uint64_t,
                 std::shared_ptr<rai::Block>&,
                 std::shared_ptr<rai::Block>&) const;
    bool ForkGet(const rai::Iterator&, std::shared_ptr<rai::Block>&,
                 std::shared_ptr<rai::Block>&) const;
    bool ForkDel(rai::Transaction&, const rai::Account&);
    bool ForkDel(rai::Transaction&, const rai::Account&, uint64_t);
    bool ForkExists(rai::Transaction&, const rai::Account&, uint64_t) const;
    rai::Iterator ForkLowerBound(rai::Transaction&, const rai::Account&);
    rai::Iterator ForkUpperBound(rai::Transaction&, const rai::Account&);
    bool NextFork(rai::Transaction&, rai::Account&, uint64_t&,
                  std::shared_ptr<rai::Block>&,
                  std::shared_ptr<rai::Block>&) const;
    bool ReceivableInfoPut(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, const rai::ReceivableInfo&);
    bool ReceivableInfoGet(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, rai::ReceivableInfo&) const;
    bool ReceivableInfoExists(rai::Transaction&, const rai::Account&,
                              const rai::BlockHash&) const;
    bool ReceivableInfoGet(const rai::Iterator&, rai::Account&, rai::BlockHash&,
                           rai::ReceivableInfo&) const;

    bool ReceivableInfosGet(rai::Transaction&, rai::ReceivableInfosType,
                            rai::ReceivableInfosAll&,
                            size_t = std::numeric_limits<size_t>::max());
    bool ReceivableInfosGet(rai::Transaction&, const rai::Account&,
                            rai::ReceivableInfosType, rai::ReceivableInfos&,
                            size_t = std::numeric_limits<size_t>::max());
    bool ReceivableInfoDel(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&);
    bool ReceivableInfoCount(rai::Transaction&, size_t&) const;
    rai::Iterator ReceivableInfoLowerBound(rai::Transaction&,
                                           const rai::Account&);
    rai::Iterator ReceivableInfoUpperBound(rai::Transaction&,
                                           const rai::Account&);
    bool RewardableInfoPut(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, const rai::RewardableInfo&);
    bool RewardableInfoGet(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, rai::RewardableInfo&) const;
    bool RewardableInfoGet(const rai::Iterator&, rai::Account&, rai::BlockHash&,
                           rai::RewardableInfo&) const;
    bool RewardableInfoDel(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&);
    rai::Iterator RewardableInfoLowerBound(rai::Transaction&,
                                           const rai::Account&);
    rai::Iterator RewardableInfoUpperBound(rai::Transaction&,
                                           const rai::Account&);
    bool RollbackBlockPut(rai::Transaction&, const rai::BlockHash&,
                          const rai::Block&);
    bool RollbackBlockGet(rai::Transaction&, const rai::BlockHash&,
                          std::shared_ptr<rai::Block>&) const;

    void RepWeightAdd(rai::Transaction&, const rai::Account&,
                      const rai::Amount&);
    void RepWeightSub(rai::Transaction&, const rai::Account&,
                      const rai::Amount&);
    bool RepWeightGet(const rai::Account&, rai::Amount&) const;
    void RepWeightTotalGet(rai::Amount&) const;
    void RepWeightsGet(rai::Amount&,
                       std::unordered_map<rai::Account, rai::Amount>&) const;
    bool SourcePut(rai::Transaction&, const rai::BlockHash&);
    bool SourcePut(rai::Transaction&, const rai::BlockHash&, const rai::Block&);
    bool SourceGet(rai::Transaction&, const rai::BlockHash&,
                   std::shared_ptr<rai::Block>&) const;
    bool SourceDel(rai::Transaction&, const rai::BlockHash&);
    bool SourceExists(rai::Transaction&, const rai::BlockHash&) const;
    bool WalletInfoPut(rai::Transaction&, uint32_t, const rai::WalletInfo&);
    bool WalletInfoGet(rai::Transaction&, uint32_t, rai::WalletInfo&) const;
    bool WalletInfoGetAll(
        rai::Transaction&,
        std::vector<std::pair<uint32_t, rai::WalletInfo>>&) const;
    bool WalletAccountInfoPut(rai::Transaction&, uint32_t, uint32_t,
                              const rai::WalletAccountInfo&);
    bool WalletAccountInfoGet(rai::Transaction&, uint32_t, uint32_t,
                              rai::WalletAccountInfo&) const;
    bool WalletAccountInfoGetAll(
        rai::Transaction&, uint32_t,
        std::vector<std::pair<uint32_t, rai::WalletAccountInfo>>&) const;
    bool SelectedWalletIdPut(rai::Transaction&, uint32_t);
    bool SelectedWalletIdGet(rai::Transaction&, uint32_t&) const;
    bool VersionPut(rai::Transaction&, uint32_t);
    bool VersionGet(rai::Transaction&, uint32_t&) const;
    void UpdateRichList(const rai::Block&);
    std::vector<rai::RichListEntry> GetRichList(uint64_t);
    void UpdateDelegatorList(const rai::Block&);
    std::vector<rai::DelegatorListEntry> GetDelegatorList(const rai::Account&,
                                                          uint64_t);

    rai::ErrorCode UpgradeWallet(rai::Transaction&);
    rai::ErrorCode UpgradeWalletV1V2(rai::Transaction&);

private:
    friend class rai::Transaction;

    bool BlockIndexPut_(rai::Transaction&, const rai::Account&, uint64_t,
                        const rai::BlockHash&);
    bool BlockIndexGet_(rai::Transaction&, const rai::Account&, uint64_t,
                        rai::BlockHash&) const;
    bool BlockIndexDel_(rai::Transaction&, const rai::Account&, uint64_t);
    void RepWeightsCommit_(const std::vector<rai::RepWeightOpration>&);
    rai::ErrorCode InitMemoryTables_(rai::Transaction&);
    void UpdateRichList_(const rai::Account&, const rai::Amount&);
    void UpdateDelegatorList_(const rai::Account&, const rai::Account&,
                              const rai::Amount&, rai::BlockType);

    static uint32_t constexpr BLOCKS_PER_INDEX = 8;
    const rai::Amount RICH_LIST_MINIMUM = rai::Amount(10 * rai::RAI);

    rai::Store& store_;
    mutable std::mutex rep_weights_mutex_;
    rai::Amount total_rep_weight_;
    std::unordered_map<rai::Account, rai::Amount> rep_weights_;

    bool enable_rich_list_;
    mutable std::mutex rich_list_mutex_;
    rai::RichList rich_list_;

    bool enable_delegator_list_;
    mutable std::mutex delegator_list_mutex_;
    rai::DelegatorList delegator_list_;
};
}  // namespace rai