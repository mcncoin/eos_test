#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/currency.hpp>

#include <iostream>
#include <list>

using namespace eosio;
using namespace std;

enum EN_STATUS
{
	EN_WAITING = 0,
	EN_GAMING = 1,
};

// game i64
/// @abi table
struct game
{
	account_name host;	
	//list<account_name> guests;	// 게임 참여자들 <- list는 에러가 뜸
	vector<account_name> guests;
	
	asset deposit;
	
	//EN_STATUS nStatus;			// 0 = wait, 1 = gaming		
	uint8_t nStatus;			// 0 = wait, 1 = gaming		
	uint8_t nRound;				// 라운드

	uint64_t time_start;		// 시작 시간
	uint64_t time_end;			// 마치는 시간
	uint64_t nTargetNum;		// 타겟 숫자
	
	uint64_t primary_key() const { return host; }
	
	EOSLIB_SERIALIZE( game, (host) (guests) (deposit) (nStatus) (nRound) (time_start) (time_end) (nTargetNum) )
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





const int8_t g_nMax = 100;					// 최대 랜덤 수치
const uint64_t g_uTimeGap_End = 300;		// 시작, 끝 시간 차이(초)

class work02 : public eosio::contract 
{
public:
	using contract::contract;
	
	uint64_t gettime( ) 
	{
		uint64_t utime_sec = now();
		print( " now time ; ", utime_sec );
		return utime_sec;
	}
	
	uint64_t gettime_end(uint64_t utime_start)
	{
		return utime_start + g_uTimeGap_End;
	}
	
	/// @abi action	
	void hi( account_name user )
	{
		print( "Hello, ", name{user} );		
	}
	
	uint8_t rand( int8_t nMax )
	{
		checksum256 result;
		auto mixedBlock = tapos_block_prefix() * tapos_block_num();
		const char *mixedChar = reinterpret_cast<const char *>(&mixedBlock);
		sha256( (char *)mixedChar, sizeof(mixedChar), &result);
		const char *p64 = reinterpret_cast<const char *>(&result);
		uint8_t r = (abs((int8_t)p64[0]) % ( nMax )) + 1;
		return r;
	}
	
	/// @abi action 
	void creategame( const account_name host, const asset& quantity )
	{
		require_auth( host );		
		
		games gametable( _self, _self );		
		auto existing = gametable.find( host );
		eosio_assert( existing == gametable.end(), "host created a game already." );
		
		gametable.emplace(_self, [&]( auto& onegame )
		{
			onegame.host = host;
			onegame.guests.push_back(host);
			
			onegame.deposit = quantity;
			
			//onegame.nStatus = 0;
			onegame.nStatus = EN_WAITING;			
			onegame.nRound = 0;			
			
			onegame.time_start = 0;
			onegame.time_end = 0;
			onegame.nTargetNum = 0;
		});
		
		betting( host, quantity );		
		
		print(name{host}, " Created Game");
		return;
	}
	
	// 게임 상태를 게임중으로 바꿈
	/// @abi action 
	void setstartgame( const account_name host )
	{
		require_auth( host );
		
		games gametable( _self, _self );		
		auto existing = gametable.find( host );		
		eosio_assert( existing == gametable.end(), "not found gametable." );
		
		gametable.modify( existing, 0, [&]( auto& onegame )
		{
			//onegame.nStatus = 1;
			onegame.nStatus = EN_GAMING;
			
			onegame.time_start = gettime();
			onegame.time_end = gettime_end( onegame.time_start );
			onegame.nTargetNum = rand( g_nMax );
		});
		
		return;
	}
	
	// 게임 상태를 대기중으로 바꿈
	void setendgame( const account_name host )
	{
		require_auth( host );
		
		games gametable( _self, _self );		
		auto existing = gametable.find( host );		
		eosio_assert( existing == gametable.end(), "not found gametable." );
		
		gametable.modify( existing, 0, [&]( auto& onegame )
		{
			//onegame.nStatus = 0;
			onegame.nStatus = EN_WAITING;
			
			onegame.time_start = 0;
			onegame.time_end = 0;
			onegame.nTargetNum = 0;
		});
		
		return;
	}
	
	/// @abi action 
	void joingame( const account_name host, const account_name guest, const asset& quantity )
	{
		require_auth( guest );		
		games gametable( _self, _self );
		
		auto existing = gametable.find( host );
		
		eosio_assert( existing != gametable.end(), "host didn't create a game" );
		// 게임에 이미 참여하고 있는지 체크
		if( CheckJoinUser( host, guest ) == true )
		{
			print(name{guest}, " joined ", name{host}, "'sGame already.");
			print(" failed join game. ");
			return;
		}
		
		gametable.modify( existing, 0, [&]( auto& onegame )
		{
			onegame.deposit += quantity;			
			onegame.guests.push_back(guest);
		});
		
		betting( guest, existing->deposit );
		print(name{guest}, " joined ", name{host}, "'sGame");
		return;
	}
	
	bool CheckNextRound( const account_name host )
	{
		bool bRet = false;
		
		return bRet;
	}
	
	bool CheckJoinUser( const account_name host, const account_name guest )
	{
		bool bRet = false;
		games gametable( _self, _self );
		auto existing = gametable.find( host );
		eosio_assert( existing == gametable.end(), "not found gametable." );
		
		for ( auto iter = existing->guests.begin() ; iter != existing->guests.end() ; ++iter)
		{
			if( guest == *iter )
			{
				bRet = true;
				break;
			}
		}
		
		return bRet;		
	}
	
	/// @abi action 
	void rolldice( const account_name host, const account_name player, const asset& quantity )
	{
		require_auth( player );
		games gametable( _self, _self );
		auto existing = gametable.find( host );
		eosio_assert( existing != gametable.end(), "host didn't create a game" );
		
		uint64_t number = rand( g_nMax );
		
		auto deposit = existing->deposit + quantity;
		print( name{player}, " rolled dice! number : ", number, " total deposit : ", deposit );
		
		print( " ||||| Target number : ", existing->nTargetNum );
		
		gametable.modify( existing, 0, [&]( auto& onegame )
		{			
			onegame.deposit += quantity;
		});
		
		betting( player, quantity );
		
		if( number == existing->nTargetNum )
		{
			//account_name winner = ( player == host ? existing->guest : host );
			account_name winner = player;
			//승자에 대한 처리를 한다			
			getmoney( player, existing->deposit );
			
			print( name{winner}, " is Winner. Get : ", existing->deposit, " EOS" );
			
			gametable.erase( existing );
		}
		
		return;
	}
	
	void getmoney( const account_name from, const asset& quantity )
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

///*
#define EOSIO_ABI2( TYPE, MEMBERS ) \
extern "C" \
{ \
	void apply( uint64_t receiver, uint64_t code, uint64_t action ) \
	{ \
		auto self = receiver; \
		if( action == N(onerror)) \
		{ \
			eosio_assert( code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); \
		} \
		if( code == self || action == N(onerror) ) \
		{ \
			TYPE thiscontract( self ); \
			switch( action ) \
			{ \
				EOSIO_API( TYPE, MEMBERS ) \
			} \
		} \
		else \
		{ \
			TYPE thiscontract( self ); \
			switch( action ) \
			{ \
				case N(transfer): \
					return thiscontract.transferact(receiver, code); \
			} \
		} \
	} \
} \

//*/
EOSIO_ABI2( work02, (hi) (creategame) (joingame) (rolldice) (deletegame) (setstartgame) )





















