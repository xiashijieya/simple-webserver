#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

const char* k_request =
    "GET /hello HTTP/1.1\r\n"
    "Host: bench\r\n"
    "\r\n";

const char* k_close_request =
    "GET /hello HTTP/1.1\r\n"
    "Host: bench\r\n"
    "Connection: close\r\n"
    "\r\n";

struct Config {
    std::string host;
    int port;
    int threads;
    int requests_per_thread;
    bool new_connection;
};

struct Stats {
    std::vector<double> latencies_ms;
    int errors;
};

uint64_t now_ticks() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}

double ticks_to_ms(uint64_t begin, uint64_t end, double ticks_per_sec) {
    return static_cast<double>(end - begin) * 1000.0 / ticks_per_sec;
}

SOCKET connect_to(const Config& config) {
    SOCKET fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(config.port));
    if (inet_pton(AF_INET, config.host.c_str(), &addr.sin_addr) != 1) {
        closesocket(fd);
        return INVALID_SOCKET;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
        == SOCKET_ERROR) {
        closesocket(fd);
        return INVALID_SOCKET;
    }
    return fd;
}

bool send_all(SOCKET fd, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(fd, data + sent, len - sent, 0);
        if (n == SOCKET_ERROR) return false;
        sent += n;
    }
    return true;
}

size_t response_size(const std::string& data) {
    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) return 0;

    size_t marker = data.find("Content-Length:");
    if (marker == std::string::npos || marker > header_end) {
        marker = data.find("content-length:");
    }
    if (marker == std::string::npos || marker > header_end) return 0;

    size_t body_length = 0;
    size_t pos = marker + 15;
    while (pos < data.size() && data[pos] == ' ') ++pos;
    while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
        body_length = body_length * 10 + static_cast<size_t>(data[pos] - '0');
        ++pos;
    }

    size_t total = header_end + 4 + body_length;
    return data.size() >= total ? total : 0;
}

enum ReadResult {
    read_ok,
    read_closed,
    read_error
};

bool peer_reset() {
    int err = WSAGetLastError();
    return err == WSAECONNRESET || err == WSAECONNABORTED
           || err == WSAESHUTDOWN || err == WSAETIMEDOUT;
}

ReadResult read_one_response(SOCKET fd, std::string* leftover) {
    while (response_size(*leftover) == 0) {
        char buf[4096];
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            leftover->append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) return read_closed;
        return peer_reset() ? read_closed : read_error;
    }
    leftover->erase(0, response_size(*leftover));
    return read_ok;
}

ReadResult one_request(const Config& config, SOCKET fd, std::string* leftover) {
    const char* request = config.new_connection ? k_close_request : k_request;
    if (!send_all(fd, request, static_cast<int>(std::strlen(request)))) {
        return peer_reset() ? read_closed : read_error;
    }
    return read_one_response(fd, leftover);
}

void worker(const Config& config, Stats* stats, uint64_t* elapsed_ticks) {
    uint64_t begin = now_ticks();
    std::string leftover;
    SOCKET persistent = INVALID_SOCKET;

    if (!config.new_connection) {
        persistent = connect_to(config);
        if (persistent == INVALID_SOCKET) {
            stats->errors += config.requests_per_thread;
            *elapsed_ticks = 0;
            return;
        }
    }

    for (int i = 0; i < config.requests_per_thread; ++i) {
        ReadResult result = read_error;
        uint64_t req_begin = 0;
        uint64_t req_end = 0;

        while (true) {
            SOCKET fd = persistent;
            if (config.new_connection) {
                fd = connect_to(config);
                if (fd == INVALID_SOCKET) {
                    result = read_error;
                    break;
                }
            }
            if (fd == INVALID_SOCKET) {
                result = read_error;
                break;
            }

            req_begin = now_ticks();
            result = one_request(config, fd, &leftover);
            req_end = now_ticks();

            if (result == read_ok && config.new_connection) closesocket(fd);
            if (result != read_closed) break;

            if (!config.new_connection) {
                closesocket(persistent);
                leftover.clear();
                persistent = connect_to(config);
            } else {
                closesocket(fd);
                leftover.clear();
            }
        }

        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        stats->latencies_ms.push_back(
            ticks_to_ms(req_begin, req_end,
                        static_cast<double>(freq.QuadPart)));
        if (result != read_ok) ++stats->errors;
    }

    if (!config.new_connection && persistent != INVALID_SOCKET) {
        closesocket(persistent);
    }
    *elapsed_ticks = now_ticks() - begin;
}

double percentile(std::vector<double>* values, double percent) {
    if (values->empty()) return 0.0;
    size_t index = static_cast<size_t>(percent * static_cast<double>(values->size()) / 100.0);
    if (index >= values->size()) index = values->size() - 1;
    return (*values)[index];
}

void print_usage() {
    std::printf("usage: bench host port threads requests_per_thread [newconn]\n");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        print_usage();
        return 1;
    }

    Config config;
    config.host = argv[1];
    config.port = std::atoi(argv[2]);
    config.threads = std::atoi(argv[3]);
    config.requests_per_thread = std::atoi(argv[4]);
    config.new_connection = argc >= 6 && std::strcmp(argv[5], "newconn") == 0;

    if (config.port <= 0 || config.threads <= 0 || config.requests_per_thread <= 0) {
        print_usage();
        return 1;
    }

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::printf("WSAStartup failed\n");
        return 1;
    }

    std::vector<Stats> stats(config.threads);
    std::vector<uint64_t> elapsed_ticks(config.threads, 0);
    std::vector<std::thread> workers;

    uint64_t wall_begin = now_ticks();
    for (int i = 0; i < config.threads; ++i) {
        workers.push_back(std::thread(worker, std::cref(config),
                                      &stats[i], &elapsed_ticks[i]));
    }
    for (int i = 0; i < config.threads; ++i) workers[i].join();
    uint64_t wall_end = now_ticks();

    WSACleanup();

    std::vector<double> all_latencies;
    int total_errors = 0;
    uint64_t max_ticks = 0;
    int total_requests = 0;
    for (int i = 0; i < config.threads; ++i) {
        all_latencies.insert(all_latencies.end(),
                             stats[i].latencies_ms.begin(),
                             stats[i].latencies_ms.end());
        total_errors += stats[i].errors;
        if (elapsed_ticks[i] > max_ticks) max_ticks = elapsed_ticks[i];
        total_requests += config.requests_per_thread;
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    double wall_seconds = static_cast<double>(wall_end - wall_begin)
                          / static_cast<double>(freq.QuadPart);

    double sum = 0.0;
    for (size_t i = 0; i < all_latencies.size(); ++i) sum += all_latencies[i];
    double average = all_latencies.empty() ? 0.0 : sum / all_latencies.size();

    std::printf("\n========== bench result ==========\n");
    std::printf("target          %s:%d  (%s)\n", config.host.c_str(), config.port,
                config.new_connection ? "new connection per request" : "keep alive");
    std::printf("concurrency     %d threads\n", config.threads);
    std::printf("requests        %d total, %d per thread\n",
                total_requests, config.requests_per_thread);
    std::printf("errors          %d\n", total_errors);
    std::printf("elapsed         %.3f s\n", wall_seconds);
    std::printf("qps             %.0f\n", total_requests / wall_seconds);
    std::printf("latency ms      avg %.3f  p50 %.3f  p95 %.3f  p99 %.3f\n",
                average,
                percentile(&all_latencies, 50),
                percentile(&all_latencies, 95),
                percentile(&all_latencies, 99));
    std::printf("==================================\n");

    return total_errors == 0 ? 0 : 1;
}
