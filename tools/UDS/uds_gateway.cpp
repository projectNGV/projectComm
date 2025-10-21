#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <algorithm> // for std::min
#include <csignal>
#include <thread> // for std::this_thread::sleep_for
#include <chrono> // for std::chrono::microseconds
#include <mutex>   // 스레드 동기화를 위한 뮤텍스
#include <atomic>  // 스레드 종료 플래그를 위한 아토믹
#include <condition_variable>
#include <queue> 

// --- 설정값 ---
const char* CAN_INTERFACE = "can0";
const int DOIP_PORT = 13400;
const int UDS_REQUEST_CAN_ID = 0x7E0;   // 라즈베리파이 -> TC375
const int UDS_RESPONSE_CAN_ID = 0x7E8; // TC375 -> 라즈베리파이
const uint16_t CLIENT_LOGICAL_ADDRESS = 0x0E00;
const uint16_t SERVER_LOGICAL_ADDRESS = 0x1000;
const uint16_t MAX_CONSECUTIVE_FRAMES_IN_BLOCK = 4095; // BS=0일 때 보낼 최대 프레임 수

// --- DoIP 헤더 구조체 (Big Endian) ---
#pragma pack(push, 1)
struct DoIPHeader {
    uint8_t version;
    uint8_t inverse_version;
    uint16_t payload_type;
    uint32_t payload_length;
};
#pragma pack(pop)

// --- 전역 변수 (스레드 동기화) ---
std::mutex client_socket_mutex;                  // DoIP 소켓 쓰기 동작을 보호하기 위한 뮤텍스
std::mutex fc_queue_mutex;                       // FC 프레임 큐 접근을 보호하기 위한 뮤텍스
std::condition_variable fc_queue_cv;             // FC 큐에 아이템이 생길 때까지 대기하기 위한 Condition Variable
std::queue<std::vector<uint8_t>> fc_frame_queue; // CAN 스레드가 받은 FC 프레임을 메인 스레드에 전달하기 위한 큐


// *** --- 함수 프로토타입 --- ***
int setup_can_socket();
void handle_doip_session(int client_sock);
void can_to_doip_forwarder(int can_sock, int client_sock, std::atomic<bool>& session_active);

// --- ISO-TP 송신 관련 함수 ---
bool isotp_send(int sock, uint32_t can_id, const std::vector<uint8_t>& data);
bool isotp_send_single_frame(int sock, uint32_t can_id, const std::vector<uint8_t>& data); // ✨ 변경점
void isotp_send_first_frame(int sock, uint32_t can_id, const std::vector<uint8_t>& data);
bool isotp_send_multi_frame(int sock, uint32_t can_id, const std::vector<uint8_t>& data);
bool isotp_send_consecutive_frames(int sock, uint32_t can_id, const std::vector<uint8_t>& data, size_t* bytes_sent, uint8_t* seq_num);
bool wait_for_flow_control(can_frame& fc_frame);

// --- ISO-TP 수신 관련 함수 ---
std::vector<uint8_t> isotp_receive(int sock);
std::vector<uint8_t> isotp_handle_single_frame(const can_frame& frame);
std::vector<uint8_t> isotp_handle_multi_frame(int sock, const can_frame& first_frame);
void isotp_send_flow_control(int sock);

// --- 메인 함수 ---
int main() {
    signal(SIGPIPE, SIG_IGN); // Ctrl+C 시그널 무시 (자식 프로세스가 아닌 메인 서버가 종료되도록)

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        std::cerr << "TCP 소켓 생성 실패." << std::endl; return -1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DOIP_PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "TCP 바인딩 실패." << std::endl; close(server_sock); return -1;
    }

    if (listen(server_sock, 5) == -1) {
        std::cerr << "TCP 리슨 실패." << std::endl; close(server_sock); return -1;
    }

    std::cout << "DoIP 게이트웨이 시작. 포트 " << DOIP_PORT << "에서 연결 대기 중..." << std::endl;

    while (true) {
        int client_sock = accept(server_sock, nullptr, nullptr);
        if (client_sock < 0) {
            std::cerr << "클라이언트 연결 수락 실패." << std::endl;
            continue;
        }

        std::cout << "\n진단기 클라이언트 연결됨. 세션 시작." << std::endl;
        handle_doip_session(client_sock); // 연결된 클라이언트 처리
        close(client_sock);
        std::cout << "클라이언트 세션 종료." << std::endl;
    }

    close(server_sock);
    return 0;
}

