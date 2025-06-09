/**
 * ����TCP/UDP�Ķ��߳��ļ����������
 * ���ܣ�
 * 1. TCP�˿�(1759)���տͻ����ϴ����ļ�
 * 2. UDP�˿�(1759)���տͻ��˷��͵��ı���Ϣ
 * 3. ʹ�ö��̴߳���������
 */

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // �Զ�����Winsock��

using namespace std;

// ��������
#define UDP_PORT 1759    // UDP����˿�
#define TCP_PORT 1759    // TCP����˿�
#define BUFFER_SIZE 1024 // ���ݻ�������С

mutex cout_mutex; // ����̨����������̰߳�ȫ��

/**
 * ����TCP�ͻ������ӵ��̺߳���
 * @param client_socket �ͻ���Socket������
 */
void handle_tcp_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // 1. �����ļ������ͻ������ȷ����ļ�����
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    string filename(buffer, bytes_received);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "[TCP] �����ļ�: " << filename << endl;
    }

    // 2. ���������ļ�׼��д��
    FILE* file = nullptr;
    if (fopen_s(&file, filename.c_str(), "wb") != 0 || !file) {
        lock_guard<mutex> lock(cout_mutex);
        cerr << "[TCP] �ļ�����ʧ��: " << filename << endl;
        closesocket(client_socket);
        return;
    }

    // 3. ѭ�������ļ����ݲ�д�뱾��
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    // 4. �ر���Դ
    fclose(file);
    closesocket(client_socket);
    {
        lock_guard<mutex> lock(cout_mutex);
        cout << "[TCP] �ļ��������: " << filename << endl;
    }
}

//TCP�����������
 
void tcp_server() {
    // 1. ��ʼ��Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "[TCP] WSA��ʼ��ʧ��" << endl;
        return;
    }

    // 2. ����TCP Socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "[TCP] Socket����ʧ��" << endl;
        WSACleanup();
        return;
    }

    // 3. �󶨶˿�
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // ������������

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "[TCP] �˿ڰ�ʧ��" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    // 4. ��ʼ����
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "[TCP] ����ʧ��" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "[TCP] �������������˿�: " << TCP_PORT << endl;

    // 5. ��ѭ�������ܿͻ�������
    while (true) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);

        if (client_socket == INVALID_SOCKET) {
            cerr << "[TCP] ��������ʧ��" << endl;
            continue;
        }

        // ��ӡ�ͻ�����Ϣ
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "[TCP] �ͻ�������: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;
        }

        // �������̴߳���ͻ���
        thread(handle_tcp_client, client_socket).detach();
    }

    // 6. ������Դ��ʵ�ʲ���ִ�е����
    closesocket(server_socket);
    WSACleanup();
}

//UDP�����������

void udp_server() {
    // 1. ��ʼ��Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        cerr << "[UDP] WSA��ʼ��ʧ��" << endl;
        return;
    }

    // 2. ����UDP Socket
    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket == INVALID_SOCKET) {
        cerr << "[UDP] Socket����ʧ��" << endl;
        WSACleanup();
        return;
    }

    // 3. �󶨶˿�
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "[UDP] �˿ڰ�ʧ��" << endl;
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    cout << "[UDP] �������������˿�: " << UDP_PORT << endl;

    // 4. ��ѭ��������UDP��Ϣ
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);

    while (true) {
        int bytes_received = recvfrom(server_socket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&client_addr, &addr_len);
        if (bytes_received == SOCKET_ERROR) {
            cerr << "[UDP] ����ʧ��" << endl;
            continue;
        }

        // ��ӡ�ͻ�����Ϣ
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        string message(buffer, bytes_received);

        lock_guard<mutex> lock(cout_mutex);
        cout << "[UDP] ���� " << client_ip << ":" << ntohs(client_addr.sin_port)
            << " ����Ϣ: " << message << endl;
    }

    // 5. ������Դ��ʵ�ʲ���ִ�е����
    closesocket(server_socket);
    WSACleanup();
}

int main() {
    // ����TCP��UDP�����߳�
    thread tcp_thread(tcp_server);
    thread udp_thread(udp_server);

    // �ȴ��߳̽�����ʵ�ʻ�һֱ���У�
    tcp_thread.join();
    udp_thread.join();

    return 0;
}