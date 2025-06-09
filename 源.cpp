#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define UDP_PORT 1759
#define TCP_PORT 1759
#define BUFFER_SIZE 1024
#define _CRT_SECURE_NO_WARNINGS

mutex cout_mutex;

void handle_tcp_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // 接收文件名
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    string filename(buffer, bytes_received);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "接收文件: " << filename << endl;
    }

    // 打开文件准备写入
    FILE* file = nullptr;
    if (fopen_s(&file, filename.c_str(), "wb") != 0 || !file) {
        lock_guard<mutex> lock(cout_mutex);
        cerr << "无法创建文件: " << filename << endl;
        closesocket(client_socket);
        return;
    }

    // 接收文件数据
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0))) {
        if (bytes_received < 0) {
            lock_guard<mutex> lock(cout_mutex);
            cerr << "接收数据错误" << endl;
            break;
        }
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    closesocket(client_socket);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "文件接收完成: " << filename << endl;
    }
}

void tcp_server() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "WSAStartup失败" << endl;
        return;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "创建TCP socket失败" << endl;
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "TCP绑定端口失败" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "TCP监听失败" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "TCP服务器启动，监听端口: " << TCP_PORT << endl;

    while (true) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            cerr << "接受连接失败" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "新TCP连接来自: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;
        }

        thread(handle_tcp_client, client_socket).detach();
    }

    closesocket(server_socket);
    WSACleanup();
}

void udp_server() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "WSAStartup失败" << endl;
        return;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "创建UDP socket失败" << endl;
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "UDP绑定端口失败" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "UDP服务器启动，监听端口: " << UDP_PORT << endl;

    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (true) {
        int bytes_received = recvfrom(server_socket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&client_addr, &client_addr_len);
        if (bytes_received == SOCKET_ERROR) {
            cerr << "接收数据失败" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        string message(buffer, bytes_received);

        lock_guard<mutex> lock(cout_mutex);
        cout << "收到UDP消息来自 " << client_ip << ":" << ntohs(client_addr.sin_port)
            << ": " << message << endl;
    }

    closesocket(server_socket);
    WSACleanup();
}

int main() {
    thread tcp_thread(tcp_server);
    thread udp_thread(udp_server);

    tcp_thread.join();
    udp_thread.join();

    return 0;
}