// --- CAN -> DoIP 데이터 전송 전담 스레드 함수 ---
void can_to_doip_forwarder(int can_sock, int client_sock, std::atomic<bool>& session_active) {
    std::cout << "[CAN->DoIP Thread] 수신 대기 시작." << std::endl;
    while (session_active) {
        can_frame rx_frame;
        int bytes_read = read(can_sock, &rx_frame, sizeof(can_frame));

        if (bytes_read <= 0) {
            if (!session_active) break;
            continue; // 타임아웃은 정상, 계속 시도
        }

        // --- FC 프레임 처리 로직 ---
        // 프레임 타입이 3(FC)이면 무조건 큐에 넣습니다.
        uint8_t pci_type = (rx_frame.data[0] & 0xF0) >> 4;

        if (pci_type == 3) { // Flow Control 프레임인 경우
            std::lock_guard<std::mutex> lock(fc_queue_mutex);
            fc_frame_queue.push(std::vector<uint8_t>(rx_frame.data, rx_frame.data + rx_frame.can_dlc));
            fc_queue_cv.notify_one(); // 기다리는 메인 스레드를 깨움
            continue; // 이 프레임은 처리했으므로 루프 계속
        }

        // 여기서부터는 기존의 isotp_receive 로직을 직접 처리
        std::vector<uint8_t> uds_response;

        switch (pci_type) {
        case 0: // Single Frame
            uds_response = isotp_handle_single_frame(rx_frame);
            break;
        case 1: // First Frame
            uds_response = isotp_handle_multi_frame(can_sock, rx_frame);
            break;
        default:
            std::cerr << "잘못된 시작 프레임 수신 (Type: " << (int)pci_type << ")" << std::endl;
            continue; // 루프 계속
        }

        if (!session_active) break;
        if (uds_response.empty()) {
            continue;
        }

        std::cout << "[CAN -> DoIP] CAN으로부터 UDS 데이터 수신 (" << uds_response.size() << " 바이트)" << std::endl;

        std::vector<uint8_t> final_uds_payload;

        uint8_t first_byte = uds_response[0];

        // UDS 표준 응답 SID는 긍정 응답(0x40~0x7E)과 부정 응답(0x7F)을 포함합니다.
        // 첫 바이트가 이 범위에 속하면, 이미 유효한 UDS 응답이므로 변환하지 않습니다.
        if (first_byte >= 0x40 && first_byte <= 0x7F) {
            // 원본 UDS 응답을 그대로 사용
            final_uds_payload = uds_response;
        }
        else {
            // 표준 응답 SID가 아닌 경우 (예: DID로 시작하는 주기적 데이터)
            // 진단 라이브러리가 인식할 수 있도록 0x62(RDBI 긍정 응답)를 붙여줍니다.
            std::cout << "[Gateway] 비표준 응답을 정식 UDS 응답(0x62)으로 변환합니다." << std::endl;
            final_uds_payload.resize(1 + uds_response.size());
            final_uds_payload[0] = 0x62; // ReadDataByIdentifier Positive Response
            memcpy(final_uds_payload.data() + 1, uds_response.data(), uds_response.size());
        }

        size_t doip_payload_size = 4 + final_uds_payload.size();
        std::vector<uint8_t> resp_msg(sizeof(DoIPHeader) + doip_payload_size);
        DoIPHeader* resp_header = (DoIPHeader*)resp_msg.data();
        resp_header->version = 2; resp_header->inverse_version = ~2;
        resp_header->payload_type = htons(0x8001);
        resp_header->payload_length = htonl(doip_payload_size);
        uint16_t* sa = (uint16_t*)(resp_msg.data() + sizeof(DoIPHeader));
        uint16_t* ta = (uint16_t*)(resp_msg.data() + sizeof(DoIPHeader) + 2);
        *sa = htons(SERVER_LOGICAL_ADDRESS);
        *ta = htons(CLIENT_LOGICAL_ADDRESS);
        memcpy(resp_msg.data() + sizeof(DoIPHeader) + 4, final_uds_payload.data(), final_uds_payload.size());

        {
            std::lock_guard<std::mutex> lock(client_socket_mutex);
            if (write(client_sock, resp_msg.data(), resp_msg.size()) <= 0) {
                std::cerr << "[CAN->DoIP Thread] 클라이언트 소켓 쓰기 실패. 스레드 종료." << std::endl;
                session_active = false;
            }
        }
    }
    std::cout << "[CAN->DoIP Thread] 스레드 종료." << std::endl;
}

