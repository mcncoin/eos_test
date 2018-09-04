#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/currency.hpp>

using namespace eosio;

// game i64
/// @abi table
struct game
{
	account_name host;
	account_name guest;
	uint8_t point;
	asset deposit;
	account_name turn;
	
	uint64_t primary_key() const { return host; }
	EOSLIB_SERIALIZE( game, (host) (guest) (point) (deposit) (turn) )
};
typedef multi_index<N(game), game> games;

/// @abi table balance i64
struct balance
{
	account_name holder;
	asset amount;
	uint64_t primary_key() const { return holder; }
	EOSLIB_SERIALIZE( balance, ( holder ) ( amount ) );
};
typedef multi_index<N(balance), balance> balances;

class work01 : public eosio::contract 
{
public:
	using contract::contract;

	
	/// @abi action
	void hi( account_name user ) 
	{
		print( "Hello, ", name{user} );
	}
	
	uint8_t rand()
	{
		checksum256 result;
		auto mixedBlock = tapos_block_prefix() * tapos_block_num();
		const char *mixedChar = reinterpret_cast<const char *>(&mixedBlock);
		sha256( (char *)mixedChar, sizeof(mixedChar), &result);
		const char *p64 = reinterpret_cast<const char *>(&result);
		uint8_t r = (abs((int8_t)p64[0]) % ( 10 )) + 1;
		return r;
	}
	
	/// @abi action 
	void creategame( const account_name host, const asset& quantity )
	{
		games gametable( _self, _self );
		require_auth( host );		
		
		auto existing = gametable.find( host );
		eosio_assert( existing == gametable.end(), "host created a game already." );
		
		gametable.emplace(_self, [&]( auto& onegame )
		{
			onegame.host = host;
			onegame.guest = 0;
			onegame.point = 0;
			onegame.deposit = quantity;
			onegame.turn = host;			
		});
		
		betting( host, quantity );
		//deposit( host, quantity );
		
		print(name{host}, " Created Game");
		return;
	}
	
	/// @abi action 
	void joingame( const account_name host, const account_name guest, const asset& quantity )
	{
		require_auth( guest );
		
		games gametable( _self, _self );
		
		auto existing = gametable.find( host );
		
		eosio_assert( existing != gametable.end(), "host didn't create a game" );
		eosio_assert( existing->guest == 0, "another guest has already joined" );
		
		gametable.modify( existing, 0, [&]( auto& onegame )
		{
			onegame.guest = guest;
		});
		
		betting( guest, existing->deposit );			
		print(name{guest}, " joined ", name{host}, "'sGame");
		return;
	}
	
	/// @abi action 
	void rolldice( const account_name host, const account_name player )
	{
		require_auth( player );
		games gametable( _self, _self );
		auto existing = gametable.find( host );
		eosio_assert( existing != gametable.end(), "host didn't create a game" );
		eosio_assert( existing->turn == player, "This is not player's turn" );
		
		uint64_t number = rand();
		
		auto point = existing->point + number;
		print( name{player}, " rolled dice! number : ", number, " total score : ", point );
		
		gametable.modify( existing, 0, [&]( auto& onegame )
		{
			onegame.turn = ( player == host ? onegame.guest : host );
			onegame.point = point;
		});
		
		if( point >= 31 )
		{
			account_name winner = ( player == host ? existing->guest : host );
			print( name{winner}, " is Win" );
			
			gametable.erase( existing );				
		}
		
		return;
	}
	
	void deposit( const account_name from, const asset& quantity )
	{
		eosio_assert( quantity.is_valid(), "invalid quantiy" );
		eosio_assert( quantity.amount > 0, "must deposit positive quantity" );
		action
		(
			permission_level
			{
				from, N( active )
			},
			N( eosio.token ), 
			N( transfer ),
			std::make_tuple( from, _self, quantity, std::string("") )
		).send();
		
		return;
	}
	
	void transferact( uint64_t receiver, uint64_t code )
	{
		eosio_assert( code == N( eosio.token ), "I reject your non-eosio.token deposit" );
		
		auto data = unpack_action_data<currency::transfer>();
		if( data.from == _self || data.to != _self )
			return;
		eosio_assert( data.quantity.symbol == string_to_symbol( 4, "EOS" ), "I think you're looking for another contract" );
		eosio_assert( data.quantity.is_valid(), "Are you trying to corrupt me?" );
		eosio_assert( data.quantity.amount > 0, "when pigs fly" );
		
		balances balancetable( _self, _self );
		auto onebalance = balancetable.find( data.from );
		if( onebalance != balancetable.end() )
		{
			balancetable.modify( onebalance, 0, [&]( auto& bal )
			{
				bal.holder = data.from;
				bal.amount += data.quantity;					
			});
		}
		else
		{
			balancetable.emplace( _self, [&]( auto& bal )				
			{
				bal.holder = data.from;
				bal.amount += data.quantity;
			});
		}
		
		print( name{ data.from }, " deposited:     ", data.quantity, "\n" );
		print( "I got EOS!!!!" );
		
		return;
	}
	
	//@abi action
    void deletegame(const account_name host) {
        games gametable(_self,_self);
        
        auto onegame = gametable.find(host);
		eosio_assert( onegame != gametable.end(), "deletegame is failed" );
		
        gametable.erase(onegame);		
		print(name{host} ,"'s  game delete" );      
        // Delete Game
    }
	
	void betting( const account_name player, const asset& quantity )
	{
		balances balancetable( _self, _self );
		auto onebalance = balancetable.find( player );
		
		eosio_assert( onebalance != balancetable.end(), "Player doesn't have deposit history" );
		eosio_assert( quantity <= onebalance->amount, "Player doesn't have enough money" );
		
		balancetable.modify( onebalance, 0, [&]( auto& bal )
		{
			bal.amount -= quantity;
		});
		
		return;
	}
	
	void withdraw( const account_name to, const asset& quantity )
	{
		asset prize = quantity * 2;
		action
		(
			permission_level
			{
				_self, 
				N(active),
			},
			N( eosio.token ),
			N( transfer ),
			std::make_tuple( _self, to, prize, std::string( "" ) )
		).send();
		
		return;
	}
	
};

#define EOSIO_ABI2( TYPE, MEMBERS ) \
extern "C" \
{ \
	void apply( uint64_t receiver, uint64_t code, uint64_t action ) \
	{ \
		auto self = receiver; \
		if( action == N(onerror)) \
		{ \
			/* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
			eosio_assert( code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); \
		} \
		if( code == self || action == N(onerror) ) \
		{ \
			TYPE thiscontract( self ); \
			switch( action ) \
			{ \
				EOSIO_API( TYPE, MEMBERS ) \
			} \
		/* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
		} \
		else \
		{ \
			TYPE thiscontract( self ); \
			switch( action ) \
			{ \
				case N(transfer): \
					return thiscontract.transferact(receiver, code); \
			} \
			/* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
		} \
	} \
} \

EOSIO_ABI2( work01, (creategame) (joingame) (rolldice) (deletegame) )





















