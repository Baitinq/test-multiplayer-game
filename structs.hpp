enum class PacketType
{
	packet_connect,
	packet_disconnect,
	packet_move,
	packet_ping,
	packet_server_info
};

struct Player 
{
	unsigned int sender_id;
	int fd;
	char name[20];
	char ip[20];
	int x, y;
	int color;
	bool is_color;
};

struct PacketConnect
{
	Player player;
};

struct PacketDisconnect
{
	Player player;
};

struct PacketMove
{
	Player player;
	int x, y;
       	bool is_color;	
};

struct PacketPing
{
};

struct PacketServerInfo
{
	int max_players;
	int fps;
};

struct Packet
{
	PacketType type;
	unsigned int sender_id;
	union
	{
		PacketConnect packet_connect;
		PacketDisconnect packet_disconnect;
		PacketMove packet_move;
		PacketPing packet_ping;
		PacketServerInfo packet_server_info;
	} data;
};
