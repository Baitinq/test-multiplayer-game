#include "structs.hpp"

#include <vector>
#include <mutex>
#include <ncurses.h>

//#define DEBUG

int handle_incoming_packet(Player* player, Packet* packet, std::vector<Player*>* players);
int handle_packet_connect(Player* player, PacketConnect* packet, std::vector<Player*>* players);
int handle_packet_disconnect(Player* player, PacketDisconnect* packet, std::vector<Player*>* players);
int handle_packet_move(Player* player, PacketMove* packet, std::vector<Player*>* players);
int handle_packet_server_info(PacketServerInfo* packet);
int add_player(Player* player,  std::vector<Player*>* players);
int remove_player(unsigned int sender_id,  std::vector<Player*>* players);
int initialize_new_client(Player* player, std::vector<Player*>* players);
int send_packet(Player* player, PacketType packet_type, int* sockfd, void* extra_data);
int ping_server(int* sockfd);
int draw_game(std::vector<Player*>* players);
int setup_ncurses();
int players_contain( std::vector<Player*>* players, unsigned int sender_id);
void print_debug_players( std::vector<Player*>* players);
void print_player_info(Player* player);

extern std::vector<Player*> players;
extern int max_players;
extern int fps;
extern const int server;
extern int client_id;
extern bool visual;
extern std::mutex mutex;

int handle_incoming_packet(Player* player, Packet* packet, std::vector<Player*>* players)
{
	int status;
	switch(packet->type)
	{
		case PacketType::packet_connect:
			status = handle_packet_connect(player, &packet->data.packet_connect, players);
			break;
		case PacketType::packet_disconnect:
			status = handle_packet_disconnect(player, &packet->data.packet_disconnect, players);
			break;
		case PacketType::packet_move:
			status = handle_packet_move(player, &packet->data.packet_move, players);
			break;
		case PacketType::packet_ping:
			return -1;
			break;
		case PacketType::packet_server_info:
			status = handle_packet_server_info(&packet->data.packet_server_info);
			break;
		default:
			return 2;
	}

	return status;
}

int handle_packet_connect(Player* player, PacketConnect* packet, std::vector<Player*>* players)
{
	int player_fd;
	char player_ip[20];
	
	strncpy(player_ip, player->ip, sizeof(player_ip));
	player_fd = player->fd;
	memcpy(player, (const char*)&packet->player, sizeof(*player));
	player->fd = player_fd;
	strncpy(player->ip, player_ip, sizeof(player->ip));
	memcpy(&packet->player, (const char*)player, sizeof(packet->player));

	if(add_player(player, players))
		return 1;

	if(server)
	{
		initialize_new_client(player, players); //send the connect packet of all things to client
		
		PacketServerInfo server_info {};
		server_info.max_players = max_players;
		server_info.fps = fps;
		send_packet(player, PacketType::packet_server_info, &player->fd, &server_info);
	}
	
	#ifdef DEBUG
	std::cout << "PacketConnect start -----------" << std::endl;
	print_debug_players(players);
	std::cout << "PacketConnect end -----------" << std::endl;
	#endif
	return 0;
}

int handle_packet_disconnect(Player* player, PacketDisconnect* packet, std::vector<Player*>* players)
{
	remove_player(packet->player.sender_id, players);
	
	#ifdef DEBUG
	std::cout << "PacketDisconnect start -----------" << std::endl;
	print_debug_players(players);
	std::cout << "PacketDisconnect end -----------" << std::endl;
	#endif

	return 0;
}

int handle_packet_move(Player* player, PacketMove* packet, std::vector<Player*>* players)
{
	int index = players_contain(players, player->sender_id);
	if(index != -1)
	{
		//movement checks?
		player->x = packet->x;
		player->y = packet->y;

	#ifdef DEBUG
	std::cout << "PacketMove start -----------" << std::endl;
	print_player_info(player);
	std::cout << "PacketMove end -----------" << std::endl;
	#endif
	}
	return 0;
}

int handle_packet_server_info(PacketServerInfo* packet)
{
	max_players = packet->max_players;
	fps = packet->fps;
	
	return 0;
}

int add_player(Player* player,  std::vector<Player*>* players)
{
	if(players_contain(players, player->sender_id) == -1)
	{
		mutex.lock();
		players->push_back(player);
		if(server && !visual)
			std::cout << "[+] New Player! [" << player->name << "-" << player->sender_id << "] (" << player->ip << ")\t(" << players->size() << "/" << max_players << " players)" <<  std::endl;	
		fflush(stdout);
		mutex.unlock();
	}
	else
	{
		if(server && !visual)
			std::cout << "[!] A player tried to join with the same player_id as another one! [" << player->name << "-" << player->sender_id << "] (" << player->ip << ")" << std::endl;
	}

		

	return 0;
}

