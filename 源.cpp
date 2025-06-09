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

    // �����ļ���
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    string filename(buffer, bytes_received);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "�����ļ�: " << filename << endl;
    }

    // ���ļ�׼��д��
    FILE* file = nullptr;
    if (fopen_s(&file, filename.c_str(), "wb") != 0 || !file) {
        lock_guard<mutex> lock(cout_mutex);
        cerr << "�޷������ļ�: " << filename << endl;
        closesocket(client_socket);
        return;
    }

    // �����ļ�����
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0))) {
        if (bytes_received < 0) {
            lock_guard<mutex> lock(cout_mutex);
            cerr << "�������ݴ���" << endl;
            break;
        }
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    closesocket(client_socket);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "�ļ��������: " << filename << endl;
    }
}

void tcp_server() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "WSAStartupʧ��" << endl;
        return;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "����TCP socketʧ��" << endl;
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "TCP�󶨶˿�ʧ��" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "TCP����ʧ��" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "TCP�����������������˿�: " << TCP_PORT << endl;

    while (true) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            cerr << "��������ʧ��" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "��TCP��������: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;
        }

        thread(handle_tcp_client, client_socket).detach();
    }

    closesocket(server_socket);
    WSACleanup();
}

void udp_server() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "WSAStartupʧ��" << endl;
        return;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "����UDP socketʧ��" << endl;
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "UDP�󶨶˿�ʧ��" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "UDP�����������������˿�: " << UDP_PORT << endl;

    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (true) {
        int bytes_received = recvfrom(server_socket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&client_addr, &client_addr_len);
        if (bytes_received == SOCKET_ERROR) {
            cerr << "��������ʧ��" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        string message(buffer, bytes_received);

        lock_guard<mutex> lock(cout_mutex);
        cout << "�յ�UDP��Ϣ���� " << client_ip << ":" << ntohs(client_addr.sin_port)
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