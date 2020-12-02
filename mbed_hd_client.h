// ----------------------------------------------------------------------------
// Copyright 2019 ARM Ltd.
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

#ifndef MBED_HD_CLIENT_H
#define MBED_HD_CLIENT_H

#include "mbed.h"


#define HDC15_MAX_DEVICE_ID_LENGHT    15          // 设备ID字节数
#define HDC15_MAX_PROGRAM_GUID_LENGHT    39

#define HDC15_MD5_LENGHT             32           // MD5长度字节数

#define HDC15_LOCAL_TCP_VERSION       0x1000005   // TCP传输协议版本

#define HDC15_LOCAL_UDP_VERSION       0x1000005   // UDP传输协议版本


#define HDC15_UDP_PORT 10001
#define HDC15_TCP_PORT 10001

/* the delay(ms) between two receive */
#define HDC15_UDP_RECV_TIME_DELAY_MS             10
/* NTP receive timeout(S) */
#define HDC15_UDP_GET_TIMEOUT                5

#define HDC15_DEVICE_NUM  1
#define HDC15_PROGRAM_GUID_NUM  2

#define HDC15_GUID_SIZE  33

#define HDC15_TCP_HEADER_LENGTH   12

enum HDC15_CmdType
{
    Unknown = -1,
    TcpHeartbeatAsk = 0x005f,      //< TCP心跳包请求
    TcpHeartbeatAnswer = 0x0060,   //< TCP心跳包反馈
    SearchDeviceAsk = 0x1001,      //< 搜索设备请求
    SearchDeviceAnswer = 0x1002,   //< 搜索设备应答
    ErrorAnswer = 0x2000,          //< 出错反馈
    SDKServiceAsk = 0x2001,        //< 版本协商请求
    SDKServiceAnswer = 0x2002,     //< 版本协商应答
    SDKCmdAsk = 0x2003,            //< sdk命令请求
    SDKCmdAnswer = 0x2004,         //< sdk命令反馈
    FileStartAsk = 0x8001,         //< 文件开始传输请求
    FileStartAnswer = 0x8002,      //< 文件开始传输应答
    FileContentAsk = 0x8003,       //< 携带文件内容的请求
    FileContentAnswer = 0x8004,    //< 写文件内容的应答
    FileEndAsk = 0x8005,           //< 文件结束传输请求
    FileEndAnswer = 0x8006,        //< 文件结束传输应答
    ReadFileAsk = 0x8007,          //< 回读文件请求
    ReadFileAnswer = 0x8008,       //< 回读文件应答

};

enum HDC15_ErrorCode
{
    kUnknown = -1,
    kSuccess = 0,
    kWriteFinish,           //< 写文件完成
    kProcessError,          //< 流程错误
    kVersionTooLow,         //< 版本过低
    kDeviceOccupa,          //< 设备被占用
    kFileOccupa,            //< 文件被占用
    kReadFileExcessive,     //< 回读文件用户过多
    kInvalidPacketLen,      //< 数据包长度错误
    kInvalidParam,          //< 无效的参数
    kNotSpaceToSave,        //< 存储空间不够
    kCreateFileFailed,      //< 创建文件失败
    kWriteFileFailed,       //< 写文件失败
    kReadFileFailed,        //< 读文件失败
    kInvalidFileData,       //< 无效的文件数据
    kFileContentError,      //< 文件内容出错
    kOpenFileFailed,        //< 打开文件失败
    kSeekFileFailed,        //< 定位文件失败
    kRenameFailed,          //< 重命名失败
    kFileNotFound,          //< 文件未找到
    kFileNotFinish,         //< 文件未接收完成
    kXmlCmdTooLong,         //< xml命令过长
    kInvalidXmlIndex,       //< 无效的xml命令索引值
    kParseXmlFailed,        //< 解析xml出错
    kInvalidMethod,         //< 无效的方法名
    kMemoryFailed,          //< 内存错误
    kSystemError,           //< 系统错误
    kUnsupportVideo,        //< 不支持的视频
    kNotMediaFile,          //< 不是多媒体文件
    kParseVideoFailed,      //< 解析视频文件失败
    kUnsupportFrameRate,    //< 不支持的波特率
    kUnsupportResolution,   //< 不支持的分辨率(视频)
    kUnsupportFormat,       //< 不支持的格式(视频)
    kUnsupportDuration,     //< 不支持的时间长度(视频)
    kDownloadFileFailed,    //< 下载文件失败

    kScreenNodeIsNull,
    kNodeExist,
    kNodeNotExist,
    kPluginNotExist,
    kCheckLicenseFailed,    //< 校验license失败
    kNotFoundWifiModule,    //< 未找到wifi模块
    kTestWifiUnsuccessful,  //< 测试wifi模块未
    kRunningError,          //< 运行错误
    kUnsupportMethod,       //< 不支持的方法
    kInvalidGUID,           //< 非法的guid

    kDelayRespond,          //< 延迟反馈
    kShortlyReturn,         //< 直接返回, 不进行xml转换

    KConnectionFailed,//套接字不能连接

    kCount,
};

typedef struct HDC15_UdpHeader
{
    uint32_t version;    //< 版本号
    uint16_t cmd;        //< 命令值
} HDC15_UdpHeader;


typedef struct HDC15_UdpExt
{
    uint32_t version;                        //< 版本号
    uint16_t cmd;                            //< 命令值
    uint8_t   devID[HDC15_MAX_DEVICE_ID_LENGHT];    //< 设备ID
    uint32_t    chanege;                         //< 扩展数据起始地址
} HDC15_UdpResponse;

typedef struct HDC15_Device
{
    char ip_addr[NSAPI_IP_SIZE];
    uint32_t version;                        //< 版本号
    uint32_t  chanege;                         //< 扩展数据起始地址
    uint8_t   id[HDC15_MAX_DEVICE_ID_LENGHT];    //< 设备ID
} HDC15_Device;

typedef struct HDC15_Program_Guid
{
    char guid[HDC15_MAX_PROGRAM_GUID_LENGHT];
} HDC15_Program_Guid;

typedef struct HDC15_Device_List
{
    uint8_t  num;
    HDC15_Device dev[HDC15_DEVICE_NUM];
} HDC15_Device_List;

typedef struct HDC15_Program_Guid_List
{
    uint8_t  num;
    HDC15_Program_Guid program[HDC15_PROGRAM_GUID_NUM];
} HDC15_Program_Guid_List;


#ifdef __cplusplus
extern "C" {
#endif

int hd_scan(void);
int hd_get_guid(int id);
int hd_textcontrol(int id, int guid, bool en, const char *text_string);
int hd_playcontrol(int id, int guid, bool en);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif /* MBED_HD_CLIENT_H */