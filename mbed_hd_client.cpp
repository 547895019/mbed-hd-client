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
#include "tinyxml.h"
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <string>

#define TRACE_GROUP "mbed_hd_client"

#define BUFSZ 2048

static char tcp_data[BUFSZ];
static char recv_xml[BUFSZ];
static HDC15_Device_List hd_dev;
static HDC15_Program_Guid_List hd_program_guid[HDC15_DEVICE_NUM];
static char guid[HDC15_GUID_SIZE];

static const char get_ifversion_xml[] = {
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<sdk guid=\"##GUID\">\n"
    "<in method=\"GetIFVersion\">\n"
    "<version value=\"1000000\"/>\n"
    "</in>\n"
    "</sdk>\n"};

static const char cmd_xml[] = {"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                               "<sdk guid=\"##GUID\">"
                               "<in method=\"##CMD\"></in>"
                               "</sdk>"};

static const char add_program_xml[]{
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<sdk guid=\"##GUID\">"
    "<in method=\"AddProgram\">"
    "<screen>"
    "<program guid=\"d0014343-5c25-4719-af95-2eccf2e74550\" type=\"normal\">"
    "<playControl count=\"1\" disabled= \"true\"/>"
    "<area guid=\"e2fc3d5d-190b-460e-9333-1e51f5b8ff03\">"
    "<rectangle x=\"0\" y=\"0\" width=\"640\" height=\"64\"/>"
    "<resources>"
    "<text guid=\"54f188b2-43bc-47ef-b131-28f23e880c0e\" singleLine=\"true\">"
    "<string>欢迎光临</string>"
    "<effect in=\"0\" out=\"20\" inSpeed=\"4\" outSpeed=\"4\" duration=\"50\"/>"
    "<font name=\"宋体\"  bold=\"False\" italic=\"False\" underline=\"False\" "
    "size=\"48\"/>"
    "</text>"
    "</resources>"
    "</area>"
    "</program>"
    "</screen>"
    "</in>"
    "</sdk>"};

static const char textcontrol_xml[]{
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<sdk guid=\"##GUID\">"
    "<in method=\"UpdateProgram\">"
    "<screen>"
    "<program guid=\"##guid\">"
    "<playControl disabled= \"##en\"/>"
    "<area guid=\"e2fc3d5d-190b-460e-9333-1e51f5b8ff03\">"
    "<resources>"
    "<text guid=\"54f188b2-43bc-47ef-b131-28f23e880c0e\">"
    "<string>##str</string>"
    "</text>"
    "</resources>"
    "</area>"
    "</program>"
    "</screen>"
    "</in>"
    "</sdk>"};

static const char playcontrol_xml[]{"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                                    "<sdk guid=\"##GUID\">"
                                    "<in method=\"UpdateProgram\">"
                                    "<screen>"
                                    "<program guid=\"##guid\">"
                                    "<playControl disabled= \"##en\"/>"
                                    "</program>"
                                    "</screen>"
                                    "</in>"
                                    "</sdk>"};

int hd_scan(void) {
  /* Create a UDP socket. */

  UDPSocket *sock = new UDPSocket;

  if (sock == NULL) {
    tr_err("Create socket failed.");
    return -1;
  }
  NetworkInterface *net = NetworkInterface::get_default_instance();
  sock->open(net);

  // const int optval = 1;
  /* set to receive broadcast packet */
  // sock->setsockopt(SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
  SocketAddress send_addr("255.255.255.255", HDC15_UDP_PORT);
  HDC15_UdpHeader packet;
  packet.version = (uint32_t)HDC15_LOCAL_UDP_VERSION;
  packet.cmd = (uint16_t)SearchDeviceAsk;
  if (sock->sendto(send_addr, (char *)&packet, 6) != 6) {
    sock->close();
    delete sock;
    tr_err("Sendto failed.\n");
    return -1;
  }
  sock->set_timeout(3000);

  hd_dev.num = 0;
  memset(&hd_dev, 0, sizeof(HDC15_Device_List));

  for (int i = 0; i < HDC15_DEVICE_NUM; i++) {
    HDC15_UdpResponse recv_packet;
    /* non-blocking receive the packet back from the server. If n == -1, it
     * failed. */
    SocketAddress serv_addr;
    int n = sock->recvfrom(&serv_addr, (char *)&recv_packet, 25);
    // printf("recvfrom %s\n",(char *) &recv_packet);
    if (n == 25) {
      if (recv_packet.cmd == SearchDeviceAnswer) {
        hd_dev.dev[hd_dev.num].chanege = recv_packet.chanege;

        memcpy((char *)(hd_dev.dev[hd_dev.num].ip_addr),
               serv_addr.get_ip_address(), NSAPI_IP_SIZE);
        memcpy((char *)(hd_dev.dev[hd_dev.num].id), &(recv_packet.devID),
               HDC15_MAX_DEVICE_ID_LENGHT);
        hd_dev.dev[hd_dev.num].version = recv_packet.version;
        tr_info("%d: %x %s %s", hd_dev.num, hd_dev.dev[hd_dev.num].version,
               hd_dev.dev[hd_dev.num].id, hd_dev.dev[hd_dev.num].ip_addr);
        // printf("%x .\n",hd_dev.dev[hd_dev.num].chanege);
        hd_dev.num++;
      }
    }
  }

__exit:
  sock->close();
  delete sock;
  return hd_dev.num;
}