int remove_player(unsigned int sender_id,  std::vector<Player*>* players)
{
	int index = players_contain(players, sender_id);
	if(index != -1)
	{
		mutex.lock();
		Player* player = players->at(index);
		close(player->fd);
		players->erase(players->begin() + index);
		if(strlen(player->name))
			if(server && !visual)
				std::cout << "[-] Player left! [" << player->name << "-" << player->sender_id << "] (" << player->ip << ")\t(" << players->size() << "/" << max_players << " players)" <<  std::endl;	
		fflush(stdout);
		mutex.unlock();
		return 0;
	}

	return 1;
}

int initialize_new_client(Player* player, std::vector<Player*>* players)
{
	for(Player* p : *players)
	{
		if(p->sender_id != player->sender_id)
		{
			send_packet(p, PacketType::packet_connect, &player->fd, NULL);
			std::cout << "initialize x: " << p->x << ", and y: " << p->y << std::endl;
			fflush(stdout);
		}
	}

	return 0;
}

int send_packet(Player* player, PacketType packet_type, int* sockfd, void* extra_data)
{
	switch(packet_type)
	{
		case PacketType::packet_connect:
		{
			Packet packet {};
			packet.type = PacketType::packet_connect;
			packet.sender_id = player->sender_id;
			packet.data.packet_connect.player = *player;

			if(send(*sockfd, (const char*)&packet, sizeof(packet), 0) < 0)
				return 1;

			break;
		}
		case PacketType::packet_disconnect:
		{
			Packet packet {};
			packet.sender_id = player->sender_id;
			packet.type = PacketType::packet_disconnect;
			packet.data.packet_disconnect.player = *player;

			if(send(*sockfd, (const char*)&packet, sizeof(packet), 0) < 0)
				return 2;
			break;
		}
		case PacketType::packet_move:
		{
			Packet packet {};
			packet.sender_id = player->sender_id;
			packet.type = PacketType::packet_move;
			packet.data.packet_move.x = player->x;
			packet.data.packet_move.y = player->y;
			packet.data.packet_move.is_color = player->is_color;
			packet.data.packet_move.player = *player;

			if(send(*sockfd, (const char*)&packet, sizeof(packet), 0) < 0)
				return 3;
			break;
		}
		case PacketType::packet_ping:
		{
			Packet packet {};
			packet.sender_id = 0;
			packet.type = PacketType::packet_ping;

			if(send(*sockfd, (const char*)&packet, sizeof(packet), 0) < 0)
				return 4;
			
			break;
		}
		case PacketType::packet_server_info:
	       	{
			PacketServerInfo* original_packet = (PacketServerInfo*)extra_data;
			Packet packet {};
			packet.sender_id = 0;
			packet.type = PacketType::packet_server_info;
			packet.data.packet_server_info.max_players = original_packet->max_players;
			packet.data.packet_server_info.fps = original_packet->fps;

			if(send(*sockfd, (const char*)&packet, sizeof(packet), 0) < 0)
				return 5;
			
			break;
		}	
		default:
			return -1;
	}

	return 0;
}

int ping_server(int* sockfd)
{
	return send_packet(NULL, PacketType::packet_ping, sockfd, NULL);
}

int draw_game(std::vector<Player*>* players)
{
	clear();
	
	//iscolor as well
	mutex.lock();	
	for(Player* p : *players)
	{
		mvaddstr(p->y, p->x, "x");
	}
	mutex.unlock();

	refresh();

	return 0;
}

int setup_ncurses()
{
	initscr();
	clear();
	noecho();
	timeout(0);
	curs_set(0);

	return 0;
}

int players_contain( std::vector<Player*>* players, unsigned int sender_id)
{
	int index = 0;
	mutex.lock();
	for(Player* p : *players)
	{
		if(p->sender_id == sender_id)
		{
			mutex.unlock();
			return index;
		}
		index++;
	}

	mutex.unlock();
	return -1;
}

void print_debug_players( std::vector<Player*>* players)
{
	mutex.lock();
	for(Player* player : *players)
	{
		print_player_info(player);
	}
	mutex.unlock();
}

void print_player_info(Player* player)
{
	std::cout << "--------------------------" << std::endl << std::endl;

	std::cout << "Name: " << player->name << std::endl;
	std::cout << "IP: " << player->ip << std::endl;
	std::cout << "x: " << player->x << std::endl;
	std::cout << "y: " << player->y << std::endl;
	std::cout << "color: " << player->color << std::endl;
	std::cout << "is_color: " << player->is_color << std::endl;
	std::cout << "fd: " << player->fd << std::endl;
	std::cout << "id: " << player->sender_id << std::endl;
	
	std::cout << std::endl <<  "--------------------------"  << std::endl;
	fflush(stdout);
}
