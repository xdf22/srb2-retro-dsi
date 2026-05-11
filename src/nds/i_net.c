#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <nds.h>
#include "../doomdef.h"
#include "../i_system.h"
#include "../d_event.h"
#include "../d_net.h"
#include "../m_argv.h"
#include "../doomstat.h"
#include "../i_net.h"
#include "../z_zone.h"
#include "../i_tcp.h"
#include "SDL_net.h"
#include "SDLnetsys.h"


#define MAXBANS 20

/* Since the UNIX/Win32/BeOS code is so different from MacOS,
   we'll just have two completely different sections here.
*/
static int SDLNet_started = 0;

#include <signal.h>

static __inline__ u16 SDL_Swap16(u16 x)
{
	return((x<<8)|(x>>8));
} 
 
static __inline__ u32 SDL_Swap32(u32 x) {
	return((x<<24)|((x<<8)&0x00FF0000)|((x>>8)&0x0000FF00)|(x>>24));
}

/* Initialize/Cleanup the network API */
int  SDLNet_Init(void)
{
	if ( !SDLNet_started ) {
		/* SIGPIPE is generated when a remote socket is closed */
		void (*handler)(int);
		handler = signal(SIGPIPE, SIG_IGN);
		if ( handler != SIG_DFL ) {
			signal(SIGPIPE, handler);
		}
	}
	++SDLNet_started;
	return(0);
}
void SDLNet_Quit(void)
{
	if ( SDLNet_started == 0 ) {
		return;
	}
	if ( --SDLNet_started == 0 ) {
		/* Restore the SIGPIPE handler */
		void (*handler)(int);
		handler = signal(SIGPIPE, SIG_DFL);
		if ( handler != SIG_IGN ) {
			signal(SIGPIPE, handler);
		}
	}
}

/* Resolve a host name and port to an IP address in network form */
int SDLNet_ResolveHost(IPaddress *address, const char *host, u16 port)
{
	int retval = 0;

	// Perform the actual host resolution
	if ( host == NULL ) {
		address->host = INADDR_ANY;
	} else {
		address->host = inet_addr(host);
		
		if ( address->host == INADDR_NONE ) {
			address->host = INADDR_ANY;
		}
	}
	address->port = SDL_SwapBE16(port);
	//address->port = port;

	/* Return the status */
	return(retval);
}

/* Resolve an ip address to a host name in canonical form.
   If the ip couldn't be resolved, this function returns NULL,
   otherwise a pointer to a static buffer containing the hostname
   is returned.  Note that this function is not thread-safe.
*/
/* Written by Miguel Angel Blanch.
 * Main Programmer of Arianne RPG.
 * http://come.to/arianne_rpg
 */
