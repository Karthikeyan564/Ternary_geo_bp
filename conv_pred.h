#include <iostream>
#include <vector>
#include <unordered_map>
#include <array>
#include <numeric>
#include <algorithm>
#include <cstdint>
#include <stdlib.h>
#include <cassert>


#define MAX_HISTORY_LENGTH 1024
#define COUNTER_MAX 15 // 2^COUNTER_BITS - 1
#define THRESHOLD 0
#define N_TABLES 6

std::array<int, N_TABLES> history_lengths = {256, 128, 64, 32, 16, 8};

uint64_t current_count = 0;

struct conv_hist
{
    std::array<bool, MAX_HISTORY_LENGTH> ghr;
    uint64_t id;

    conv_hist() {
        this->id = 0;
        ghr.fill(false);
    }
};

std::unordered_map<uint64_t, int> bank_id;

class conv_pred {
    conv_hist active_hist;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::pair<std::vector<int>, int>>> weights[N_TABLES];
    std::unordered_map<uint64_t, conv_hist> pred_time_histories;

public:
    void setup(){
        // Optional setup code can go here
    }

    void terminate(){
        // Optional clean up code can go here
    }

    // Function to print the size of weights

    void predictorsize(){
        int STORAGESIZE = 0;
        for (size_t i = 0; i < N_TABLES; ++i) {
            for (auto& [pc, weight_vec] : weights[i]) {
                STORAGESIZE += weight_vec.size() * (history_lengths[i]*2 + 3);
            }
        }
        std::cout << "Storage size: " << STORAGESIZE << std::endl;
    }

    uint64_t get_unique_inst_id(uint64_t seq_no, uint8_t piece) const
    {
        assert(piece < 16);
        return (seq_no << 4) | (piece & 0x000F);
    }

    bool predict(uint64_t seq_no, uint8_t piece, uint64_t PC)
    {
        // predictorsize();
        const auto pred_hist_key = get_unique_inst_id(seq_no, piece);
        pred_time_histories.emplace(pred_hist_key, active_hist);
        auto& pred_time_history = pred_time_histories.at(pred_hist_key);
        const bool pred_taken = predict_using_given_hist(seq_no, piece, PC, pred_time_history);
        return pred_taken;
    }

    bool predict_using_given_hist(uint64_t seq_no, uint8_t piece, uint64_t pc, conv_hist& hist_to_use) {
        int max_similarity = THRESHOLD - 1;
        bool best_prediction = false; // Initialize best_prediction
        for (int bank = 0; bank < history_lengths.size(); bank++) {
            if (weights[bank].find(pc) != weights[bank].end()) {
                for (const auto& [id, weight_data] : weights[bank][pc]) {
                    const auto& [weight_vec, counter] = weight_data;
                    int dot_product = 0;
                    for (size_t i = 0; i < weight_vec.size(); ++i) {
                        if (weight_vec[i] != 0) {
                            if (weight_vec[i] == 1 && hist_to_use.ghr[i] == 1)
                            dot_product += 1;
                            else if (weight_vec[i] == -1 && hist_to_use.ghr[i] == 0)
                            dot_product += 1;
                            else
                            dot_product -= 1;
                        }
                    }
                    if (dot_product >= THRESHOLD && dot_product > max_similarity) {
                        max_similarity = dot_product;
                        hist_to_use.id = id;
                        bank_id[id] = bank;
                        best_prediction = (counter >= COUNTER_MAX / 2);
                    }
                }
            }
        }
        // std::cout << "max_similarity: " << max_similarity << std::endl;
        if (max_similarity >= THRESHOLD) {
            return best_prediction;
        }


        weights[0][pc][current_count] = {initialize_weight(0), COUNTER_MAX / 2};
        bank_id[current_count] = 0;
        hist_to_use.id = current_count;
        current_count++;
        return false;
    }

    void update(uint64_t seq_no, uint8_t piece, uint64_t PC, bool resolveDir, bool predDir, uint64_t nextPC)
    {
        const auto pred_hist_key = get_unique_inst_id(seq_no, piece);
        auto& pred_time_history = pred_time_histories.at(pred_hist_key);
        update_hist(seq_no, piece, PC, resolveDir, predDir, nextPC, pred_time_history);
        pred_time_histories.erase(pred_hist_key);
    }

    void update_hist(uint64_t seq_no, uint8_t piece, uint64_t PC, bool resolveDir, bool pred_taken, uint64_t nextPC, const conv_hist& hist_to_use) {
        auto& [weight_vec, counter] = weights[bank_id[hist_to_use.id]][PC][hist_to_use.id];
        int count_zeros = 0;
        if (resolveDir == pred_taken) {
            for (int i = 0; i < history_lengths[bank_id[hist_to_use.id]]; ++i) {
                if ((weight_vec[i] == -1 && hist_to_use.ghr[i] == 1) || (weight_vec[i] == 1 && hist_to_use.ghr[i] == 0)) {
                    weight_vec[i] = 0;
                    if (i > history_lengths[bank_id[hist_to_use.id]] / 2) {
                        count_zeros++;
                    }
                }
            }
        }
        counter = (resolveDir == 1) ? std::min(counter + 1, COUNTER_MAX) : std::max(counter - 1, 0);
        
        if (bank_id[hist_to_use.id] < N_TABLES - 1 && count_zeros > (history_lengths[bank_id[hist_to_use.id]] * 3) / 8) {
            auto mid = weight_vec.begin() + weight_vec.size() / 2;
            std::vector<int> first_half(weight_vec.begin(), weight_vec.begin() + history_lengths[bank_id[hist_to_use.id] + 1]);
            int count = counter;
            weights[bank_id[hist_to_use.id] + 1][PC][hist_to_use.id] = {first_half, count};
            weights[bank_id[hist_to_use.id]][PC].erase(hist_to_use.id);
            if (weights[bank_id[hist_to_use.id]][PC].empty()) {
                weights[bank_id[hist_to_use.id]].erase(PC);
            }
            bank_id[hist_to_use.id] += 1;
        }
    }

    void history_update(uint64_t seq_no, uint8_t piece, uint64_t PC, bool taken, uint64_t nextPC) {
        for (size_t i = MAX_HISTORY_LENGTH - 1; i > 0; --i) {
            active_hist.ghr[i] = active_hist.ghr[i - 1];
        }
        active_hist.ghr[0] = taken;
    }

private:
    std::vector<int> initialize_weight(int bank) {
        std::vector<int> weight(history_lengths[bank], -1);
        for (size_t i = 0; i < history_lengths[bank]; ++i) {
            if (active_hist.ghr[i] != 0) weight[i] = active_hist.ghr[i];
        }
        return weight;
    }
};

static conv_pred conv_predicter;
