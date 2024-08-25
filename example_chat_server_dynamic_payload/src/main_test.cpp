//
// Created by maxim on 24.08.2024.
//
#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include "OptimizedDynamicPayload.h"
#include "DynamicPayload.h"

class ChatMessage : public NetworkMessages::BinaryData {
public:
    std::string username;
    std::string message;

    ChatMessage() = default;
    ChatMessage(std::string username, std::string message)
            : username(std::move(username)), message(std::move(message)) {}

    [[nodiscard]] ByteVector serialize() const override {
        ByteVector data;
        append_bytes(data, username);
        append_bytes(data, message);
        return data;
    }

    void deserialize(const ByteVector& data) override {
        size_t offset = 0;
        username = read_bytes<std::string>(data, offset);
        message = read_bytes<std::string>(data, offset);
    }

};



// Function to generate a random string
std::string generate_random_string(int length) {
    const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.length() - 1);
    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    return result;
}

// Function to measure execution time
template<typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}
double test_normal_payload(int num_messages) {
    double serialize_time = 0.0;
    double deserialize_time = 0.0;
    std::vector<NetworkMessages::BinaryData::ByteVector> serialized_messages;
    serialized_messages.reserve(num_messages);
    for (int i = 0; i < num_messages; ++i) {
        NetworkMessages::BinaryMessage<ChatMessage> message(0, ChatMessage());
        auto& payload = message.getPayload();
        std::string username = generate_random_string(10);
        std::string random_message = "Hello World!";
        payload.username = username;
        payload.message = random_message;

        serialize_time += measure_time([&]() {
            serialized_messages.push_back(message.serialize());
        });
    }

    for (const auto& serialized : serialized_messages) {
        NetworkMessages::BinaryMessage<ChatMessage> message(0, ChatMessage());
        deserialize_time += measure_time([&]() {
            message.deserialize(serialized);

            std::cout<< message.getPayload().message << std::endl;
        });
    }

    double total_time = serialize_time + deserialize_time;
    std::cout << "ChatMessage:\n";
    std::cout << "  Serialize time: " << serialize_time << " ms\n";
    std::cout << "  Deserialize time: " << deserialize_time << " ms\n";
    std::cout << "  Total time: " << total_time << " ms\n";
    return total_time;
}

// Test function for OptimizedDynamicPayload
double test_optimized_dynamic_payload(int num_messages) {
    double serialize_time = 0.0;
    double deserialize_time = 0.0;
    std::vector<NetworkMessages::BinaryData::ByteVector> serialized_messages;

    for (int i = 0; i < num_messages; ++i) {


        serialize_time += measure_time([&]() {
            serialized_messages.push_back(message->serialize());
        });
    }

    for (const auto& serialized : serialized_messages) {
        auto message = MessageFactory::createMessage("ChatMessage");
        deserialize_time += measure_time([&]() {
            message->deserialize(serialized);
        });
    }

    double total_time = serialize_time + deserialize_time;
    std::cout << "DynamicPayloadOptimized:\n";
    std::cout << "  Serialize time: " << serialize_time << " ms\n";
    std::cout << "  Deserialize time: " << deserialize_time << " ms\n";
    std::cout << "  Total time: " << total_time << " ms\n";
    return total_time;
}

int main() {
    MessageFactory::loadDefinitions("chat_messages.json");

    auto message = MessageFactory::createMessage("ChatMessage");
    auto& payload = message->getPayload();
    std::string username = "MadWizard";
    std::string chat_message = "Hello World!";
    payload.set("username", username);
    payload.set("message", chat_message);

    message->serialize();
    const int num_messages = 1000;  // Number of messages per test
    const int num_runs = 100;  // Number of times to run each test

    std::cout << "Testing with " << num_messages << " messages, " << num_runs << " runs each\n\n";

    // Warm-up run (not timed)
    test_normal_payload(1000);
    test_optimized_dynamic_payload(1000);

    std::vector<double> normal_times, optimized_times;

    for (int i = 0; i < num_runs; ++i) {
        std::cout << "Run " << (i + 1) << ":\n";

        normal_times.push_back(test_normal_payload(num_messages));
        optimized_times.push_back(test_optimized_dynamic_payload(num_messages));

        std::cout << "\n";
    }

    // Calculate and print averages
    auto calculate_average = [](const std::vector<double>& times) {
        return std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    };

    std::cout << "Average times:\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "ChatMessage: " << calculate_average(normal_times) << " ms\n";
    //std::cout << "DynamicPayload: " << calculate_average(dynamic_times) << " ms\n";
    std::cout << "OptimizedDynamicPayload: " << calculate_average(optimized_times) << " ms\n";


    double chat_message_avg = calculate_average(normal_times);
    //double dynamic_payload_avg = calculate_average(dynamic_times);
    double optimized_dynamic_payload_avg = calculate_average(optimized_times);

    std::cout << "\nPerformance Summary:\n";
    std::cout << "ChatMessage (baseline): 100%\n";
    //std::cout << "DynamicPayload: " << (dynamic_payload_avg / chat_message_avg * 100) << "%\n";
    std::cout << "OptimizedDynamicPayload: " << (optimized_dynamic_payload_avg / chat_message_avg * 100) << "%\n";

    //std::cout << "OptimizedDynamicPayload: " << (optimized_dynamic_payload_avg / dynamic_payload_avg * 100) << "%\n";
    return 0;
}