const char *SDLNet_ResolveIP(IPaddress *ip)
{
	struct addrinfo hint;
	struct addrinfo *result, *rp;
	unsigned char bytes[4];
	
    bytes[0] = (ip->host >> 24) & 0xFF;
    bytes[1] = (ip->host >> 16) & 0xFF;
    bytes[2] = (ip->host >> 8) & 0xFF;
    bytes[3] = ip->host & 0xFF;

	hint.ai_flags = AI_CANONNAME;
    hint.ai_family = AF_INET;     // Allow IPv4 and IPv6
    hint.ai_socktype = SOCK_STREAM; // TCP
    hint.ai_protocol = 0;
    hint.ai_addrlen = 0;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;

    int err = getaddrinfo(va("%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]), "80", &hint, &result);
    if (err != 0)
    {
        CONS_Printf("getaddrinfo(): %d\n", err);
        return NULL;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        struct sockaddr_in *sinp;
        const char *addr;
        char buf[1024];

        return rp->ai_canonname;
    }
	
	return NULL;
}

struct _UDPsocket {
	int ready;
	SOCKET channel;
	IPaddress address;

	struct UDP_channel {
		int numbound;
		IPaddress address[SDLNET_MAX_UDPADDRESSES];
	} binding[SDLNET_MAX_UDPCHANNELS];
};

/* Allocate/free a single UDP packet 'size' bytes long.
   The new packet is returned, or NULL if the function ran out of memory.
 */
extern UDPpacket *SDLNet_AllocPacket(int size)
{
	UDPpacket *packet;
	int error;

	error = 1;
	packet = (UDPpacket *)malloc(sizeof(*packet));
	if ( packet != NULL ) {
		packet->maxlen = size;
		packet->data = (u8 *)malloc(size);
		if ( packet->data != NULL ) {
			error = 0;
		}
	}
	if ( error ) {
		SDLNet_FreePacket(packet);
		packet = NULL;
	}
	return(packet);
}

int SDLNet_ResizePacket(UDPpacket *packet, int newsize)
{
	u8 *newdata;

	newdata = (u8 *)malloc(newsize);
	if ( newdata != NULL ) {
		free(packet->data);
		packet->data = newdata;
		packet->maxlen = newsize;
	}
	return(packet->maxlen);
}

extern void SDLNet_FreePacket(UDPpacket *packet)
{
	if ( packet ) {
		if ( packet->data )
			free(packet->data);
		free(packet);
	}
}

/* Allocate/Free a UDP packet vector (array of packets) of 'howmany' packets,
   each 'size' bytes long.
   A pointer to the packet array is returned, or NULL if the function ran out
   of memory.
 */
UDPpacket **SDLNet_AllocPacketV(int howmany, int size)
{
	UDPpacket **packetV;

	packetV = (UDPpacket **)malloc((howmany+1)*sizeof(*packetV));
	if ( packetV != NULL ) {
		int i;
		for ( i=0; i<howmany; ++i ) {
			packetV[i] = SDLNet_AllocPacket(size);
			if ( packetV[i] == NULL ) {
				break;
			}
		}
		packetV[i] = NULL;

		if ( i != howmany ) {
			SDLNet_FreePacketV(packetV);
			packetV = NULL;
		}
	}
	return(packetV);
}

void SDLNet_FreePacketV(UDPpacket **packetV)
{
	if ( packetV ) {
		int i;
		for ( i=0; packetV[i]; ++i ) {
			SDLNet_FreePacket(packetV[i]);
		}
		free(packetV);
	}
}

/* Since the UNIX/Win32/BeOS code is so different from MacOS,
   we'll just have two completely different sections here.
*/

/* Open a UDP network socket
   If 'port' is non-zero, the UDP socket is bound to a fixed local port.
*/
extern UDPsocket SDLNet_UDP_Open(u16 port)
{
	UDPsocket sock;

	/* Allocate a UDP socket structure */
	sock = (UDPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		CONS_Printf("SDL_NET: Out of memory\n");
		goto error_return;
	}
	memset(sock, 0, sizeof(*sock));
	
	/* Open the socket */
	sock->channel = socket(AF_INET, SOCK_DGRAM, 0);

	if ( sock->channel == INVALID_SOCKET ) 
	{
		CONS_Printf("SDL_NET: Couldn't create socket\n");
		goto error_return;
	}

	/* Bind locally, if appropriate */
	if ( port )
	{
		struct sockaddr_in sock_addr;
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = INADDR_ANY;
		sock_addr.sin_port = SDL_SwapBE16(port);
	//	sock_addr.sin_port = port;

		/* Bind the socket for listening */
		if ( bind(sock->channel, (struct sockaddr *)&sock_addr,
				sizeof(sock_addr)) == SOCKET_ERROR ) {
			CONS_Printf("SDL_NET: Couldn't bind to local port\n");
			goto error_return;
		}
		/* Fill in the channel host address */
		sock->address.host = sock_addr.sin_addr.s_addr;
		sock->address.port = sock_addr.sin_port;
	}

	/* The socket is ready */
	
	return(sock);

error_return:
	SDLNet_UDP_Close(sock);
	
	return(NULL);
}

/* Verify that the channel is in the valid range */
static int ValidChannel(int channel)
{
	if ( (channel < 0) || (channel >= SDLNET_MAX_UDPCHANNELS) ) {
		CONS_Printf("SDL_NET: Invalid channel");
		return(0);
	}
	return(1);
}

/* Bind the address 'address' to the requested channel on the UDP socket.
   If the channel is -1, then the first unbound channel will be bound with
   the given address as it's primary address.
   If the channel is already bound, this new address will be added to the
   list of valid source addresses for packets arriving on the channel.
   If the channel is not already bound, then the address becomes the primary
   address, to which all outbound packets on the channel are sent.
   This function returns the channel which was bound, or -1 on error.
*/
int SDLNet_UDP_Bind(UDPsocket sock, int channel, IPaddress *address)
{
	struct UDP_channel *binding;

	if ( channel == -1 ) {
		for ( channel=0; channel < SDLNET_MAX_UDPCHANNELS; ++channel ) {
			binding = &sock->binding[channel];
			if ( binding->numbound < SDLNET_MAX_UDPADDRESSES ) {
				break;
			}
		}
	} else {
		if ( ! ValidChannel(channel) ) {
			return(-1);
		}
		binding = &sock->binding[channel];
	}
	if ( binding->numbound == SDLNET_MAX_UDPADDRESSES ) {
		return(-1);
	}
	binding->address[binding->numbound++] = *address;
	return(channel);
}

/* Unbind all addresses from the given channel */
void SDLNet_UDP_Unbind(UDPsocket sock, int channel)
{
	if ( (channel >= 0) && (channel < SDLNET_MAX_UDPCHANNELS) ) {
		sock->binding[channel].numbound = 0;
	}
}

/* Get the primary IP address of the remote system associated with the
   socket and channel.
   If the channel is not bound, this function returns NULL.
 */
IPaddress *SDLNet_UDP_GetPeerAddress(UDPsocket sock, int channel)
{
	IPaddress *address;

	address = NULL;
	switch (channel) {
		case -1:
			/* Return the actual address of the socket */
			address = &sock->address;
			break;
		default:
			/* Return the address of the bound channel */
			if ( ValidChannel(channel) &&
				(sock->binding[channel].numbound > 0) ) {
				address = &sock->binding[channel].address[0];
			}
			break;
	}
	return(address);
}

/* Send a vector of packets to the the channels specified within the packet.
   If the channel specified in the packet is -1, the packet will be sent to
   the address in the 'src' member of the packet.
   Each packet will be updated with the status of the packet after it has 
   been sent, -1 if the packet send failed.
   This function returns the number of packets sent.
*/
int SDLNet_UDP_SendV(UDPsocket sock, UDPpacket **packets, int npackets)
{
	int numsent, i, j;
	struct UDP_channel *binding;
	int status;

	int sock_len;
	struct sockaddr_in sock_addr;

	/* Set up the variables to send packets */
	sock_len = sizeof(sock_addr);
	
	numsent = 0;
	for ( i=0; i<npackets; ++i ) 
	{
		/* if channel is < 0, then use channel specified in sock */
		
		if ( packets[i]->channel < 0 ) 
		{
			sock_addr.sin_addr.s_addr = packets[i]->address.host;
			sock_addr.sin_port = packets[i]->address.port;
			sock_addr.sin_family = AF_INET;
			status = sendto(sock->channel, 
					packets[i]->data, packets[i]->len, 0,
					(struct sockaddr *)&sock_addr,sock_len);
			if ( status >= 0 )
			{
				packets[i]->status = status;
				++numsent;
			}
		}
		else 
		{
			/* Send to each of the bound addresses on the channel */
#ifdef DEBUG_NET
			//printf("SDLNet_UDP_SendV sending packet to channel = %d\n", packets[i]->channel );
#endif
			
			binding = &sock->binding[packets[i]->channel];
			
			for ( j=binding->numbound-1; j>=0; --j ) 
			{
				sock_addr.sin_addr.s_addr = binding->address[j].host;
				sock_addr.sin_port = binding->address[j].port;
				sock_addr.sin_family = AF_INET;
				status = sendto(sock->channel, 
						packets[i]->data, packets[i]->len, 0,
						(struct sockaddr *)&sock_addr,sock_len);
				if ( status >= 0 )
				{
					packets[i]->status = status;
					++numsent;
				}
			}
		}
	}
	
	return(numsent);
}

int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet)
{
	/* This is silly, but... */
	packet->channel = channel;
	return(SDLNet_UDP_SendV(sock, &packet, 1));
}

/* Returns true if a socket is has data available for reading right now */
static int SocketReady(SOCKET sock)
{
	int retval = 0;
	struct timeval tv;
	fd_set mask;

	/* Check the file descriptors for available data */
	do {
		errno = 0;

		/* Set up the mask of file descriptors */
		FD_ZERO(&mask);
		FD_SET(sock, &mask);

		/* Set up the timeout */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		/* Look! */
		retval = select(sock+1, &mask, NULL, NULL, &tv);
	} while ( errno == EINTR );

	return(retval == 1);
}

/* Receive a vector of pending packets from the UDP socket.
   The returned packets contain the source address and the channel they arrived
   on.  If they did not arrive on a bound channel, the the channel will be set
   to -1.
   This function returns the number of packets read from the network, or -1
   on error.  This function does not block, so can return 0 packets pending.
*/
extern int SDLNet_UDP_RecvV(UDPsocket sock, UDPpacket **packets)
{
	int numrecv, i, j;
	struct UDP_channel *binding;
	socklen_t sock_len;
	struct sockaddr_in sock_addr;

	numrecv = 0;
	while ( packets[numrecv] && SocketReady(sock->channel) ) 
	{
	UDPpacket *packet;

		packet = packets[numrecv];
		
		sock_len = sizeof(sock_addr);
		packet->status = recvfrom(sock->channel,
				packet->data, packet->maxlen, 0,
				(struct sockaddr *)&sock_addr,
						&sock_len);
		if ( packet->status >= 0 ) {
			packet->len = packet->status;
			packet->address.host = sock_addr.sin_addr.s_addr;
			packet->address.port = sock_addr.sin_port;
		}

		if (packet->status >= 0)
		{
			packet->channel = -1;
			
			for (i=(SDLNET_MAX_UDPCHANNELS-1); i>=0; --i ) 
			{
				binding = &sock->binding[i];
				
				for ( j=binding->numbound-1; j>=0; --j ) 
				{
					if ( (packet->address.host == binding->address[j].host) &&
					     (packet->address.port == binding->address[j].port) ) 
					{
						packet->channel = i;
						goto foundit; /* break twice */
					}
				}
			}
foundit:
			++numrecv;
		} 
		
		else 
		{
			packet->len = 0;
		}
	}
	
	sock->ready = 0;
	
	return(numrecv);
}

/* Receive a single packet from the UDP socket.
   The returned packet contains the source address and the channel it arrived
   on.  If it did not arrive on a bound channel, the the channel will be set
   to -1.
   This function returns the number of packets read from the network, or -1
   on error.  This function does not block, so can return 0 packets pending.
*/
int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet)
{
	UDPpacket *packets[2];

	/* Receive a packet array of 1 */
	packets[0] = packet;
	packets[1] = NULL;
	return(SDLNet_UDP_RecvV(sock, packets));
}

