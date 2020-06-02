#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.hpp"

#define LOCALHOST (char*) "127.0.0.1"
#define PUBLIC (char*) "0.0.0.0"

#define IP LOCALHOST
#define PORT (char*) "4444"

int main(int argc, char** argv);
int start_game(Player* player, std::vector<Player*>* players, int* sockfd, int fps);
int connect_to_server(char* ip, char* port, Player* player, int* sockfd);
void* packet_recieveing_thread(void* psockfd);
int move(Player* player, char* key, int* sockfd);

std::vector<Player*> players;
int max_players = 0;
int fps = 10;
const int server = 0;
int client_id;
bool visual = true;
Player player {};
std::mutex mutex;

int main(int argc, char** argv)
{
	int sockfd;
	srand(time(0));

	client_id = rand();

	player.sender_id = client_id;
	player.color = 1;
	player.is_color = false;
	player.x = rand() % 7; //improve  this
	player.y = rand() % 7;

	add_player(&player, &players);

	char* ip = IP;
	if(argv[1])
		ip = argv[1];

	char* port = PORT;
	if(argv[2])
		port = argv[2];

	char player_name[20] = "RandomPlayer";
	char num[5];
	sprintf(num, "%d", (int)(time(0) % 9999));
	strncat(player_name, num, sizeof(player_name));
	if(argv[3])
		strncpy(player_name, argv[3], sizeof(player_name));
	strncpy(player.name, player_name, sizeof(player.name));

	int result = connect_to_server(ip, port, &player, &sockfd);
	if(result)
	{
		std::cout << "[!] Failed to connect to the server! [" << ip << ":" << port << "] (" << result << ")" << std::endl;
		return 1;
	}

	pthread_t packet_recieveing_thread_id;
	pthread_create(&packet_recieveing_thread_id, NULL, packet_recieveing_thread, (void*)(intptr_t)sockfd);

	if(start_game(&player, &players, &sockfd, fps))
	{
		std::cout << "[!] Lost connection to server!" << std::endl;
		return 2;
	}

	if(send_packet(&player, PacketType::packet_disconnect, &sockfd, NULL))
		return 3;

	close(sockfd);

	return 0;
}

int start_game(Player* player, std::vector<Player*>* players, int* sockfd, int fps)
{
	int status = 0;
	setup_ncurses();

	char key;
	while(1)
	{
		if((rand() % 100) < 10 && ping_server(sockfd))
		{
			status = 1;
			goto end;
		}	

		key = getch();
		if(move(player, &key, sockfd))
		{
			status = 2;
			goto end;
		}

		draw_game(players);
		usleep(1000000 / fps);
	}

	end:
	endwin();

	return status;
}

int connect_to_server(char* ip, char* port, Player* player, int* sockfd)
{
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket < 0)
		return 1;
	
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = inet_addr(ip);
	sockaddr.sin_port = htons(atoi(port));

	if(connect(*sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)))
		return 2;
	
	std::cout << "[+] Connecting to the game server (" << ip << ":" << port << ")" << std::endl;

	if(send_packet(player, PacketType::packet_connect, sockfd, NULL))
		return 3;

	return 0;
}

void* packet_recieveing_thread(void* psockfd)
{
	int sockfd = (intptr_t)psockfd;
	Packet incoming_packet {};
 
	int read_size = 0;
	while((read_size = recv(sockfd, &incoming_packet, sizeof(incoming_packet), 0)) > 0)
	{
		switch(incoming_packet.type)
		{
			case PacketType::packet_connect:
				handle_incoming_packet(&incoming_packet.data.packet_connect.player, &incoming_packet, &players);
				send_packet(&player, PacketType::packet_move, &sockfd, NULL);
				break;
			case PacketType::packet_disconnect:
				handle_incoming_packet(&incoming_packet.data.packet_disconnect.player, &incoming_packet, &players);
				break;
			case PacketType::packet_move:
				handle_incoming_packet(&incoming_packet.data.packet_move.player, &incoming_packet, &players);
				break;
			case PacketType::packet_server_info:
				handle_incoming_packet(NULL, &incoming_packet, NULL);
				break;
			default:
				send_packet(&player, PacketType::packet_disconnect, &sockfd, NULL);
				exit(1);
				break;
		}
	}

	return 0;
}

int move(Player* player, char* key, int* sockfd)
{
	//iscolor
	switch(*key)
	{
		case 'q':
			return 1;
			break;
		case 'w':
			player->y = player->y - 1;
			send_packet(player, PacketType::packet_move, sockfd, NULL);
			break;
		case 'a':
			player->x = player->x - 2;
			send_packet(player, PacketType::packet_move, sockfd, NULL);
			break;
		case 's':
			player->y = player->y + 1;
			send_packet(player, PacketType::packet_move, sockfd, NULL);
			break;
		case 'd':
			player->x = player->x + 2;
			send_packet(player, PacketType::packet_move, sockfd, NULL);
			break;
		default:
			break;
	}

	return 0;
}
