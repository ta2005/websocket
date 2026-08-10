#include "client.hpp"
#include <format>
#include <iostream>
#include <string>

void run_test_case(int case_num, const std::string &host,
                   const std::string &port) {
    std::string path =
        std::format("/runCase?case={}&agent=MyCppClient", case_num);

    auto client_res = ws::Client::create(host, path, port);
    if (!client_res)
        return;

    auto &client = *client_res;

    // Autobahn Echo Loop
    while (true) {
        auto chunk_res = client.read_chunk();
        if (!chunk_res) {
            // Connection closed or protocol error encountered
            break;
        }

        auto &chunk = *chunk_res;
        if (chunk.type == ws::opcode::close) {
            break;
        }

        // Autobahn expects the client to echo back Text or Binary frames
        if (chunk.type == ws::opcode::text) {
            std::string_view text(
                reinterpret_cast<const char *>(chunk.payload.data()),
                chunk.payload.size());
            if (!client.send(text))
                break;
        } else if (chunk.type == ws::opcode::binary) {
            if (!client.send(chunk.payload))
                break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 1;
    }
    const std::string host = "127.0.0.1";
    const std::string port = "9001";

    // 1. Get total case count from Autobahn
    int total_cases = std::stoi(argv[1]);
    // {
    //     auto client = ws::Client::create(host, "/getCaseCount", port);
    //     if (client) {
    //         auto chunk = client->read_chunk();
    //  // chunk->payload
    //  //
    //         if (chunk) {
    //             std::string count_str(
    //                 reinterpret_cast<const char *>(chunk->payload.data()),
    //                 chunk->payload.size());
    //             total_cases = std::stoi(count_str);
    //         }
    //     }
    // }

    if (total_cases == 0) {
        std::cerr << "Failed to fetch test case count from Autobahn server at "
                  << host << ":" << port << std::endl;
        // return 1;
    }

    std::cout << "Starting Autobahn test suite (" << total_cases << " cases)..."
              << std::endl;

    // 2. Loop through all test cases
    for (int i = 1; i <= total_cases; ++i) {
        std::cout << "Running case " << i << "/" << total_cases << "...\r"
                  << std::flush;
        run_test_case(i, host, port);
    }

    std::cout << "\nTest cases finished. Generating HTML report..."
              << std::endl;

    // 3. Trigger report generation
    {
        auto client =
            ws::Client::create(host, "/updateReports?agent=MyCppClient", port);
        if (client) {
            if (!client->read_chunk()) {
                return 1;
            }
        }
    }

    std::cout
        << "Done! Open autobahn/reports/clients/index.html to view results."
        << std::endl;
    return 0;
}