// --- 메인 세션 처리 함수 (DoIP -> CAN 전송만 담당) ---
void handle_doip_session(int client_sock) {
    int can_sock = setup_can_socket();
    if (can_sock < 0) {
        std::cerr << "CAN 소켓 설정 실패." << std::endl;
        return;
    }
    std::cout << "CAN 소켓이 성공적으로 설정되었습니다." << std::endl;

    std::atomic<bool> session_active(true);

    // CAN -> DoIP 수신 전용 스레드 시작
    std::thread can_reader(can_to_doip_forwarder, can_sock, client_sock, std::ref(session_active));

    while (session_active) {
        // 1. DoIP 헤더(고정 8바이트) 먼저 읽기
        DoIPHeader header;
        int bytes_read = read(client_sock, &header, sizeof(DoIPHeader));
        if (bytes_read <= 0) {
            session_active = false; // 클라이언트 연결 끊김
            break;
        }

        if (bytes_read != sizeof(DoIPHeader)) {
            std::cerr << "DoIP 헤더를 완전히 수신하지 못했습니다." << std::endl;
            continue;
        }

        // 2. 페이로드 길이 파싱 (Network to Host byte order)
        uint32_t payload_length = ntohl(header.payload_length);
        uint16_t payload_type = ntohs(header.payload_type);

        // 3. 페이로드 길이만큼 정확히 데이터 수신
        std::vector<uint8_t> payload_buffer(payload_length);
        size_t total_received = 0;
        while (total_received < payload_length) {
            bytes_read = read(client_sock, payload_buffer.data() + total_received, payload_length - total_received);
            if (bytes_read <= 0) {
                session_active = false;
                break; // 수신 중 오류 또는 연결 끊김
            }
            total_received += bytes_read;
        }

        if (!session_active || total_received != payload_length) {
            std::cerr << "페이로드를 완전히 수신하지 못했습니다. (기대: " << payload_length << ", 실제: " << total_received << ")" << std::endl;
            break;
        }


        if (payload_type == 0x0005) { // 라우팅 활성화 요청
            std::cout << "라우팅 활성화 요청 수신. 긍정 응답 전송." << std::endl;
            std::vector<uint8_t> resp(sizeof(DoIPHeader) + 9);
            DoIPHeader* resp_header = (DoIPHeader*)resp.data();
            resp_header->version = 2;
            resp_header->inverse_version = ~2;
            resp_header->payload_type = htons(0x0006); // 긍정 응답 타입
            resp_header->payload_length = htonl(9);    // 페이로드 길이 9

            // 페이로드 구성
            uint16_t* client_addr_ptr = (uint16_t*)(resp.data() + sizeof(DoIPHeader));
            uint16_t* server_addr_ptr = (uint16_t*)(resp.data() + sizeof(DoIPHeader) + 2);
            uint8_t* response_code_ptr = (uint8_t*)(resp.data() + sizeof(DoIPHeader) + 4);

            *client_addr_ptr = htons(CLIENT_LOGICAL_ADDRESS); // 클라이언트 주소
            *server_addr_ptr = htons(SERVER_LOGICAL_ADDRESS); // 서버(ECU) 주소
            *response_code_ptr = 0x10; // 0x10: 성공적으로 활성화됨

            // 응답 전송
            {
                std::lock_guard<std::mutex> lock(client_socket_mutex);
                write(client_sock, resp.data(), resp.size());
            }
            continue;
        }

        if (payload_type != 0x8001) continue;

        // DoIP ACK(0x8002) 전송
        std::vector<uint8_t> ack_msg(sizeof(DoIPHeader) + 5);
        DoIPHeader* ack_header = (DoIPHeader*)ack_msg.data();
        ack_header->version = 2; ack_header->inverse_version = ~2;
        ack_header->payload_type = htons(0x8002);
        ack_header->payload_length = htonl(5);
        // ACK 페이로드 구성
        uint16_t* ack_sa = (uint16_t*)(ack_msg.data() + sizeof(DoIPHeader));
        uint16_t* ack_ta = (uint16_t*)(ack_msg.data() + sizeof(DoIPHeader) + 2);
        uint8_t* ack_code = (uint8_t*)(ack_msg.data() + sizeof(DoIPHeader) + 4);
        *ack_sa = htons(SERVER_LOGICAL_ADDRESS); // 요청을 받은 주체 (ECU)
        *ack_ta = htons(CLIENT_LOGICAL_ADDRESS); // 요청을 보낸 주체 (진단기)
        *ack_code = 0x00;                        // 0x00: Positive ACK (정상 수신)

        {
            std::lock_guard<std::mutex> lock(client_socket_mutex);
            write(client_sock, ack_msg.data(), ack_msg.size());
        }
        std::cout << "\nDoIP: Positive ACK (0x8002) 전송 완료" << std::endl; // 확인용 로그


        // DoIP 페이로드에서 SA, TA를 제외한 순수 UDS 데이터 추출
        std::vector<uint8_t> uds_request(payload_buffer.begin() + 4, payload_buffer.end());
        std::cout << "\n[DoIP -> CAN] DoIP로부터 UDS 요청 수신 (" << uds_request.size() << " 바이트)" << std::endl;

        isotp_send(can_sock, UDS_REQUEST_CAN_ID, uds_request);
    }

    session_active = false;
    fc_queue_cv.notify_all(); // CV 변수명 변경
    if (can_reader.joinable()) {
        can_reader.join();
    }
    close(can_sock);
}