/* Close a UDP network socket */
extern void SDLNet_UDP_Close(UDPsocket sock)
{
	if ( sock != NULL ) 
	{
		if ( sock->channel != INVALID_SOCKET ) 
		{
			closesocket(sock->channel);
		}
		
		free(sock);
	}
}

struct _TCPsocket {
	int ready;
	SOCKET channel;
	IPaddress remoteAddress;
	IPaddress localAddress;
	int sflag;
};

/* Open a TCP network socket
   If 'remote' is NULL, this creates a local server socket on the given port,
   otherwise a TCP connection to the remote host and port is attempted.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Open(IPaddress *ip)
{
	TCPsocket sock;
	struct sockaddr_in sock_addr;

	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		CONS_Printf("SDL_NET: Out of memory");
		goto error_return;
	}

	/* Open the socket */
	sock->channel = socket(AF_INET, SOCK_STREAM, 0);
	if ( sock->channel == INVALID_SOCKET ) {
		CONS_Printf("SDL_NET: Couldn't create socket");
		goto error_return;
	}

	/* Connect to remote, or bind locally, as appropriate */
	if ( (ip->host != INADDR_NONE) && (ip->host != INADDR_ANY) ) {

	// #########  Connecting to remote
	
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = ip->host;
		sock_addr.sin_port = ip->port;

		/* Connect to the remote host */
		if ( connect(sock->channel, (struct sockaddr *)&sock_addr,
				sizeof(sock_addr)) == SOCKET_ERROR ) {
			CONS_Printf("SDL_NET: Couldn't connect to remote host");
			goto error_return;
		}
		sock->sflag = 0;
	} else {

	// ##########  Binding locally

		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = INADDR_ANY;
		sock_addr.sin_port = ip->port;

/*
 * Windows gets bad mojo with SO_REUSEADDR:
 * http://www.devolution.com/pipermail/sdl/2005-September/070491.html
 *   --ryan.
 */
		/* allow local address reuse */
/*		{ int yes = 1;
			setsockopt(sock->channel, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
		}*/

		/* Bind the socket for listening */
		if ( bind(sock->channel, (struct sockaddr *)&sock_addr,
				sizeof(sock_addr)) == SOCKET_ERROR ) {
			CONS_Printf("SDL_NET: Couldn't bind to local port");
			goto error_return;
		}
		if ( listen(sock->channel, 5) == SOCKET_ERROR ) {
			CONS_Printf("SDL_NET: Couldn't listen to local port");
			goto error_return;
		}

		/* Set the socket to non-blocking mode for accept() */
//		fcntl(sock->channel, F_SETFL, O_NONBLOCK);

		sock->sflag = 1;
	}
	sock->ready = 0;

	/* Set the nodelay TCP option for real-time games */
