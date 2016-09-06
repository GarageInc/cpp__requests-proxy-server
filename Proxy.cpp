//#define _AFXDLL
// Proxy.cpp is very simple program to implement the HTTP proxy function.
// To make it short and clear , some error tolerant codes are omitted.
// Written by HU Zhongshan   
// e-mail huzhongshan@hotmail.com OR yangjy@mail.njust.edu.cn
// 1999-4-18

#include "stdafx.h"
#include "Proxy.h"
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// The one and only application object

#define HTTP  "http://"

#define PROXYPORT    54322    //Proxy Listen Port
#define BUFSIZE   10240      //chBuffer size

#define SQL_ADDRESS "127.0.0.1" 
#define SQL_PORT 5432

CWinApp theApp;

using namespace std;

UINT ProxyToServer(LPVOID pParam);
UINT UserToProxyThread(void *pParam);
UINT StartAccept(void *pParam);

struct SocketPair {
	SOCKET  user_proxy;      //socket : local machine to proxy server
	SOCKET  proxy_server;    //socket : proxy sever to remote server
	BOOL    is_user_proxy_closed; // status of local machine to proxy server
	BOOL    is_proxy_server_closed; // status of proxy server to remote server
};


struct ProxyParam {
    char *chAddress;    // address of remote server
	HANDLE hUserSvrOK;    // status of setup connection between proxy server and remote server
	SocketPair *pSocketsPair;    // pointer to socket pair
	int     iPort;         // port which will be used to connect to remote server
};                   //This struct is used to exchange information between threads.

SOCKET    gListen_Socket;

int StartServer()
{
	WSADATA wsaData;
	sockaddr_in local;
	SOCKET listen_socket;

	if (::WSAStartup(0x202, &wsaData) != 0) {
		printf("\nError in Startup session.\n");
		WSACleanup();
		return -1;
	};

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(PROXYPORT);

	listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	if (listen_socket == INVALID_SOCKET) {
		printf("\nError in New a Socket.");
		WSACleanup();
		return -2;
	}

	if (::bind(listen_socket, (sockaddr *)&local, sizeof(local)) != 0) {
		printf("\n Error in Binding socket.");
		WSACleanup();
		return -3;
	};

	if (::listen(listen_socket, 5) != 0) {
		printf("\n Error in Listen.");
		WSACleanup();
		return -4;
	}

#ifdef _DEBUG
	printf("\nSuccess with binding to PORT %d\n", PROXYPORT);
#endif

	gListen_Socket = listen_socket;

	AfxBeginThread(StartAccept, NULL);   //Start accept function

	return 1;
}

int CloseServer()
{
	closesocket(gListen_Socket);
	WSACleanup();
	return 1;
}


UINT StartAccept(void *pParam)
{
	while (1) {

		sockaddr_in from;
		int fromlen = sizeof(from);

		SOCKET msg_socket = accept(gListen_Socket, (struct sockaddr*)&from, &fromlen);

		AfxBeginThread(UserToProxyThread, (LPVOID)&msg_socket);
	}

	return true;
}


