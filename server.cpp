#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "utils.hpp"

#define LOCALHOST (char*) "127.0.0.1"
#define PUBLIC (char*) "0.0.0.0"

#define IP PUBLIC
#define PORT (char*) "4444"

int main(int argc, char** argv);
int game_server(int* max_players,  std::vector<Player*>* players, const char* ip, const char* port, int* sockfd);
void* server_handler(void* psockaddr);
void* client_handler(void* client_fd);
int waiting_loop(std::vector<Player*>* players, int fps, bool* visual);
int notify_players( std::vector<Player*>* players, Player* sender_player, Packet* packet);
int manual_disconnect(Player* player,  std::vector<Player*>* players);

 std::vector<Player*> players;
int max_players;
const int server = 1;
int client_id = 0;
bool visual = false;
int fps = 10;
int sockfd;
std::mutex mutex;

int main(int argc, char** argv)
{
	max_players = -1;
	
	char* port = PORT;

	if(argv[1])
		max_players = atoi(argv[1]);
	if(argv[2])
		port = argv[2];
	if(argv[3])
		if(!strcmp(argv[3], "-visual"))
			visual = true;

	std::cout << "Starting server with access for " << max_players << " players. (" << IP << ":" << port << ")" << std::endl;
	
	int result = game_server(&max_players, &players, IP, port, &sockfd);
	if(result)
		std::cout << "[!] Server failed to start! [" << IP << ":" << port << "] (" << result << ")" << std::endl;
	else	
		waiting_loop(&players, fps, &visual);

	return 0;
}

int game_server(int* max_players,  std::vector<Player*>* players, const char* ip, const char* port, int* sockfd)
{
	*sockfd = socket(AF_INET, SOCK_STREAM , 0);
	if(*sockfd < 0)
		return 1;
	
	sockaddr_in sockaddr;
  	sockaddr.sin_family = AF_INET;
  	sockaddr.sin_addr.s_addr = inet_addr(ip);
  	sockaddr.sin_port = htons(atoi(port));

	if(bind(*sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)))
		return 2;

	if (listen(*sockfd, 1))
		return 3;

	if(visual)
	{
		pthread_t server_thread;
		if(pthread_create(&server_thread, NULL, server_handler, (void*)&sockaddr))
			return 5;
	}
	else
		server_handler((void*)&sockaddr);

	return 0;
}

void* server_handler(void* psockaddr)
{
	sockaddr_in* sockaddr = (sockaddr_in*)psockaddr;
	socklen_t addrlen = sizeof(*sockaddr);
	int connection;
       	while((connection = accept(sockfd, (struct sockaddr*)sockaddr, &addrlen)))
	{
	  	if (connection < 0)
			return 0;
		
		if((int)(&players)->size() == *(&max_players))
		{
			std::cout << "[-] Player limit has been reached. Incoming player has been kicked" << std::endl;
			fflush(stdout);
			close(connection);
			continue;
		}	

		pthread_t sniffer_thread;
		if(pthread_create(&sniffer_thread, NULL, client_handler, (void*)(intptr_t)connection))
			return 0;
	}

	return 0;
}

void* client_handler(void* pclient_fd)
{
	struct sockaddr_in client;
	socklen_t addrlen = sizeof(client);
	getpeername((intptr_t)pclient_fd, (struct sockaddr*)&client, &addrlen);

	Player player {};
	player.fd = (intptr_t)pclient_fd; 
	strncpy(player.ip, inet_ntoa(client.sin_addr), sizeof(player.ip));

	Packet incoming_packet {};

	int read_size = 0;
	while((read_size = recv(player.fd, &incoming_packet, sizeof(incoming_packet), 0) > 0))	
	{
		int result =  handle_incoming_packet(&player, &incoming_packet, &players);
		if(result > 0)
		{
			if(strlen(player.name))
				std::cout << "[!] Invalid packet from player! [" << player.name << "] (" << player.ip << ")" << std::endl; 
			else
				std::cout << "[!] Someone tried to connect without using the game's client! (" << player.ip << ")" << std::endl;
				
			fflush(stdout);
			break;
		}
		
		if(result != -1)
			notify_players(&players, &player, &incoming_packet);
	}
	
	manual_disconnect(&player, &players);
	close((intptr_t)pclient_fd);

	return 0;
}

int waiting_loop(std::vector<Player*>* players, int fps, bool* visual)
{
	if(*visual)
	{
		setup_ncurses();
		
		while(1)
		{
			if(getch() == 'q')
				goto end;

			draw_game(players);
			usleep(1000000 / fps);
		}

		end:
		endwin();
	}

	return 0;
}

int notify_players( std::vector<Player*>* players, Player* sender_player, Packet* packet)
{
	for(Player* player : *players)
	{
		if(player->sender_id != sender_player->sender_id)
			if(send(player->fd, (const char*)packet, sizeof(*packet), 0) < 0)
				manual_disconnect(player, players);
	}

	return 0;
}

int manual_disconnect(Player* player,  std::vector<Player*>* players)
{
	Packet packet {};
	packet.type = PacketType::packet_disconnect;
	packet.sender_id = player->sender_id;
	packet.data.packet_disconnect.player = *player;
	
	int result =  handle_incoming_packet(player, &packet, players);
	notify_players(players, player, &packet);
	
	return result;
}