/*	{ int yes = 1;
	setsockopt(sock->channel, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
	}*/

	/* Fill in the channel host address */
	sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
	sock->remoteAddress.port = sock_addr.sin_port;

	/* The socket is ready */
	return(sock);

error_return:
	SDLNet_TCP_Close(sock);
	return(NULL);
}

/* Accept an incoming connection on the given server socket.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Accept(TCPsocket server)
{
	TCPsocket sock;
	struct sockaddr_in sock_addr;
	socklen_t sock_alen;

	/* Only server sockets can accept */
	if ( ! server->sflag ) {
		CONS_Printf("SDL_NET: Only server sockets can accept()");
		return(NULL);
	}
	server->ready = 0;

	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		CONS_Printf("SDL_NET: Out of memory");
		goto error_return;
	}

	/* Accept a new TCP connection on a server socket */
	sock_alen = sizeof(sock_addr);
	sock->channel = accept(server->channel, (struct sockaddr *)&sock_addr,
								&sock_alen);
	if ( sock->channel == SOCKET_ERROR ) {
		CONS_Printf("SDL_NET: accept() failed");
		goto error_return;
	}

	sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
	sock->remoteAddress.port = sock_addr.sin_port;

	sock->sflag = 0;
	sock->ready = 0;

	/* The socket is ready */
	return(sock);