// --- CAN 소켓 설정 함수 ---
int setup_can_socket() {
    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) return -1;
    ifreq ifr;
    strcpy(ifr.ifr_name, CAN_INTERFACE);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) return -1;
    sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    can_filter rfilter[1];
    rfilter[0].can_id = UDS_RESPONSE_CAN_ID;
    rfilter[0].can_mask = CAN_SFF_MASK;
    setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
    struct timeval tv;
    tv.tv_sec = 2; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    return sock;
}

// // --- ISO-TP 송신 메인 함수 ---
bool isotp_send(int sock, uint32_t can_id, const std::vector<uint8_t>& data) {
    if (data.size() <= 7) {
        return isotp_send_single_frame(sock, can_id, data);
    }
    else {
        return isotp_send_multi_frame(sock, can_id, data);
    }
}

bool isotp_send_single_frame(int sock, uint32_t can_id, const std::vector<uint8_t>& data) {
    can_frame frame;
    frame.can_id = can_id;
    frame.can_dlc = data.size() + 1;
    frame.data[0] = 0x00 | data.size();
    memcpy(&frame.data[1], data.data(), data.size());

    //write(sock, &frame, sizeof(frame));
    if (write(sock, &frame, sizeof(frame)) <= 0) {
        std::cerr << "Single Frame CAN 소켓 쓰기 실패." << std::endl;
        return false;
    }
    std::cout << "  -> CAN: Single Frame 전송" << std::endl;
    return true;
}

bool isotp_send_multi_frame(int sock, uint32_t can_id, const std::vector<uint8_t>& data) {
    isotp_send_first_frame(sock, can_id, data);

    size_t bytes_sent = 6;
    uint8_t seq_num = 1;

    while (bytes_sent < data.size()) {
        if (!isotp_send_consecutive_frames(sock, can_id, data, &bytes_sent, &seq_num)) {
            std::cerr << "Consecutive frame 전송 실패. 중단." << std::endl;
            return false;
        }
    }

    std::cout << "  -> CAN: 모든 Multi-frame 데이터 전송 완료." << std::endl;
    return true;
}