static int hd_send_xml(int id, const char *xml, char *recv_data) {
  /* Create a UDP socket. */
  if(id >= hd_dev.num){
      tr_err("id >= hd_dev.num.");
        return -1;
  }
  if (recv_data) {
    memset(recv_data, 0, BUFSZ);
  }
  TCPSocket *sock = new TCPSocket;
  if (sock == NULL) {
    tr_err("Create socket failed.");
    return -1;
  }
  NetworkInterface *net = NetworkInterface::get_default_instance();
  sock->open(net);

  SocketAddress send_addr;
  send_addr.set_port(HDC15_TCP_PORT);
  send_addr.set_ip_address(hd_dev.dev[id].ip_addr);
  sock->connect(send_addr);

  int len = 8;
  *(uint16_t *)&tcp_data[0] = len;
  *(uint16_t *)&tcp_data[2] = SDKServiceAsk;
  *(uint32_t *)&tcp_data[4] = HDC15_LOCAL_TCP_VERSION;
  if (sock->send((char *)tcp_data, len) != len) {
    sock->close();
    delete sock;
    tr_err("Sendto failed.\n");
    return -1;
  }
  sock->set_timeout(3000);
  memset(tcp_data, 0, BUFSZ);
  int n = sock->recv((char *)tcp_data, BUFSZ - 1);
  if (n != 8) {
    sock->close();
    delete sock;
    tr_err("Recv failed.\n");
    return -1;
  }
  int xml_len = strlen(get_ifversion_xml);
  len = xml_len + HDC15_TCP_HEADER_LENGTH;
  *(uint16_t *)&tcp_data[0] = len;
  *(uint16_t *)&tcp_data[2] = SDKCmdAsk;
  *(uint32_t *)&tcp_data[4] = xml_len;
  *(uint32_t *)&tcp_data[8] = 0;
  memcpy(&tcp_data[HDC15_TCP_HEADER_LENGTH], get_ifversion_xml, xml_len);
  // printf("send %s\n",(char *) &tcp_data[HDC15_TCP_HEADER_LENGTH]);
  if (sock->send((char *)tcp_data, len) != len) {
    sock->close();
    delete sock;
    tr_err("Sendto failed.\n");
    return -1;
  }

  memset(tcp_data, 0, BUFSZ);
  n = sock->recv((char *)tcp_data, BUFSZ - 1);
  if (n > HDC15_TCP_HEADER_LENGTH) {
    int total = *(uint32_t *)&tcp_data[4];
    int index = *(uint32_t *)&tcp_data[8];
    TiXmlDocument doc;
    // Parse Xml.
    doc.Parse(&tcp_data[HDC15_TCP_HEADER_LENGTH + index]);
    const char *attr = doc.RootElement()->Attribute("guid");
    if (attr) {
      strlcpy(guid, attr, sizeof(guid));
    } else {
      sock->close();
      delete sock;
      tr_err("Guid failed.\n");
      return -1;
    }
  }

  TiXmlDocument doc;
  doc.Parse(xml);
  TiXmlElement *element = doc.RootElement();
  if (element)
    element->SetAttribute("guid", (const char *)guid);
  TiXmlPrinter printer;
  doc.Accept(&printer);
  const char *xml_str = printer.CStr();
  // printf("%s\n", xml_str);
  xml_len = strlen(xml_str);
  len = xml_len + HDC15_TCP_HEADER_LENGTH;
  *(uint16_t *)&tcp_data[0] = len;
  *(uint16_t *)&tcp_data[2] = SDKCmdAsk;
  *(uint32_t *)&tcp_data[4] = xml_len;
  *(uint32_t *)&tcp_data[8] = 0;
  memcpy(&tcp_data[HDC15_TCP_HEADER_LENGTH], xml_str, xml_len);

  if (sock->send((char *)tcp_data, len) != len) {
    sock->close();
    delete sock;
    tr_err("Sendto failed.\n");
    return -1;
  }
  int tcp_data_index = 0;
  memset(tcp_data, 0, BUFSZ);
  n = sock->recv((char *)&tcp_data[tcp_data_index], BUFSZ - 1);
  if (n > HDC15_TCP_HEADER_LENGTH) {
    tcp_data_index += n;
    if (tcp_data_index >= (BUFSZ - 1)) {
      tr_err("recv len failed %d.\n", tcp_data_index);
      goto __exit;
    }
  }
  sock->set_timeout(100);
recv:
  n = sock->recv((char *)&tcp_data[tcp_data_index], BUFSZ - tcp_data_index - 1);
  if (n > 0) {
    tcp_data_index += n;
    if (tcp_data_index >= (BUFSZ - 1)) {
      tr_err("recv len failed %d.\n", tcp_data_index);
      goto __exit;
    }
  } else {
    int index = *(uint32_t *)&tcp_data[8];
    if (recv_data) {
      strlcpy(recv_data,
              (const char *)&tcp_data[HDC15_TCP_HEADER_LENGTH + index], BUFSZ);
    }
    // printf("%s\n",recv_data);
    goto __exit;
  }
  goto recv;
__exit:
  sock->close();
  delete sock;
  return 0;
}

