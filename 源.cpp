/**
 * 基于TCP/UDP的多线程文件传输服务器
 * 功能：
 * 1. TCP端口(1759)接收客户端上传的文件
 * 2. UDP端口(1759)接收客户端发送的文本消息
 * 3. 使用多线程处理并发请求
 */

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // 自动链接Winsock库

using namespace std;

// 常量定义
#define UDP_PORT 1759    // UDP服务端口
#define TCP_PORT 1759    // TCP服务端口
#define BUFFER_SIZE 1024 // 数据缓冲区大小

mutex cout_mutex; // 控制台输出锁（多线程安全）

/**
 * 处理TCP客户端连接的线程函数
 * @param client_socket 客户端Socket描述符
 */
void handle_tcp_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // 1. 接收文件名（客户端首先发送文件名）
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    string filename(buffer, bytes_received);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "[TCP] 接收文件: " << filename << endl;
    }

    // 2. 创建本地文件准备写入
    FILE* file = nullptr;
    if (fopen_s(&file, filename.c_str(), "wb") != 0 || !file) {
        lock_guard<mutex> lock(cout_mutex);
        cerr << "[TCP] 文件创建失败: " << filename << endl;
        closesocket(client_socket);
        return;
    }

    // 3. 循环接收文件数据并写入本地
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    // 4. 关闭资源
    fclose(file);
    closesocket(client_socket);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "[TCP] 文件接收完成: " << filename << endl;
    }
}

//TCP服务端主函数
 
void tcp_server() {
    // 1. 初始化Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "[TCP] WSA初始化失败" << endl;
        return;
    }

    // 2. 创建TCP Socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "[TCP] Socket创建失败" << endl;
        WSACleanup();
        return;
    }

    // 3. 绑定端口
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "[TCP] 端口绑定失败" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    // 4. 开始监听
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[TCP] 监听失败" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "[TCP] 服务器启动，端口: " << TCP_PORT << endl;

    // 5. 主循环：接受客户端连接
    while (true) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);

        if (client_socket == INVALID_SOCKET) {
            cerr << "[TCP] 接受连接失败" << endl;
            continue;
        }

        // 打印客户端信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "[TCP] 客户端连接: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;
        }

        // 创建新线程处理客户端
        thread(handle_tcp_client, client_socket).detach();
    }

    // 6. 清理资源（实际不会执行到这里）
    closesocket(server_socket);
    WSACleanup();
}

//UDP服务端主函数

void udp_server() {
    // 1. 初始化Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "[UDP] WSA初始化失败" << endl;
        return;
    }

    // 2. 创建UDP Socket
    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "[UDP] Socket创建失败" << endl;
        WSACleanup();
        return;
    }

    // 3. 绑定端口
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "[UDP] 端口绑定失败" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "[UDP] 服务器启动，端口: " << UDP_PORT << endl;

    // 4. 主循环：接收UDP消息
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);

    while (true) {
        int bytes_received = recvfrom(server_socket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&client_addr, &addr_len);
        if (bytes_received == SOCKET_ERROR) {
            cerr << "[UDP] 接收失败" << endl;
            continue;
        }

        // 打印客户端消息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        string message(buffer, bytes_received);

        lock_guard<mutex> lock(cout_mutex);
        cout << "[UDP] 来自 " << client_ip << ":" << ntohs(client_addr.sin_port)
            << " 的消息: " << message << endl;
    }

    // 5. 清理资源（实际不会执行到这里）
    closesocket(server_socket);
    WSACleanup();
}

int main() {
    // 启动TCP和UDP服务线程
    thread tcp_thread(tcp_server);
    thread udp_thread(udp_server);

    // 等待线程结束（实际会一直运行）
    tcp_thread.join();
    udp_thread.join();

    return 0;
}