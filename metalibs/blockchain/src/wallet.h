#ifndef WALLET_H
#define WALLET_H

#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <transaction.h>
#include <xxhash.h>

class Wallet;

class WalletMap {
private:
    struct string_hasher {
        std::size_t operator()(const std::string& k) const
        {
            return XXH64(k.data(), k.size(), 0);
        }
    };

    std::unordered_map<std::string, Wallet*, string_hasher> wallet_map;

    std::unordered_set<Wallet*> changed_wallets;

public:
    Wallet* get_wallet(const std::string&);

    auto begin() { return wallet_map.begin(); }
    auto end() { return wallet_map.end(); }

    bool empty() { return wallet_map.empty(); }

    void apply_changes();
    void clear_changes();

private:
    Wallet* wallet_factory(const std::string&);
};

class Wallet {
protected:
    std::unordered_set<Wallet*>& changed_wallets;

    uint64_t transaction_id = 1;
    uint64_t balance = 0;

    uint64_t real_transaction_id = 1;
    uint64_t real_balance = 0;

public:
    explicit Wallet(std::unordered_set<Wallet*>&);
    Wallet(const Wallet&) = delete;
    Wallet(Wallet&&) = delete;
    Wallet& operator=(const Wallet& other) = delete;
    Wallet& operator=(Wallet&& other) = delete;

    virtual ~Wallet() = default;

    uint64_t get_value();

    virtual std::tuple<uint64_t, uint64_t, std::string> serialize() = 0;
    virtual bool initialize(uint64_t, uint64_t, const std::string&);

    virtual void add(uint64_t value);
    virtual uint64_t sub(Wallet* other, TX const* tx, uint64_t real_fee);

    virtual bool try_apply_method(Wallet* other, TX const* tx) = 0;

    virtual void apply();
    virtual void clear();
};

class CommonWallet : public Wallet {
private:
    struct WalletAdditions {
        bool founder = false;
        uint64_t used_limit = 0;
        uint64_t limit = 0;

        uint64_t delegated_from_sum = 0; // сумма средств делегированных этому кошельку
        uint64_t delegated_to_sum = 0; // сумма средств делегированных этим кошельком

        uint64_t state = 0;
        uint64_t trust = 2;

        std::deque<std::pair<std::string, uint64_t>> delegated_from; // монеты делегированные с других кошельков
        std::deque<std::pair<std::string, uint64_t>> delegate_to; // монеты делегированные другим кошелькам

        std::deque<std::pair<std::string, uint64_t>> delegated_from_daly_snapshot; // монеты делегированные с других кошельков
        std::deque<std::pair<std::string, uint64_t>> delegate_to_daly_snapshot; // монеты делегированные другим кошелькам
    };

    WalletAdditions* addition = nullptr;
    WalletAdditions* real_addition = nullptr;

private:
    bool try_delegate(Wallet* other, TX const* tx);
    bool try_undelegate(Wallet* other, TX const* tx);
    bool register_node(Wallet* other, TX const* tx);

    uint64_t get_balance();

public:
    explicit CommonWallet(std::unordered_set<Wallet*>&);
    CommonWallet(const CommonWallet&) = delete;
    CommonWallet(CommonWallet&&) = delete;
    CommonWallet& operator=(const CommonWallet& other) = delete;
    CommonWallet& operator=(CommonWallet&& other) = delete;

    ~CommonWallet() override;

    uint64_t get_delegated_from_sum();

    uint64_t get_state();
    void set_state(uint64_t);

    uint64_t get_trust();
    void set_trust(uint64_t);
    void add_trust();
    void sub_trust();

    std::deque<std::pair<std::string, uint64_t>> get_delegate_to_list();
    std::deque<std::pair<std::string, uint64_t>> get_delegated_from_list();

    void apply_delegates();

    std::tuple<uint64_t, uint64_t, std::string> serialize() override;
    bool initialize(uint64_t, uint64_t, const std::string& json) override;

    uint64_t sub(Wallet* other, TX const* tx, uint64_t real_fee) override;

    bool try_apply_method(Wallet* other, TX const* tx) override;

    void apply() override;
    void clear() override;
};

class DecentralizedApplication : public Wallet {
private:
    uint64_t init_nonce = 0;

    uint64_t mhc_per_day = 0;
    std::map<std::string, uint64_t> host;

    uint64_t real_mhc_per_day = 0;
    std::map<std::string, uint64_t> real_host;

public:
    explicit DecentralizedApplication(std::unordered_set<Wallet*>&);
    DecentralizedApplication(const DecentralizedApplication&) = delete;
    DecentralizedApplication(DecentralizedApplication&&) = delete;
    DecentralizedApplication& operator=(const DecentralizedApplication& other) = delete;
    DecentralizedApplication& operator=(DecentralizedApplication&& other) = delete;
    ~DecentralizedApplication() override = default;

    bool initialize(uint64_t);

    std::tuple<uint64_t, uint64_t, std::string> serialize() override;
    bool initialize(uint64_t, uint64_t, const std::string&) override;

    bool try_apply_method(Wallet* other, TX const* tx) override;

    bool try_dapp_create(TX const* tx);
    bool try_dapp_modify(TX const* tx);
    bool try_dapp_add_host(TX const* tx);
    bool try_dapp_del_host(TX const* tx);

    std::map<std::string, uint64_t> make_dapps_host_rewards_list();

    void apply() override;
    void clear() override;
};

#endif // WALLET_H