error_return:
	SDLNet_TCP_Close(sock);
	return(NULL);
}

/* Get the IP address of the remote system associated with the socket.
   If the socket is a server socket, this function returns NULL.
*/
IPaddress *SDLNet_TCP_GetPeerAddress(TCPsocket sock)
{
	if ( sock->sflag ) {
		return(NULL);
	}
	return(&sock->remoteAddress);
}

/* Send 'len' bytes of 'data' over the non-server socket 'sock'
   This function returns the actual amount of data sent.  If the return value
   is less than the amount of data sent, then either the remote connection was
   closed, or an unknown socket error occurred.
*/
int SDLNet_TCP_Send(TCPsocket sock, const void *datap, int len)
{
	const u8 *data = (const u8 *)datap;	/* For pointer arithmetic */
	int sent, left;

	/* Server sockets are for accepting connections only */
	if ( sock->sflag ) {
		CONS_Printf("SDL_NET: Server sockets cannot send");
		return(-1);
	}

	/* Keep sending data until it's sent or an error occurs */
	left = len;
	sent = 0;
	errno = 0;
	do {
		len = send(sock->channel, (const char *) data, left, 0);
		if ( len > 0 ) {
			sent += len;
			left -= len;
			data += len;
		}
	} while ( (left > 0) && ((len > 0) || (errno == EINTR)) );

	return(sent);
}

/* Receive up to 'maxlen' bytes of data over the non-server socket 'sock',
   and store them in the buffer pointed to by 'data'.
   This function returns the actual amount of data received.  If the return
   value is less than or equal to zero, then either the remote connection was
   closed, or an unknown socket error occurred.
*/
int SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen)
{
	int len;

	/* Server sockets are for accepting connections only */
	if ( sock->sflag ) {
		CONS_Printf("SDL_NET: Server sockets cannot receive");
		return(-1);
	}

	errno = 0;
	do {
		len = recv(sock->channel, (char *) data, maxlen, 0);
	} while ( errno == EINTR );

	sock->ready = 0;
	return(len);
}

/* Close a TCP network socket */
void SDLNet_TCP_Close(TCPsocket sock)
{
	if ( sock != NULL ) {
		if ( sock->channel != INVALID_SOCKET ) {
			closesocket(sock->channel);
		}
		free(sock);
	}
}


/* The select() API for network sockets */

struct SDLNet_Socket {
	int ready;
	SOCKET channel;
};

struct _SDLNet_SocketSet {
	int numsockets;
	int maxsockets;
	struct SDLNet_Socket **sockets;
};

/* Allocate a socket set for use with SDLNet_CheckSockets() 
   This returns a socket set for up to 'maxsockets' sockets, or NULL if 
   the function ran out of memory. 
 */
SDLNet_SocketSet SDLNet_AllocSocketSet(int maxsockets)
{
	struct _SDLNet_SocketSet *set;
	int i;

	set = (struct _SDLNet_SocketSet *)malloc(sizeof(*set));
	if ( set != NULL ) {
		set->numsockets = 0;
		set->maxsockets = maxsockets;
		set->sockets = (struct SDLNet_Socket **)malloc
					(maxsockets*sizeof(*set->sockets));
		if ( set->sockets != NULL ) {
			for ( i=0; i<maxsockets; ++i ) {
				set->sockets[i] = NULL;
			}
		} else {
			free(set);
			set = NULL;
		}
	}
	return(set);
}

/* Add a socket to a set of sockets to be checked for available data */
int SDLNet_AddSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock)
{
	if ( sock != NULL ) {
		if ( set->numsockets == set->maxsockets ) {
			CONS_Printf("SDL_NET: socketset is full");
			return(-1);
		}
		set->sockets[set->numsockets++] = (struct SDLNet_Socket *)sock;
	}
	return(set->numsockets);
}