// Setup chanel and read data from local , send data to remote
UINT UserToProxyThread( void *pParam)
{
	SOCKET * msg_socket = (SOCKET*)pParam;

	if ( *msg_socket == INVALID_SOCKET) {
		printf("\nError  in accept ");
		return -5;
	}

	SocketPair sockets_pair;

	sockets_pair.is_user_proxy_closed = FALSE;
	sockets_pair.is_proxy_server_closed = TRUE;
	sockets_pair.user_proxy = *msg_socket;
	
	char chBuffer[ BUFSIZE + 1];

	int readed_bytes_count = recv( sockets_pair.user_proxy, chBuffer, BUFSIZE, 0);

	if (readed_bytes_count == SOCKET_ERROR){

		printf("\nError recv");
		
		if ( sockets_pair.is_user_proxy_closed == FALSE){

			closesocket( sockets_pair.user_proxy);
			sockets_pair.is_user_proxy_closed = TRUE;			
		}

		return 1;
	}

	if (readed_bytes_count == 0){

		printf("\nClient close connection\n");
		
		if ( sockets_pair.is_user_proxy_closed == FALSE){

			closesocket(sockets_pair.user_proxy);
			sockets_pair.is_user_proxy_closed = TRUE;
		}

		return 1;
	}


#ifdef _DEBUG

	chBuffer[readed_bytes_count] = 0;
	printf("\nReceived %d bytes,data\n[%s]\nfrom client\n", readed_bytes_count, chBuffer);
#endif

	// Connection with 
	ProxyParam proxy_param;

	proxy_param.pSocketsPair = &sockets_pair;
	proxy_param.hUserSvrOK = CreateEvent(NULL, TRUE, FALSE, NULL);

	proxy_param.chAddress = SQL_ADDRESS;
	proxy_param.iPort = SQL_PORT;

	CWinThread *pChildThread = AfxBeginThread(ProxyToServer, (LPVOID)&proxy_param);
	::WaitForSingleObject( proxy_param.hUserSvrOK, 60000);  //Wait for connection between proxy and remote server
	::CloseHandle( proxy_param.hUserSvrOK);	

	int sended_bytes_count;

	while ( sockets_pair.is_proxy_server_closed == FALSE && sockets_pair.is_user_proxy_closed == FALSE) {		

		sended_bytes_count = send(sockets_pair.proxy_server, chBuffer, BUFSIZE, 0);

		if (sended_bytes_count == SOCKET_ERROR) {

			printf("\nsend to server failed:error%d\n", WSAGetLastError());

			if ( sockets_pair.is_proxy_server_closed == FALSE) {

				closesocket(sockets_pair.proxy_server);
				sockets_pair.is_proxy_server_closed = TRUE;
			}

			break;
		}
		else {
			
			readed_bytes_count = recv(sockets_pair.user_proxy, chBuffer, BUFSIZE, 0);


			if ( readed_bytes_count == SOCKET_ERROR) {

				printf("\nError recv from user");

				if ( sockets_pair.is_user_proxy_closed == FALSE) {

					closesocket( sockets_pair.user_proxy);
					sockets_pair.is_user_proxy_closed = TRUE;
				}

				break;

			} else if ( readed_bytes_count == 0) {

				printf("\nClient close connection\n");

				if ( sockets_pair.is_user_proxy_closed == FALSE) {

					closesocket( sockets_pair.user_proxy);
					sockets_pair.is_user_proxy_closed = TRUE;
				}

				break;
			}
			else {

				chBuffer[readed_bytes_count] = 0;
#ifdef _DEBUG
				printf("\nReceived %d bytes from client \n[%s]\n", readed_bytes_count, chBuffer);
#endif
			}

		}
	}

	if (sockets_pair.is_proxy_server_closed == FALSE){

		closesocket(sockets_pair.proxy_server);
		sockets_pair.is_proxy_server_closed = TRUE;
	}

	if (sockets_pair.is_user_proxy_closed == FALSE){

		closesocket(sockets_pair.user_proxy);
		sockets_pair.is_user_proxy_closed = TRUE;
	}

	::WaitForSingleObject( pChildThread->m_hThread, 20000);  //Should check the reture value

	return 0;
}

