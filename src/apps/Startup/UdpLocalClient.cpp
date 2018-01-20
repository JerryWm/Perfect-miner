#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <share/cpp/Common.cpp>
#include <share/lib/winsock/winsock.h>

class UdpLocalClient {
	private:
		WSAData wsadata;
		SOCKET socket = INVALID_SOCKET;
		bool disconnected = true;
		
	public:
		UdpLocalClient() {
			::WSAStartup(MAKEWORD( 1, 1 ), &this->wsadata);
		}
		~UdpLocalClient() {
			this->close();
		}
		
		bool connect(uint16_t port) {
			this->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if ( this->socket == INVALID_SOCKET ) {
				return false;
			}
			
			struct sockaddr_in addr;
			MACRO_ZERO_MEMORY(addr);
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			addr.sin_port = htons(port);
			
			if ( ::connect(this->socket, (const struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ) {
				this->close();
				return false;
			}
			
			this->disconnected = false;
			/**
			u_long fl_true = 1;
			if( ioctlsocket(this->socket, FIONBIO, &fl_true ) == SOCKET_ERROR ) {
				this->close();
				return false;
			}			
			*/
			return true;
		}
		
		bool send(uint8_t *buf, uint32_t size) {
			if ( this->disconnected ) { return false; }
			
			if ( ::send(this->socket, (char*)buf, size, 0) == SOCKET_ERROR ) {
				this->close();
				return false;
			}
			
			return true;
		}
		
		bool recv(uint8_t *buf, uint32_t *size) {
			if ( this->disconnected ) { return false; }
			
			*size = 0;
			
			u_long rSize = 0;
			if ( ::ioctlsocket(this->socket, FIONREAD, &rSize) == SOCKET_ERROR ) {
				this->close();
				return false;
			}
			
			if ( rSize ) {
				int ret = ::recv(this->socket, (char*)buf, 4096, 0);
				if ( ret == SOCKET_ERROR ) {
					this->close();
					return false;
				}
				
				*size = ret;
				
				return true;
			}
			
			return true;
		}
		
		void close() {
			if ( this->socket != INVALID_SOCKET ) {
				::closesocket(this->socket);
				this->socket = INVALID_SOCKET;
			}
			
			if ( this->disconnected ) {
				return;
			}
			
			this->disconnected = false;
		}
};
