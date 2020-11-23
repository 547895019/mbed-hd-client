// ----------------------------------------------------------------------------
// Copyright 2016-2020 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------
#include "mbed_hd_client.h"
#include "mbed-trace/mbed_trace.h" // Required for mbed_trace_*

#define TRACE_GROUP  "mbed_hd_client"

static HDC15_Device_List hd_dev;

static const char GetIFVersion_xml[] = {
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<sdk guid=\"##GUID\">\n"
    "<in method=\"GetIFVersion\">\n"
    "<version value=\"1000000\"/>\n"
    "</in>\n"
    "</sdk>\n"};

int hd_scan_device(void)
{
    /* Create a UDP socket. */
    int sockfd;
    SocketAddress serv_addr;
    SocketAddress send_addr("255.255.255.255",HDC15_UDP_PORT);
    
    rt_tick_t start = 0;

    memset(&hd_dev, 0, sizeof(HDC15_Device_List));

    Socket *sockfd = new Socket;

    if (sockfd == NULL)
    {
        tr_err("Create socket failed.");
        return -1;
    }

    HDC15_UdpHeader packet;
    packet.version = (uint32_t) HDC15_LOCAL_UDP_VERSION;
    packet.cmd = (uint16_t)SearchDeviceAsk;

    const int optval = 1;
    /* set to receive broadcast packet */
    sockfd->setsockopt(SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    if(sockfd->sendto(send_addr,(char *) &packet, 6) <= 0)
    {
        sockfd->close();
        delete sockfd;
        return -1;
    }
	sockfd->set_timeout(3000);	

    hd_dev.num = 0;
   
	for (int i = 0; i < HDC15_DEVICE_NUM; i++)
	{
		HDC15_UdpResponse recv_packet;
		/* non-blocking receive the packet back from the server. If n == -1, it failed. */
		int n = sockfd->recvfrom(&serv_addr, (char *) &recv_packet, sizeof(HDC15_UdpResponse));
		if (n == 25)
		{
			if(recv_packet.cmd == SearchDeviceAnswer){
			hd_dev.dev[hd_dev.num].chanege= recv_packet.chanege;
			memcpy(&hd_dev.dev[hd_dev.num].ip_addr,serv_addr.get_addr(),sizeof(nsapi_addr_t));
			memcpy(&(hd_dev.dev[hd_dev.num].id), &(recv_packet.devID), HDC15_MAX_DEVICE_ID_LENGHT);
			hd_dev.dev[hd_dev.num].version = recv_packet.version;
			//printf("%d: %x %s %s %x .\n",hd_dev.num, hd_dev.dev[hd_dev.num].version,hd_dev.dev[hd_dev.num].id,inet_ntoa(hd_dev.dev[hd_dev.num].ip),hd_dev.dev[hd_dev.num].chanege);
			hd_dev.num++;
			}
		}
	}
    
    __exit:
    sockfd->close();
    delete sockfd;
    return hd_dev.num;
}