/* Remove a socket from a set of sockets to be checked for available data */
int SDLNet_DelSocket(SDLNet_SocketSet set, SDLNet_GenericSocket sock)
{
	int i;

	if ( sock != NULL ) {
		for ( i=0; i<set->numsockets; ++i ) {
			if ( set->sockets[i] == (struct SDLNet_Socket *)sock ) {
				break;
			}
		}
		if ( i == set->numsockets ) {
			CONS_Printf("SDL_NET: socket not found in socketset");
			return(-1);
		}
		--set->numsockets;
		for ( ; i<set->numsockets; ++i ) {
			set->sockets[i] = set->sockets[i+1];
		}
	}
	return(set->numsockets);
}

/* This function checks to see if data is available for reading on the
   given set of sockets.  If 'timeout' is 0, it performs a quick poll,
   otherwise the function returns when either data is available for
   reading, or the timeout in milliseconds has elapsed, which ever occurs
   first.  This function returns the number of sockets ready for reading,
   or -1 if there was an error with the select() system call.
*/
int SDLNet_CheckSockets(SDLNet_SocketSet set, u32 timeout)
{
	int i;
	SOCKET maxfd;
	int retval;
	struct timeval tv;
	fd_set mask;

	/* Find the largest file descriptor */
	maxfd = 0;
	for ( i=set->numsockets-1; i>=0; --i ) {
		if ( set->sockets[i]->channel > maxfd ) {
			maxfd = set->sockets[i]->channel;
		}
	}

	/* Check the file descriptors for available data */
	do {
		errno = 0;

		/* Set up the mask of file descriptors */
		FD_ZERO(&mask);
		for ( i=set->numsockets-1; i>=0; --i ) {
			FD_SET(set->sockets[i]->channel, &mask);
		}

		/* Set up the timeout */
		tv.tv_sec = timeout/1000;
		tv.tv_usec = (timeout%1000)*1000;

		/* Look! */
		retval = select(maxfd+1, &mask, NULL, NULL, &tv);
	} while ( errno == EINTR );

	/* Mark all file descriptors ready that have data available */
	if ( retval > 0 ) {
		for ( i=set->numsockets-1; i>=0; --i ) {
			if ( FD_ISSET(set->sockets[i]->channel, &mask) ) {
				set->sockets[i]->ready = 1;
			}
		}
	}
	return(retval);
}
   
/* Free a set of sockets allocated by SDL_NetAllocSocketSet() */
extern void SDLNet_FreeSocketSet(SDLNet_SocketSet set)
{
	if ( set ) {
		free(set->sockets);
		free(set);
	}
}

static IPaddress clientaddress[MAXNETNODES+1];
static IPaddress banned[MAXBANS];

static UDPpacket mypacket;
static UDPsocket mysocket = NULL;
static SDLNet_SocketSet myset = NULL;

static size_t numbans = 0;
static boolean NET_bannednode[MAXNETNODES+1]; /// \note do we really need the +1?
static boolean init_SDLNet_driver = false;

boolean nodeconnected[MAXNETNODES+1];

static const char *NET_AddrToStr(IPaddress* sk)
{
	return va("%s:%d", SDLNet_ResolveIP(sk), sk->port);
}

static const char *NET_GetNodeAddress(INT32 node)
{
	return NET_AddrToStr(&clientaddress[node]);
}

static const char *NET_GetBanAddress(size_t ban)
{
	if (ban > numbans)
		return NULL;
	return NET_AddrToStr(&banned[ban]);
}

static boolean NET_cmpaddr(IPaddress* a, IPaddress* b)
{
	return (a->host == b->host && (b->port == 0 || a->port == b->port));
}

static boolean NET_CanGet(void)
{
	return myset?(SDLNet_CheckSockets(myset,0)  == 1):false;
}

