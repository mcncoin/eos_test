#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/currency.hpp>

using namespace std;
using namespace eosio;

class br31 : public eosio::contract {
  public:
    using contract::contract;
        
    //@abi action
    void creategame(const account_name host, const asset& quantity)
    {
        games gametable(_self,_self);
        require_auth(host);
        auto existing = gametable.find( host );
        eosio_assert( existing == gametable.end(), "host already created a game" );
        gametable.emplace(_self,[&](auto& onegame)     
        {
            onegame.host = host;
            onegame.guest = 0;
            onegame.point = 0;
            onegame.turn = host;
            onegame.deposit=quantity;
        });

        betting( host, quantity );

        print(name{host} ," Created Game");
    }
    //@abi action
    void joingame(const account_name host, const account_name guest)
    {
        games gametable(_self,_self);
        require_auth(guest);
        auto existing = gametable.find( host );
        eosio_assert( existing != gametable.end(), "host didn't create a game" );
        eosio_assert( existing->guest == 0, "Another guest has already joined " );

        betting( guest, existing->deposit );

        gametable.modify( existing, 0, [&]( auto& onegame ) {
            onegame.guest = guest;
        });

        print(name{guest}," joined", name{host},"Game");        
    }
    //@abi action
    void rolldice (const account_name host, const account_name player) {
        require_auth(player);
        games gametable(_self,_self);
        auto existing = gametable.find( host );
        eosio_assert( existing != gametable.end(), "host didn't create a game" );
        eosio_assert( existing->turn == player, "it is not player’s turn" );
        
        // Make Random Number
        uint64_t number = rand();
        // Accumulate game point
        auto point = existing->point + number;
        print(name{player} ," rolled dice! number : ", number, " total score : ", point);

        //Update game status
        gametable.modify( existing, 0, [&]( auto& onegame ) {
            // trun change
            onegame.turn = (player == host ? onegame.guest : host);
            onegame.point = point;
        });

        // Check game over
        if(point >= 31) {
            // current player lose.
            account_name winner = (player == host ? existing->guest : host);
            print(name{winner} ," is Win" ); 
            withdraw(winner,existing->deposit );     

            // Remove game
            gametable.erase(existing);
        }
        
    }
    //@abi action
    void deletegame(const account_name host) {
        games gametable(_self,_self);

        print(name{host} ,"'s  game delete" );      
        // Delete Game
        auto onegame = gametable.find(host);
        gametable.erase(onegame);
    }

    void betting( const account_name player, const asset& quantity ) {
        balances balancetable(_self, _self);
        auto onebalance = balancetable.find(player);
        eosio_assert( onebalance != balancetable.end(), "Player doesn't have deposit history");
        eosio_assert( quantity <= onebalance->amount, "Player doesn't have enough money");

        balancetable.modify(onebalance, 0, [&](auto& bal){
                bal.amount -= quantity;
        });
        
    }
    //@abi action
    void transferact( uint64_t receiver, uint64_t code )
    {
        eosio_assert(code == N(eosio.token), "I reject your non-eosio.token deposit");

        auto data = unpack_action_data<currency::transfer>();
        if(data.from == _self || data.to != _self) return;

        eosio_assert(data.quantity.symbol == string_to_symbol(4, "EOS"), "I think you're looking for another contract");
        eosio_assert(data.quantity.is_valid(), "Are you trying to corrupt me?");
        eosio_assert(data.quantity.amount > 0, "When pigs fly");

        balances balancetable(_self, _self);
        auto onebalance = balancetable.find(data.from);

        if(onebalance != balancetable.end())
            balancetable.modify(onebalance, 0, [&](auto& bal){
                // Assumption: total currency issued by eosio.token will not overflow asset
                bal.holder = data.from;
                bal.amount += data.quantity;
        });
        else
            balancetable.emplace(_self, [&](auto& bal){
                bal.holder = data.from;
                bal.amount += data.quantity;
        });

        print(name{data.from}, " deposited:       ", data.quantity, "\n");
        print("I got EOS!!!");
    };

    void withdraw( const account_name to, const asset& quantity ) {
        
        asset prize = quantity*2;
        action(
        permission_level{ _self, N(active) },
        N(eosio.token), N(transfer),
        std::make_tuple(_self, to, prize, std::string(""))
        ).send();

    }

    uint8_t rand() { 
        checksum256 result;
        auto mixedBlock = tapos_block_prefix() * tapos_block_num();

        const char *mixedChar = reinterpret_cast<const char *>(&mixedBlock);
        sha256( (char *)mixedChar, sizeof(mixedChar), &result);
        const char *p64 = reinterpret_cast<const char *>(&result);

        uint8_t r = (abs((int8_t)p64[0]) % (10)) + 1;
        return r;
    }

  private:

    /// @abi table game i64
    struct game{
        account_name host;
        account_name guest;
        uint8_t point;
        asset deposit;
        account_name turn;
        uint64_t primary_key() const {return host;}
        EOSLIB_SERIALIZE(game,(host)(guest)(point)(deposit)(turn))
    };

    /// @abi table balance i64
    struct balance {
        account_name holder;
        asset amount;
        uint64_t primary_key() const {return holder;}
        EOSLIB_SERIALIZE(balance,(holder)(amount));
    };

    typedef multi_index<N(balance),balance> balances;
    typedef multi_index<N(game),game> games;

};



#define EOSIO_ABI2( TYPE, MEMBERS ) \
extern "C" { \
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
      auto self = receiver; \
      if( action == N(onerror)) { \
         /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
         eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); \
      } \
      if( code == self || action == N(onerror) ) { \
         TYPE thiscontract( self ); \
         switch( action ) { \
            EOSIO_API( TYPE, MEMBERS ) \
         } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } \
      else { \
         TYPE thiscontract( self ); \
         switch( action ) { \
            case N(transfer): return thiscontract.transferact(receiver, code); \
         } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } \
   } \
} \

EOSIO_ABI2( br31, (creategame) (joingame) (rolldice) (deletegame) )