static int hd_send_cmd(int id, const char *cmd, char *recv_data) {
 if(id >= hd_dev.num){
      tr_err("id >= hd_dev.num.");
        return -1;
  }
  TiXmlDocument doc;
  doc.Parse(cmd_xml);
  TiXmlElement *child = doc.RootElement();
  if (child) {
    TiXmlElement *child_next = child->FirstChildElement("in");
    if (child_next) {
      child_next->SetAttribute("method", cmd);
    }
  }
  TiXmlPrinter printer;
  doc.Accept(&printer);
  hd_send_xml(id, (const char *)(printer.CStr()), recv_data);
  if (recv_data) {
    return strlen((const char *)recv_data);
  } else {
    return 0;
  }
}

int hd_get_guid(int id) {
     if(id >= hd_dev.num){
      tr_err("id >= hd_dev.num.");
        return -1;
  }
  int n = hd_send_cmd(id, "GetProgram", recv_xml);
  if (n) {
    //printf("%s\n",recv_xml);
    TiXmlDocument doc;
    doc.Parse(recv_xml);
    TiXmlHandle docHandle(&doc);
    hd_program_guid[id].num = 0;
    int i = 0;
    while (true) {
      TiXmlElement *child = docHandle.FirstChild("sdk")
                                .FirstChild("out")
                                .FirstChild("screen")
                                .Child("program", i)
                                .ToElement();
      if (!child) {
        break;
      }
      // do something
      const char *attr = child->Attribute("guid");
      if (attr) {
        strlcpy(hd_program_guid[id].program[i].guid, attr, HDC15_MAX_PROGRAM_GUID_LENGHT);
        tr_info("%d %s", hd_program_guid[id].num, hd_program_guid[id].program[i].guid);
        hd_program_guid[id].num++;
        if (hd_program_guid[id].num >= HDC15_PROGRAM_GUID_NUM) {
          break;
        }
      }
      i++;
    }
  }
  return hd_program_guid[id].num;
}

