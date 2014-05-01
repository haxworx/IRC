/*

Start of an interactive IRC Client and Bot
Configuration stored in "./config.txt" format is as follows:

HOST=irc.freenode.net
NICK=netczar
NAME=Robo Cop
USER=Clone
PORT=1234
CHAN=haxworx

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFSIZE 32768

#define TCP_TIMEOUT 5		// seconds for TCP timeout

void Bork(char *fmt, ...)
{
	va_list ap;
	char buf[1024] = { 0 };

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	fprintf(stderr, "Error: %s\n", buf);

	exit(EXIT_FAILURE);
}

int Connect(char *hostname, int port)
{
	int sock;
	int status;
	int flags;
	struct hostent *host;
	struct sockaddr_in host_addr;
	fd_set fds;
	struct timeval tm;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		Bork("socket %s", strerror(errno));

	host = gethostbyname(hostname);
	if (host == NULL) {
		printf("! FATAL - invalid hostname \"%s\" %s\n", hostname,
		       strerror(errno));
		exit(EXIT_FAILURE);
	}

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons(port);
	host_addr.sin_addr = *((struct in_addr *) host->h_addr);
	memset(&host_addr.sin_zero, 0, 8);

	status = connect(sock, (struct sockaddr *) &host_addr,
			 sizeof(struct sockaddr));

	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	tm.tv_sec = TCP_TIMEOUT;
	tm.tv_usec = 0;

	if (status == -1) {
		if (errno == EINPROGRESS) {
			flags = fcntl(sock, F_GETFL, 0);
			fcntl(sock, F_SETFL, flags | O_NONBLOCK);
			status = select(sock + 1, &fds, &fds, NULL, &tm);
			if (status == 0) {
				// Timed Data so return
				close(sock);
				return 0;
			} else if (status < 0)
				// Issue with select()
				close(sock);
			Bork("select() %s", strerror(errno));
		} else {
			// Some other error
			close(sock);
			return 0;
		}
	}
	// NON BLOCKING FOR select: fcntl(sock, F_SETFL, flags);

	return sock;
}

typedef struct client_t client_t;
struct client_t {
	char nick[32];
	char chan[32];
	unsigned int mode;
	client_t *next;
};

typedef struct config_t config_t;
struct config_t {
	char nick[32];
	char user[32];
	char name[32];
	char host[32];
	char chan[32];
	unsigned int port;
	client_t *clients;
};

client_t * ChannelWinners(client_t *clients, char *chan, char *nick, unsigned int mode)
{
	client_t *c = clients;
	// this seems to work
	while (c->next) 
		c = c->next;

	if (c->next == NULL) {
		c->next = calloc(1, sizeof(client_t));
		c = c->next;
		sprintf(c->nick, "%s", nick);
		sprintf(c->chan, "%s", chan);
		c->mode = mode;
	}

	return c;
}
void Chomp(char *str)
{
	while (*str) {
		if (*str == '\n' || *str == '\r') {
			*str = '\0';
			return;
		}
		++str;
	}
}

char *GetOption(char *text, char *value)
{
	char *p = NULL;

	if ((p = strstr(text, value))) {

		p += strlen(value) + 1;	// beyond = sign                        
		char *end = strchr(p, '\n');
		*end = '\0';
		char *entry = strdup(p);
		*end = '\n';
		return entry;
	}
	return NULL;
}

#define CONFIG_MAX_LINES 20

config_t *Setup(config_t * Config)
{
	char buf[BUFSIZE] = { 0 };
	char map[BUFSIZE * CONFIG_MAX_LINES] = { 0 };
	char *p = NULL;
	FILE *f = fopen("config.txt", "r");
	if (f == NULL)
		Bork("Could not open config.txt");

	memset(Config, 0, sizeof(config_t));

	while ((p = fgets(buf, sizeof(buf) - 1, f)) != NULL) {
		strcat(map, buf);
	}

	fclose(f);

	char *port = GetOption(map, "PORT");
	if (port)
		Config->port = atoi(port);

	char *nick = GetOption(map, "NICK");
	if (nick)
		strncpy(Config->nick, nick, sizeof(Config->nick));

	char *user = GetOption(map, "USER");
	if (user)
		strncpy(Config->user, user, sizeof(Config->user));

	char *name = GetOption(map, "NAME");
	if (name)
		strncpy(Config->name, name, sizeof(Config->name));

	char *host = GetOption(map, "HOST");
	if (host)
		strncpy(Config->host, host, sizeof(Config->host));

	char *chan = GetOption(map, "CHAN");
	if (chan)
		strncpy(Config->chan, chan, sizeof(Config->chan));

	printf("Connecting to %s on port %d as %s\n", Config->host,
	       Config->port, Config->nick);
	// init data structure
	
	Config->clients = calloc(1, sizeof(client_t));

	return Config;
}

config_t *ChannelUsers(config_t *Config, char *list)
{
	char *ptr = strtok(list, " ");
	ChannelWinners(Config->clients, Config->chan, ptr, 0);
	while (ptr) {
		ptr = strtok(NULL, " ");
		if (ptr)
			ChannelWinners(Config->clients, Config->chan, ptr, 0);
	}
	
	return Config;
}


void ChannelLosers(config_t *Config, char *nick)
{
	client_t *ptr = Config->clients;
	client_t *prev = NULL;

	while (ptr && 0 != strcmp(ptr->nick, nick)) {
		prev = ptr;
		ptr = ptr->next;	
	}

	if (0 == strcmp(ptr->nick, nick)) {
		prev->next = ptr->next;
		free(ptr);
	}
}
void ShowChannelUsers(config_t *Config)
{
	client_t *p = Config->clients;
	int count = 0;

	printf("[#%s] Join\n", Config->chan);

	while (p) {
		if (strlen(p->nick)) {
			++count;
			printf("%s ", p->nick);
		}
		p = p->next;
	}
	printf("\nTotal of %d users\n", count);
}

void WriteServerResponse(int sock, char *Data)
{
	//printf("write: %s\n", Data);
	write(sock, Data, strlen(Data));
	memset(Data, 0, 256);
}

#define SEND_NICK "NICK %s\r\n"
#define SEND_USER "USER %s 8 * : %s\r\n"
#define SEND_JOIN "JOIN #%s\r\n\r\n"

void Authenticate(int sock, config_t * Config)
{
	char Data[256] = { 0 };

	sprintf(Data, SEND_NICK, Config->nick);
	WriteServerResponse(sock, Data);

	sprintf(Data, SEND_USER, Config->user, Config->name);
	WriteServerResponse(sock, Data);
}

#define RECV_MODE "MODE #%s\r\n\r\n"

void Join(int sock, config_t * Config)
{
	char Data[1024] = { 0 };
	ssize_t len = 0;
	char mode[1024] = { 0 };
	
	sprintf(Data, SEND_JOIN, Config->chan);
	WriteServerResponse(sock, Data);
	
	sprintf(mode, "353 %s = #%s :", Config->nick, Config->chan);
	int count = 0;
	while((len = read(sock, Data, sizeof(Data)) > 0) && count == 0) {
		 char *ptr = strstr(Data, mode);
		 if (ptr) {
			ptr += strlen(mode);
			char *end = strstr(ptr, "\r");
			*end = '\0';
			Config = ChannelUsers(Config, ptr);
			count = 1;
			break;
		}
	}
}

#define MODE_CHANGE ":%s MODE %s :"

#define PRIVATE_MESSAGE_CHANNEL "PRIVMSG #%s :%s\r\n\r\n"
#define PRIVATE_MESSAGE_USER "PRIVMSG %s :%s\r\n\r\n"

#define COMMAND_MSG "/msg"
#define COMMAND_JOIN "/join"
#define COMMAND_NAMES "/names"
#define COMMAND_QUIT "/quit"

void ProcessInput(int sock, config_t * Config)
{
	char buf[BUFSIZE] = { 0 };
	char cmd[BUFSIZE] = { 0 };
	fgets(buf, sizeof(buf), stdin);
	Chomp(buf);
	if (buf[0] != '/' && strlen(buf)) {
		sprintf(cmd, PRIVATE_MESSAGE_CHANNEL, Config->chan, buf);
		write(sock, cmd, strlen(cmd));
		printf("[#%s] %s\n", Config->chan, buf);
	} else {
		if (!strncmp(buf, COMMAND_MSG, strlen(COMMAND_MSG))) {
			char *mesg = NULL;
			char *dest = buf;
			dest += strlen(COMMAND_MSG);
			strtok(buf, " ");
			dest = strtok(NULL, " ");
			mesg = strtok(NULL, " ");

			sprintf(cmd, PRIVATE_MESSAGE_USER, dest, mesg);
			write(sock, cmd, strlen(cmd));
			if (strcmp(dest, Config->nick)) // Don't echo our own mesg 
				printf("[#%s/%s] %s\n", Config->chan, dest, mesg);

		} else if (!strncmp
			   (buf, COMMAND_JOIN, strlen(COMMAND_JOIN))) {
			char *chan = buf;

			chan += strlen(COMMAND_JOIN) + 1;	// space
			sprintf(cmd, SEND_JOIN, chan);
			write(sock, cmd, strlen(cmd));
			printf("[#%s] Join (%s) %s\n", chan, Config->nick);
			strcpy(Config->chan, chan);
		} else if (!strcmp(buf, COMMAND_QUIT)) {
			close(sock);
			exit(EXIT_SUCCESS);
		} else if (0 == strcmp(buf, COMMAND_NAMES))
			ShowChannelUsers(Config);
	}

}

void ProcessData(int sock, config_t * Config, char *Data)
{
	int type = 0;
	char *nick, *user, *serv, *command, *chan, *body = { NULL };

	Chomp(Data);

	nick = strtok(Data + 1, "!");
	user = strtok(NULL, "@");
	serv = strtok(NULL, " ");
	command = strtok(NULL, " ");
	chan = strtok(NULL, ":");
	body = strtok(NULL, "");
	if (command) {
		if (!strcmp(command, "PRIVMSG")) {
			printf("[%s/%s] Message: %s\n", chan, nick, body);
		} else if (!strcmp(command, "MODE")) {
			printf("mode/%s %s\n", chan, nick);
		} else if (!strcmp(command, "PART")) {
			ChannelLosers(Config, nick);	
			printf("[#%s] Part (%s)\n", chan, nick);
		} else if (!strcmp(command, "JOIN")) {
			printf("[#%s] Join (%s)\n", chan, nick);
			ChannelWinners(Config->clients, chan, nick, 0);
		} else if (!strcmp(command, "QUIT")) {
			printf("[#%s] Quit (%s)\n", chan, nick);
			ChannelLosers(Config, nick);
		}
	}
	fflush(stdout);
}

void IRC(int sock, config_t * Config)
{
	ssize_t len = 0;
	int i = 0;
	struct timeval tv;
	char Data[BUFSIZE];
	char mode[BUFSIZE];
	int input = fileno(stdin);
	int flags = fcntl(input, F_GETFL, 0);
	fcntl(input, F_SETFL, flags | O_NONBLOCK);

	fd_set fds;	
	
	Authenticate(sock, Config);
	// Register on IRC server

	// read in server information MOTD etc!
	int red = 1;
	sprintf(mode, ":%s MODE %s :+", Config->nick, Config->nick);
	while ((len = read(sock, Data, sizeof(Data))) > 0){
		puts(Data);
		char *ptr = strstr(Data, mode);
		if (ptr){
			break;
		}
		memset(Data, 0, strlen(Data));
	}

	Join(sock, Config); 
	// Join channel
	ShowChannelUsers(Config);	

	for (;;) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&fds);
		FD_SET(input, &fds);
		FD_SET(sock, &fds);
		int res = select(sock + 1, &fds, NULL, NULL, &tv);
		for (i = 0; i <= sock && res > 0; i++) {
			if (FD_ISSET(i, &fds)) {
				if (i == sock) {
					len = read(sock, Data,
						   sizeof(Data));
					if (len <= 0)
						return;
					if (0 == strncmp(Data, "PING", 4)) {
						puts(Data);
						Data[1] = 'O';
						write(sock, Data, len);
					}
					ProcessData(sock, Config, Data);
				} 
			}
		}

		ProcessInput(sock, Config);
		memset(Data, 0, sizeof(Data));
	}
}

int main(void)
{
	int sock;
	config_t Configuration;

	Setup(&Configuration);

	sock = Connect(Configuration.host, Configuration.port);
	if (sock)
		IRC(sock, &Configuration);

	close(sock);

	return EXIT_SUCCESS;
}
