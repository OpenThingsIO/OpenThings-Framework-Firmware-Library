/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Linux Ethernet functions header file
 * This file is based on Richard Zimmerman's sprinklers_pi program
 * Copyright (c) 2013 Richard Zimmerman
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _ETHERPORT_H_
#define _ETHERPORT_H_

#if defined(ARDUINO)

#else // headers for RPI/BBB/Linux

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <string>

#ifdef __APPLE__
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif

class EthernetServer;

class EthernetClient {
public:
	EthernetClient();
	EthernetClient(int sock);
	~EthernetClient();
	virtual int connect(const char *server, uint16_t port);
	virtual bool connected();
	virtual void stop();
	virtual int read(uint8_t *buf, size_t size);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual operator bool();
	virtual int GetSocket() {
		return m_sock;
	}
    virtual void flush();
	virtual bool available();
	virtual void setTimeout(int msec);
protected:
	int m_sock = 0;
	bool m_connected;
	friend class EthernetServer;
};

class EthernetClientSsl : public EthernetClient {
public:
	EthernetClientSsl();
	EthernetClientSsl(int sock);
	~EthernetClientSsl();
	virtual int connect(const char *server, uint16_t port);
	virtual bool connected();
	virtual void stop();
	virtual int read(uint8_t *buf, size_t size);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual operator bool();
protected:
	SSL* ssl;
};

class EthernetServer {
public:
	EthernetServer(uint16_t port);
	~EthernetServer();

	virtual bool begin();
	virtual EthernetClient available();
private:
	uint16_t m_port;
	int m_sock;
};
#endif

#endif /* _ETHERPORT_H_ */