#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

namespace defivault {

    using namespace eosio;

    // reference
    const name id = "defivault"_n;
    const name code = "vault.defi"_n;
    const std::string description = "Defibox Vault";
    const name token_code = "stoken.defi"_n;

    struct [[eosio::table]] collaterals_row {
        uint64_t    id;
        name        deposit_contract;
        symbol      deposit_symbol;
        symbol      issue_symbol;
        asset       last_income;
        asset       total_income;
        uint16_t    income_ratio;
        asset       min_quantity;
        name        fees_account;
        uint16_t    release_fees;
        uint16_t    refund_ratio;

        uint64_t primary_key() const { return id; }
    };
    typedef eosio::multi_index< "collaterals"_n, collaterals_row > collaterals;

    struct [[eosio::table]] stats_row {
        asset    supply;
        asset    max_supply;
        name     issuer;

        uint64_t primary_key()const { return supply.symbol.code().raw(); }
    };
    typedef eosio::multi_index< "stat"_n, stats_row > stats;

    struct [[eosio::table]] account_row {
        asset    balance;

        uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index< "accounts"_n, account_row > accounts;

     struct [[eosio::table("rexbal")]] rex_balance {
        uint8_t     version;
        name        owner;
        asset       vote_stake;
        asset       rex_balance;
        int64_t     matured_rex;
        vector<pair<time_point_sec, int64_t>> rex_maturities;

        uint64_t primary_key() const { return owner.value; }
    };
    typedef eosio::multi_index< "rexbal"_n, rex_balance > rexbal;

    struct [[eosio::table("rexpool")]] rex_pool {
        uint8_t     version;
        asset       total_lent;
        asset       total_unlent;
        asset       total_rent;
        asset       total_lendable;
        asset       total_rex;
        asset       namebid_proceeds;
        uint64_t    loan_num;

        uint64_t primary_key() const { return loan_num; }
    };
    typedef eosio::multi_index< "rexpool"_n, rex_pool > rexpool;

    //Get EOS equivalent for REX resources for {account} account
    static asset get_rex_value(const name& account) {

        asset eos {0, symbol{"EOS",4}};

        dmdvaults::legacy::rexbal _rexbal( "eosio"_n, "eosio"_n.value );
        auto rexbalit = _rexbal.find(account.value);
        if(rexbalit == _rexbal.end()) return eos;

        dmdvaults::legacy::rexpool _rexpool( "eosio"_n, "eosio"_n.value );
        if(_rexpool.begin() == _rexpool.end()) return eos;
        auto pool = *_rexpool.begin();

        double_t price = static_cast<double_t>(pool.total_unlent.amount + pool.total_lent.amount) / pool.total_rex.amount;

        eos.amount = rexbalit->rex_balance.amount * price;

        return eos;
    }

    static collaterals_row get_pair(const symbol sym) {
        collaterals collaterals_tbl( code, code.value );
        for(const auto& collateral : collaterals_tbl) {
            if(collateral.deposit_symbol == sym) {
                return collateral;
            }
        }
        check(false, "defivault: pair not found");
        return collaterals_row{};
    }

    static asset get_supply(const name code, const symbol sym) {
        stats statstable( code, sym.code().raw() );
        const auto& st = statstable.get( sym.code().raw(), "defivault::get_supply: symbol supply doesn't exist" );
        return st.supply;
    }

    static asset get_balance(const name code, const name owner, const symbol sym) {
        accounts accountstable( code, owner.value );
        const auto& ac = accountstable.get( sym.code().raw(), "defivault::get_balance: symbol balance does not exist" );
        return ac.balance;
    }

    /**
     * ## STATIC `get_amount_out`
     *
     * Return conversion amount
     *
     * ### params
     *
     * - `{asset} in` - in asset
     * - `{symbol} sym_out` - out symbol
     *
     * ### returns
     *
     * - `{asset}` - asset
     *
     * ### example
     *
     * ```c++
     * const asset in = {5'0000, symbol{"EOS",4}};
     * const symbol sym_out = {"SEOS", 4}
     *
     * const auto out = defivault::get_amount_out( in, sym_out );
     * // out => "4.3000 SEOS"
     * ```
     */
    static asset get_amount_out(const asset in, const symbol sym_out )
    {
        const auto pair = get_pair(in.symbol);
        check(pair.deposit_symbol == in.symbol, "defivault: only depost supported");
        check(pair.issue_symbol == sym_out, "defivault: symbol out should be stoken");

        const auto stoken_supply = get_supply(token_code, pair.issue_symbol);
        auto depo_balance = get_balance(pair.deposit_contract, code, pair.deposit_symbol);
        if(in.symbol == symbol{"EOS",4}) depo_balance += get_rex_value(code);

        double rate = (double) stoken_supply.amount / (double) depo_balance.amount;
        double out_amount = (double) in.amount * rate;
        return { static_cast<int64_t>(out_amount), sym_out };
    }

}