// Read data from remote and send data to local
UINT ProxyToServer(LPVOID pParam)
{	
	unsigned int addr;

	struct sockaddr_in server;
	
	struct hostent *hp;
	
	int socket_type = SOCK_STREAM;

	ProxyParam * pProxyParam = (ProxyParam*)pParam;
	
	char *server_name = pProxyParam->chAddress;
	unsigned short port = pProxyParam->iPort;

	if ( isalpha( server_name[0])) {   /* server address is a name */
		
		hp = gethostbyname(server_name);
	} else { /* Convert nnn.nnn address to a usable one */
		
		addr = inet_addr(server_name);
		hp = gethostbyaddr( (char *)&addr, 4, AF_INET);
	}

	if ( hp == NULL) {
		fprintf(stderr, "Proxy: Cannot resolve address [%s]: Error %d\n",	server_name, WSAGetLastError());
		::SetEvent( pProxyParam->hUserSvrOK);

		return 0;
	}

	memset( &server, 0, sizeof(server));
	memcpy( &(server.sin_addr), hp->h_addr, hp->h_length);

	server.sin_family = hp->h_addrtype;
	server.sin_port = htons(port);

	SOCKET conn_to_server = socket(AF_INET, socket_type, 0); /* Open a socket */

	if ( conn_to_server < 0) {

		fprintf(stderr, "\nProxy: Error opening socket: Error %d\n",	WSAGetLastError());

		pProxyParam->pSocketsPair->is_proxy_server_closed = TRUE;
		::SetEvent( pProxyParam->hUserSvrOK);

		return -1;
	}


#ifdef _DEBUG
	printf("\nProxy connected to: %s\n", hp->h_name);
#endif

	if ( connect(conn_to_server, (struct sockaddr*)&server, sizeof(server))	== SOCKET_ERROR) {

		fprintf(stderr, "\nconnect() failed: %d\n", WSAGetLastError());
		pProxyParam->pSocketsPair->is_proxy_server_closed = TRUE;
		::SetEvent(pProxyParam->hUserSvrOK);

		return -1;
	}

	pProxyParam->pSocketsPair->proxy_server = conn_to_server;
	pProxyParam->pSocketsPair->is_proxy_server_closed = FALSE;
	::SetEvent( pProxyParam->hUserSvrOK);

	char chBuffer[ BUFSIZE + 1];
	int readed_bytes_count, sended_bytes_count;

	while ( pProxyParam->pSocketsPair->is_proxy_server_closed == FALSE && pProxyParam->pSocketsPair->is_user_proxy_closed == FALSE){
		
		readed_bytes_count = recv(pProxyParam->pSocketsPair->proxy_server, chBuffer, BUFSIZE, 0);
		
		if ( readed_bytes_count == SOCKET_ERROR) {
		
			fprintf( stderr, "\nrecv to server failed: error %d\n", WSAGetLastError());
			closesocket( pProxyParam->pSocketsPair->proxy_server);
			pProxyParam->pSocketsPair->is_proxy_server_closed = TRUE;

			break;
		} else if ( readed_bytes_count == 0) {
		
			printf("\nServer closed connection\n");
			closesocket( pProxyParam->pSocketsPair->proxy_server);
			pProxyParam->pSocketsPair->is_proxy_server_closed = TRUE;

			break;
		}
		else {

			chBuffer[ readed_bytes_count] = 0;

#ifdef _DEBUG	
			printf("\nReceived %d bytes from server \n[%s]\n", readed_bytes_count, chBuffer);
#endif

			sended_bytes_count = send( pProxyParam->pSocketsPair->user_proxy, chBuffer, BUFSIZE, 0);

			if ( sended_bytes_count == SOCKET_ERROR) {

				fprintf( stderr, "\nsend to client failed: error %d\n", WSAGetLastError());

				closesocket(pProxyParam->pSocketsPair->user_proxy);
				pProxyParam->pSocketsPair->is_user_proxy_closed = TRUE;

				break;
			}

		}
	}

	if ( pProxyParam->pSocketsPair->is_proxy_server_closed == FALSE){

		closesocket( pProxyParam->pSocketsPair->proxy_server);
		pProxyParam->pSocketsPair->is_proxy_server_closed = TRUE;
	}

	if ( pProxyParam->pSocketsPair->is_user_proxy_closed == FALSE){

		closesocket( pProxyParam->pSocketsPair->user_proxy);
		pProxyParam->pSocketsPair->is_user_proxy_closed = TRUE;
	}
	
	return 1;
}



int _tmain(int argc, TCHAR* argv[], TCHAR* envp[]){

	int nRetCode = 0;

	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		cerr << _T("\nFatal Error: MFC initialization failed") << endl;
		nRetCode = 1;
	}
	else
	{
		StartServer();

		while (1)
			if (getchar() == 'q')
				break;

		CloseServer();
	}

	return nRetCode;
}