int hd_playcontrol(int id, int guid, bool en) {
    if(id >= hd_dev.num){
      tr_err("id >= hd_dev.num.");
        return -1;
  }
  if(guid >= hd_program_guid[id].num){
      tr_err("guid >= hd_program_guid[id].num.");
        return -1;
  }
  TiXmlDocument doc;
  doc.Parse(playcontrol_xml);
  TiXmlHandle docHandle(&doc);
  // TiXmlElement* child = doc.RootElement();

  TiXmlElement *child_program = docHandle.FirstChild("sdk")
                                    .FirstChild("in")
                                    .FirstChild("screen")
                                    .FirstChild("program")
                                    .ToElement();
  if (child_program) {
    child_program->SetAttribute("guid", hd_program_guid[id].program[guid].guid);
  }
  TiXmlElement *child_playControl =
      child_program->FirstChildElement("playControl");
  if (child_playControl) {
    if (en) {
      child_playControl->SetAttribute("disabled", "false");
    } else {
      child_playControl->SetAttribute("disabled", "true");
    }
  }

  TiXmlPrinter printer;
  doc.Accept(&printer);
  // printf("%s\n",printer.CStr());
  hd_send_xml(id, (const char *)(printer.CStr()), NULL);
  return 0;
}

int hd_textcontrol(int id, int guid, bool en, const char *text_string) {
     if(id >= hd_dev.num){
      tr_err("id >= hd_dev.num.");
        return -1;
  }
  if(guid >= hd_program_guid[id].num){
      tr_err("guid >= hd_program_guid[id].num.");
        return -1;
  }
  TiXmlDocument doc;
  doc.Parse(add_program_xml);
  TiXmlHandle docHandle(&doc);
  // TiXmlElement* child = doc.RootElement();

  TiXmlElement *child_program = docHandle.FirstChild("sdk")
                                    .FirstChild("in")
                                    .FirstChild("screen")
                                    .FirstChild("program")
                                    .ToElement();
  if (child_program) {
    child_program->SetAttribute("guid", hd_program_guid[id].program[guid].guid);
    TiXmlElement *child_playControl =
        child_program->FirstChildElement("playControl");
    if (child_playControl) {
      if (en) {
        child_playControl->SetAttribute("disabled", "false");
      } else {
        child_playControl->SetAttribute("disabled", "true");
      }
    }
    TiXmlElement *child_string = child_program->FirstChildElement("area")
                                     ->FirstChildElement("resources")
                                     ->FirstChildElement("text")
                                     ->FirstChildElement("string");
    if (child_string) {
      TiXmlNode *child_node = child_string->FirstChild();
      TiXmlText *childText = child_node->ToText();
      childText->SetValue(text_string);
      // printf("child_string %s\n",childText->Value());
    }
  }

  TiXmlPrinter printer;
  doc.Accept(&printer);
  // printf("%s\n",printer.CStr());
  hd_send_xml(id, (const char *)(printer.CStr()), NULL);
  return 0;
}

#ifdef MBED_USER_ONEOS
#include "oneos.h"

#if MBED_CONF_ONEOS_OS_USING_SHELL

static void hd_test(int argc, char **argv) {
  if (argc < 3) {
    printf("Please input: hd_test <id> <num>\n");
    return;
  }
  int id = atoi(argv[1]);
  int num = atoi(argv[2]);
  int dev_num = hd_scan();
  if (dev_num && ((id < dev_num) && (id >= 0))) {
    hd_send_xml(id, add_program_xml, NULL);
    int n = hd_get_guid(id);
    if (n == 2) {
      hd_playcontrol(id, 0, false);
      char text_string[64];
      sprintf(text_string, "恭喜%d号机中奖啦！", num);
      hd_textcontrol(id, 1, true,
                     text_string);
      ThisThread::sleep_for(5s);
      hd_playcontrol(id, 1, false);
      hd_playcontrol(id, 0, true);
    }
  }
}
static void hd_cmd(int argc, char **argv) {
  if (argc < 3) {
    printf("Please input: hd_cmd <id> <cmd>\n");
    return;
  }

  int id = atoi(argv[1]);
  int dev_num = hd_scan();
  if (dev_num && ((id < dev_num) && (id >= 0))) {
    hd_send_cmd(id, argv[2], NULL);
  }
}
SH_CMD_EXPORT(hd_test, hd_test, "hd_test <id> <num>");
SH_CMD_EXPORT(hd_scan, hd_scan, "get device list");
SH_CMD_EXPORT(hd_cmd, hd_cmd, "hd_cmd <id> <cmd>");
#endif

#endif