static void NET_Get(void)
{
	INT32 mystatus;
	INT32 newnode;
	mypacket.len = MAXPACKETLENGTH;
	if (!NET_CanGet())
	{
		doomcom->remotenode = -1; // no packet
		return;
	}
	mystatus = SDLNet_UDP_Recv(mysocket,&mypacket);
	if (mystatus != -1)
	{
		if (mypacket.channel != -1)
		{
			doomcom->remotenode = mypacket.channel+1; // good packet from a game player
			doomcom->datalength = mypacket.len;
			return;
		}
		newnode = SDLNet_UDP_Bind(mysocket,-1,&mypacket.address);
		if (newnode != -1)
		{
			size_t i;
			newnode++;
			M_Memcpy(&clientaddress[newnode], &mypacket.address, sizeof (IPaddress));
			DEBFILE(va("New node detected: node:%d address:%s\n", newnode,
					NET_GetNodeAddress(newnode)));
			doomcom->remotenode = newnode; // good packet from a game player
			doomcom->datalength = mypacket.len;
			for (i = 0; i < numbans; i++)
			{
				if (NET_cmpaddr(&mypacket.address, &banned[i]))
				{
					DEBFILE("This dude has been banned\n");
					NET_bannednode[newnode] = true;
					break;
				}
			}
			if (i == numbans)
				NET_bannednode[newnode] = false;
			return;
		}
		else
			I_OutputMsg("Net Error\n");
	}
	else if (mystatus == -1)
	{
		I_OutputMsg("Net Error\n");
	}

	DEBFILE("New node detected: No more free slots\n");
	doomcom->remotenode = -1; // no packet
}

#if 0
static boolean NET_CanSend(void)
{
	return true;
}
#endif

static void NET_Send(void)
{
	if (!doomcom->remotenode)
		return;
	mypacket.len = doomcom->datalength;
	if (SDLNet_UDP_Send(mysocket,doomcom->remotenode-1,&mypacket) == 0)
	{
		I_OutputMsg("Net Error\n");
	}
}

static void NET_FreeNodenum(INT32 numnode)
{
	// can't disconnect from self :)
	if (!numnode)
		return;

	DEBFILE(va("Free node %d (%s)\n", numnode, NET_GetNodeAddress(numnode)));

	SDLNet_UDP_Unbind(mysocket,numnode-1);

	memset(&clientaddress[numnode], 0, sizeof (IPaddress));
}

static UDPsocket NET_Socket(void)
{
	UDPsocket temp = NULL;
	uint16_t portnum = 0;
	IPaddress tempip = {INADDR_BROADCAST,0};
	//Hurdler: I'd like to put a server and a client on the same computer
	//Logan: Me too
	//BP: in fact for client we can use any free port we want i have read
	//    in some doc that connect in udp can do it for us...
	//Alam: where?
	if (M_CheckParm("-clientport"))
	{
		if (!M_IsNextParm())
			I_Error("syntax: -clientport <portnum>");
		portnum = atoi(M_GetNextParm());
	}
	else
		portnum = sock_port;
	temp = SDLNet_UDP_Open(portnum);
	if (!temp)
	{
			I_OutputMsg("Net Error\n");
		return NULL;
	}
	if (SDLNet_UDP_Bind(temp,BROADCASTADDR-1,&tempip) == -1)
	{
		I_OutputMsg("Net Error\n");
		SDLNet_UDP_Close(temp);
		return NULL;
	}
	clientaddress[BROADCASTADDR].port = sock_port;
	clientaddress[BROADCASTADDR].host = INADDR_BROADCAST;

	doomcom->extratics = 1; // internet is very high ping

	return temp;
}

static void I_ShutdownSDLNetDriver(void)
{
	if (myset) SDLNet_FreeSocketSet(myset);
	myset = NULL;
	SDLNet_Quit();
	init_SDLNet_driver = false;
}

static void I_InitSDLNetDriver(void)
{
	if (init_SDLNet_driver)
		I_ShutdownSDLNetDriver();
	if (SDLNet_Init() == -1)
	{
		I_OutputMsg("Net Error");
		return; // No good!
	}
	D_SetDoomcom();
	mypacket.data = doomcom->data;
	init_SDLNet_driver = true;
}

static void NET_CloseSocket(void)
{
	if (mysocket)
		SDLNet_UDP_Close(mysocket);
	mysocket = NULL;
}

static SINT8 NET_NetMakeNodewPort(const char *hostname)
{
	INT32 newnode;
	UINT16 portnum = sock_port;
	char *localhostname = strdup(hostname);
	char *portchar;
	IPaddress hostnameIP;
	
	// retrieve portnum from address!
	strtok(localhostname, ":");
	portchar = strtok(NULL, ":");
	if (portchar)
		portnum = htons(atoi(portchar));
	
	if (SDLNet_ResolveHost(&clientaddress[newnode],localhostname,portnum) == -1)
	{
		I_OutputMsg("Net Error\n");
		return -1;
	}
	
	free(localhostname);
	newnode = SDLNet_UDP_Bind(mysocket,-1,&clientaddress[newnode]);
	if (newnode == -1)
	{
		I_OutputMsg("Net Error\n");
		return newnode;
	}
	newnode++;
	return (SINT8)newnode;
}