void isotp_send_first_frame(int sock, uint32_t can_id, const std::vector<uint8_t>& data) {
    can_frame frame;
    frame.can_id = can_id;
    frame.can_dlc = 8;
    frame.data[0] = 0x10 | ((data.size() >> 8) & 0x0F);
    frame.data[1] = data.size() & 0xFF;
    memcpy(&frame.data[2], data.data(), 6);
    write(sock, &frame, sizeof(frame));
    std::cout << "  -> CAN: First Frame 전송." << std::endl;
}

bool isotp_send_consecutive_frames(int sock, uint32_t can_id, const std::vector<uint8_t>& data, size_t* bytes_sent, uint8_t* seq_num) {
    can_frame fc_frame;

    std::cout << "  -> FC 대기 중..." << std::endl;

    // 1. CAN 리더 스레드가 전달해 줄 FC 프레임을 기다립니다.
    if (!wait_for_flow_control(fc_frame)) {
        return false; // 타임아웃 또는 오류
    }

    // 2. 수신된 FC 프레임의 파라미터를 분석합니다.
    uint8_t fs = fc_frame.data[0] & 0x0F;
    uint8_t bs = fc_frame.data[1];
    uint8_t stmin_raw = fc_frame.data[2];

    // 3. Flow Status에 따라 동작을 결정합니다.
    if (fs == 1) { // Wait
        std::cout << "  <- CAN: FC(Wait) 수신. 잠시 후 재시도합니다." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 잠시 대기
        return true; // true를 반환하여 바깥 루프가 재시도하도록 함
    }
    if (fs > 1) { // Overflow 또는 예약된 값
        std::cerr << "  <- CAN: FC(Overflow) 수신. 전송을 중단합니다." << std::endl;
        return false; // false를 반환하여 전송 완전 중단
    }

    std::cout << "  <- CAN: FC(Continue) 수신. CF 블록 전송을 시작합니다." << std::endl;

    // 4. STmin(프레임 간 최소 시간)을 계산합니다.
    long stmin_us = 0;
    if (stmin_raw <= 0x7F) {
        stmin_us = stmin_raw * 1000;
    }
    else if (stmin_raw >= 0xF1 && stmin_raw <= 0xF9) {
        stmin_us = (stmin_raw - 0xF0) * 100;
    }

    // 5. 이번 블록에서 보낼 프레임 수를 결정하고 전송합니다.
    uint16_t frames_to_send_in_block = (bs == 0) ? MAX_CONSECUTIVE_FRAMES_IN_BLOCK : bs;
    uint16_t frames_sent_in_block = 0;

    while (frames_sent_in_block < frames_to_send_in_block && *bytes_sent < data.size()) {
        can_frame cf_frame;
        cf_frame.can_id = can_id;
        cf_frame.data[0] = 0x20 | (*seq_num & 0x0F);

        size_t bytes_to_send_in_frame = std::min((size_t)7, data.size() - *bytes_sent);
        memcpy(&cf_frame.data[1], data.data() + *bytes_sent, bytes_to_send_in_frame);
        cf_frame.can_dlc = bytes_to_send_in_frame + 1;

        if (write(sock, &cf_frame, sizeof(can_frame)) <= 0) {
            std::cerr << "CF 프레임 CAN 소켓 쓰기 실패." << std::endl;
            return false;
        }

        *bytes_sent += bytes_to_send_in_frame;
        *seq_num = (*seq_num + 1) % 16;
        frames_sent_in_block++;

        // 마지막 프레임이 아니라면 STmin 만큼 대기합니다.
        if (*bytes_sent < data.size()) {
            std::this_thread::sleep_for(std::chrono::microseconds(stmin_us));
        }
    }

    return true; // 블록 전송 성공
}

bool wait_for_flow_control(can_frame& fc_frame_out) {
    std::vector<uint8_t> local_fc_frame_data;
    {
        // --- FC 대기 로직 변경 ---
        // condition variable과 predicate을 확인하는 대신, queue가 비어있지 않을 때까지 기다립니다.
        std::unique_lock<std::mutex> lock(fc_queue_mutex);
        if (!fc_queue_cv.wait_for(lock, std::chrono::seconds(2), [] { return !fc_frame_queue.empty(); })) {
            std::cerr << "FC 수신 타임아웃!" << std::endl;
            return false;
        }
        local_fc_frame_data = fc_frame_queue.front();
        fc_frame_queue.pop();
    }

    if (local_fc_frame_data.empty() || (local_fc_frame_data[0] & 0xF0) != 0x30) {
        std::cerr << "FC 큐에서 잘못된 데이터 수신." << std::endl;
        return false;
    }

    // 유효한 FC 프레임 데이터를 can_frame 구조체로 복사
    memset(&fc_frame_out, 0, sizeof(can_frame));
    fc_frame_out.can_dlc = local_fc_frame_data.size();
    memcpy(fc_frame_out.data, local_fc_frame_data.data(), local_fc_frame_data.size());

    return true;
}