static boolean NET_OpenSocket(void)
{
	memset(clientaddress, 0, sizeof (clientaddress));

	//I_OutputMsg("SDL_Net Code starting up\n");

	//I_NetCanSend = NET_CanSend;

	// build the socket but close it first
	NET_CloseSocket();
	mysocket = NET_Socket();

	if (!mysocket)
		return false;

	// for select
	myset = SDLNet_AllocSocketSet(1);
	if (!myset)
	{
		I_OutputMsg("Net Error\n");
		return false;
	}
	if (SDLNet_UDP_AddSocket(myset,mysocket) == -1)
	{
		I_OutputMsg("Net Error\n");
		return false;
	}
	return true;
}

static boolean NET_Ban(INT32 node)
{
	if (numbans == MAXBANS)
		return false;

	M_Memcpy(&banned[numbans], &clientaddress[node], sizeof (IPaddress));
	banned[numbans].port = 0;
	numbans++;
	return true;
}

static boolean NET_SetBanAddress(const char *address, const char *mask)
{
	(void)mask;
	if (numbans == MAXBANS)
		return false;

	if (SDLNet_ResolveHost(&banned[numbans], address, 0) == -1)
		return false;
	numbans++;
	return true;
}

static void NET_ClearBans(void)
{
	numbans = 0;
}
//
// I_InitNetwork
// Only required for DOS, so this is more a dummy
//
boolean I_InitNetwork(void)
{
	char serverhostname[255];
	boolean ret = false;

	//if (!M_CheckParm ("-sdlnet"))
	//	return false;
	// initilize the driver
	I_InitSDLNetDriver();
	I_AddExitFunc(I_ShutdownSDLNetDriver);
	if (!init_SDLNet_driver)
		return false;

	if (M_CheckParm("-udpport"))
	{
		if (M_IsNextParm())
			sock_port = (UINT16)atoi(M_GetNextParm());
		else
			sock_port = 0;
	}

	// parse network game options,
	if (M_CheckParm("-server") || dedicated)
	{
		server = true;

		// If a number of clients (i.e. nodes) is specified, the server will wait for the clients
		// to connect before starting.
		// If no number is specified here, the server starts with 1 client, and others can join
		// in-game.
		// Since Boris has implemented join in-game, there is no actual need for specifying a
		// particular number here.
		// FIXME: for dedicated server, numnodes needs to be set to 0 upon start
/*		if (M_IsNextParm())
			doomcom->numnodes = (INT16)atoi(M_GetNextParm());
		else */if (dedicated)
			doomcom->numnodes = 0;
		else
			doomcom->numnodes = 1;

		if (doomcom->numnodes < 0)
			doomcom->numnodes = 0;
		if (doomcom->numnodes > MAXNETNODES)
			doomcom->numnodes = MAXNETNODES;

		// server
		servernode = 0;
		// FIXME:
		// ??? and now ?
		// server on a big modem ??? 4*isdn
		net_bandwidth = 16000;
		hardware_MAXPACKETLENGTH = INETPACKETLENGTH;

		ret = true;
	}
	else if (M_CheckParm("-connect"))
	{
		if (M_IsNextParm())
			strcpy(serverhostname, M_GetNextParm());
		else
			serverhostname[0] = 0; // assuming server in the LAN, use broadcast to detect it

		// server address only in ip
		if (serverhostname[0])
		{
			COM_BufAddText("connect \"");
			COM_BufAddText(serverhostname);
			COM_BufAddText("\"\n");

			// probably modem
			hardware_MAXPACKETLENGTH = INETPACKETLENGTH;
		}
		else
		{
			// so we're on a LAN
			COM_BufAddText("connect any\n");

			net_bandwidth = 800000;
			hardware_MAXPACKETLENGTH = MAXPACKETLENGTH;
		}
	}

	mypacket.maxlen = hardware_MAXPACKETLENGTH;
	I_NetOpenSocket = NET_OpenSocket;
	I_NetMakeNode = NET_NetMakeNodewPort;
	I_Ban = NET_Ban;
	I_ClearBans = NET_ClearBans;
	I_GetNodeAddress = NET_GetNodeAddress;
	I_GetBanAddress = NET_GetBanAddress;
	I_SetBanAddress = NET_SetBanAddress;
	I_NetSend = NET_Send;
	I_NetGet = NET_Get;
	I_NetCloseSocket = NET_CloseSocket;
	I_NetFreeNodenum = NET_FreeNodenum;
	//I_NetCanSend = NET_CanSend;
	bannednode = NET_bannednode;

	return ret;
}