// --- ISO-TP 수신 메인 함수 ---
std::vector<uint8_t> isotp_receive(int sock) {
    can_frame rx_frame;
    if (read(sock, &rx_frame, sizeof(can_frame)) <= 0) {
        return {}; // 타임아웃 또는 읽기 실패
    }

    uint8_t pci_type = (rx_frame.data[0] & 0xF0) >> 4;
    switch (pci_type) {
    case 0: // Single Frame
        return isotp_handle_single_frame(rx_frame);
    case 1: // First Frame
        return isotp_handle_multi_frame(sock, rx_frame);
    default: // Consecutive 또는 FlowControl 등, 첫 프레임으로 올 수 없음
        std::cerr << "잘못된 시작 프레임 수신 (Type: " << (int)pci_type << ")" << std::endl;
        return {};
    }
}

std::vector<uint8_t> isotp_handle_single_frame(const can_frame& frame) {
    std::cout << "  <- CAN: Single Frame 수신" << std::endl;
    uint8_t len = frame.data[0] & 0x0F;
    if (len == 0 || len > 7) return {}; // 잘못된 길이
    return std::vector<uint8_t>(&frame.data[1], &frame.data[1] + len);
}

std::vector<uint8_t> isotp_handle_multi_frame(int sock, const can_frame& first_frame) {
    std::cout << "  <- CAN: First Frame 수신" << std::endl;
    uint16_t total_size = ((first_frame.data[0] & 0x0F) << 8) | first_frame.data[1];

    std::vector<uint8_t> uds_rx_buffer;
    uds_rx_buffer.reserve(total_size);
    uds_rx_buffer.assign(&first_frame.data[2], &first_frame.data[first_frame.can_dlc]);

    isotp_send_flow_control(sock);

    uint8_t expected_seq_num = 1;
    while (uds_rx_buffer.size() < total_size) {
        can_frame cf_frame;
        if (read(sock, &cf_frame, sizeof(can_frame)) <= 0) {
            std::cerr << "CF 수신 중 타임아웃" << std::endl;
            return {}; // 타임아웃
        }

        if ((cf_frame.data[0] & 0xF0) != 0x20 || (cf_frame.data[0] & 0x0F) != expected_seq_num) {
            std::cerr << "잘못된 순서의 CF 또는 예상치 못한 프레임 수신" << std::endl;
            return {}; // 오류
        }

        uint16_t bytes_to_copy = std::min((uint16_t)(total_size - uds_rx_buffer.size()), (uint16_t)(cf_frame.can_dlc - 1));
        uds_rx_buffer.insert(uds_rx_buffer.end(), &cf_frame.data[1], &cf_frame.data[1] + bytes_to_copy);
        expected_seq_num = (expected_seq_num + 1) % 16;
    }

    std::cout << "  <- CAN: 모든 Consecutive Frame 수신 완료" << std::endl;
    return uds_rx_buffer;
}

void isotp_send_flow_control(int sock) {
    can_frame fc_frame;
    fc_frame.can_id = UDS_REQUEST_CAN_ID; // 응답은 요청 ID로 보냄
    fc_frame.can_dlc = 8;
    memset(fc_frame.data, 0, sizeof(fc_frame.data));

    fc_frame.data[0] = 0x30; // [FlowStatus: CTS]
    fc_frame.data[1] = 0x00; // [BlockSize: 0 (Send All)]
    fc_frame.data[2] = 0x0A; // [STmin: 10ms]

    write(sock, &fc_frame, sizeof(can_frame));
    std::cout << "  -> CAN: Flow Control(CTS) 전송" << std::endl;
}