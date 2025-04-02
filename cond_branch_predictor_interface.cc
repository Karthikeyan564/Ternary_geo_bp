/*
  Copyright (C) ARM Limited 2008-2025  All rights reserved.                                                                                                                                                                                                                        

  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// This file provides a sample predictor integration based on the interface provided.

#include "lib/sim_common_structs.h"
#include "cbp2016_tage_sc_l.h"
#include "my_cond_branch_predictor.h"
#include "lib/log.h"
#include <cassert>
#include <stdio.h>
#include <iterator>
#include <fstream>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "lib/parameters.h"
// #include "lib/parameters.cc"

//FILE *fileptr = fopen("output/cond_branches.log", "w");
//
// beginCondDirPredictor()
// 
// This function is called by the simulator before the start of simulation.
// It can be used for arbitrary initialization steps for the contestant's code.
//
extern log_files files;
std::unordered_map<uint64_t/*key*/, DebugLog/*val*/> histories_log;
// std::unordered_map<uint64_t, std::deque<std::vector<int>>> depChains;
// std::unordered_map<uint64_t, std::vector<uint64_t>> depChains;
// extern bool LOAD_DEPENDENT_BRANCHES ;

// Function to compute CyclWP summary from unordered_map
std::map<int8_t, std::tuple<int, uint64_t, double>> compute_CyclWP_summary(const std::unordered_map<uint64_t, DebugLog>& histories_log) {
    std::map<int8_t, std::pair<int, uint64_t>> summary; // Key: Load Dependence, Value: (miss count, CyclWP sum)

    for (const auto& [key, log] : histories_log) {
        if (log.inst_class == InstClass::condBranchInstClass && log.executed) {  // Filter executed branches with Inst_Class == 3
            bool mispredicted = (log.taken != log.pred_dir);
            uint64_t CyclWP = std::max(log.fetch_cycle, log.execute_cycle) - log.pred_cycle;
            CyclWP = mispredicted ? CyclWP : 0;

            if(mispredicted){
                if(log.src_regs_string == "[]" || log.src_regs_string == "[64]")
                {
                    summary[-1].first += 1;  // Increment miss count
                    summary[-1].second += CyclWP;  // Add CyclWP sum
                }
            else 
                {
                    summary[log.load_dependence].first += 1;  // Increment miss count
                    summary[log.load_dependence].second += CyclWP;  // Add CyclWP sum
                }
            }
            
        }
    }

    // Compute CyclWP per miss and store in final map
    std::map<int8_t, std::tuple<int, uint64_t, double>> final_summary;
    for (const auto& [load_dep, stats] : summary) {
        int num_misses = stats.first;
        uint64_t total_CyclWP = stats.second;
        double avg_CyclWP = (num_misses > 0) ? static_cast<double>(total_CyclWP) / num_misses : 0.0;
        final_summary[load_dep] = std::make_tuple(num_misses, total_CyclWP, avg_CyclWP);
    }

    return final_summary;
}

class DependencyGraph {
public:
    void addDependency(uint64_t dest, uint64_t src) {
        graph[dest].insert(src);
    }

    void deleteDestination(uint64_t dest) {
        // Remove the destination register from the graph
        graph.erase(dest);

        // Remove any references to `dest` in other registers' dependency lists
        for (auto& [key, dependencies] : graph) {
            dependencies.erase(dest);
        }
    }

    std::unordered_set<uint64_t> getAllDependencies(uint64_t dest) {
        std::unordered_set<uint64_t> visited;
        dfs(dest, visited);
        visited.erase(dest);  // Remove the destination itself if present
        return visited;
    }

private:
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> graph;

    void dfs(uint64_t node, std::unordered_set<uint64_t>& visited) {
        if (visited.count(node)) return;
        visited.insert(node);

        if (graph.find(node) != graph.end()) {
            for (uint64_t src : graph[node]) {
                dfs(src, visited);
            }
        }
    }
};

uint64_t get_unique_inst_id(uint64_t seq_no, uint8_t piece) 
{
    assert(piece < 16);
    return (seq_no << 4) | (piece & 0x000F);
}

void beginCondDirPredictor()
{
    if (LOAD_DEPENDENT_BRANCHES) {
        printf("Optmizied for load dependent branches with usefulness increment of %d\n", U_incrment) ;
    }
    else
    {
        printf("Not optimized for load dependent branches\n");
    }
    // setup sample_predictor
    cbp2016_tage_sc_l.setup();
    cond_predictor_impl.setup();
}

//
// notify_instr_fetch(uint64_t seq_no, uint8_t piece, uint64_t pc, const uint64_t fetch_cycle)
// 
// This function is called when any instructions(not just branches) gets fetched.
// Along with the unique identifying ids(seq_no, piece), PC of the instruction and fetch_cycle are also provided as inputs
//
void notify_instr_fetch(uint64_t seq_no, uint8_t piece, uint64_t pc, const uint64_t fetch_cycle)
{
}

//
// get_cond_dir_prediction(uint64_t seq_no, uint8_t piece, uint64_t pc, const uint64_t pred_cycle)
// 
// This function is called by the simulator for predicting conditional branches.
// input values are unique identifying ids(seq_no, piece) and PC of the branch.
// return value is the predicted direction. 
//
bool get_cond_dir_prediction(uint64_t seq_no, uint8_t piece, uint64_t pc, const uint64_t pred_cycle,const uint64_t fetch_cycle, const uint64_t exec_cycle)
{
    // which predictor was used, predictions from each, history tables where branch is found and their preiction, history tables it was stored in dured update
    const PredDebugInfo active_predDebug =  cbp2016_tage_sc_l.predict(seq_no, piece, pc);  
    const bool tage_sc_l_pred = active_predDebug.pred_taken;
    const bool my_prediction = cond_predictor_impl.predict(seq_no, piece, pc, tage_sc_l_pred);
    // fprintf(files.pred_history, "%" PRIx64 ",%" PRIx8 ",%" PRIx64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", 
    //     seq_no, piece, pc, pred_cycle, fetch_cycle, exec_cycle);

    DebugLog activeLog;
    const auto log_key = get_unique_inst_id(seq_no, piece);
    activeLog.pc = pc;
    activeLog.pred_cycle = pred_cycle;
    activeLog.fetch_cycle = fetch_cycle;
    activeLog.execute_cycle = exec_cycle;
    activeLog.pred_dir = tage_sc_l_pred;
    activeLog.inst_class = InstClass::condBranchInstClass;
    activeLog.GHIST = active_predDebug.GHIST;
    activeLog.predictor_used = active_predDebug.predictor_used;
    histories_log[log_key] = activeLog;

    return my_prediction;
}

//
// spec_update(uint64_t seq_no, uint8_t piece, uint64_t pc, InstClass inst_class, const bool resolve_dir, const bool pred_dir, const uint64_t next_pc)
// 
// This function is called by the simulator for updating the history vectors and any state that needs to be updated speculatively.
// The function is called for all the branches (not just conditional branches). To faciliate accurate history updates, spec_update is called right
// after a prediction is made.
// input values are unique identifying ids(seq_no, piece), PC of the instruction, instruction class, predicted/resolve direction and the next_pc 
//
void spec_update(uint64_t seq_no, uint8_t piece, uint64_t pc, InstClass inst_class, const bool resolve_dir, const bool pred_dir, const uint64_t next_pc)
{
    assert(is_br(inst_class));
    int br_type = 0;
    switch(inst_class)
    {
        case InstClass::condBranchInstClass:
            br_type = 1;
            break;
        case InstClass::uncondDirectBranchInstClass:
            br_type = 0; 
            break;
        case InstClass::uncondIndirectBranchInstClass:
            br_type = 2;
            break;
        case InstClass::callDirectInstClass:
            br_type = 0;
            break;
        case InstClass::callIndirectInstClass:
            br_type = 2; 
            break;
        case InstClass::ReturnInstClass:
            br_type = 2;
            break;
        default:
            assert(false);
    }

    if(inst_class == InstClass::condBranchInstClass)
    {
        cbp2016_tage_sc_l.history_update(seq_no, piece, pc, br_type, pred_dir, resolve_dir, next_pc);
        cond_predictor_impl.history_update(seq_no, piece, pc, resolve_dir, next_pc);
    }
    else
    {
        cbp2016_tage_sc_l.TrackOtherInst(pc, br_type, pred_dir, resolve_dir, next_pc);
    }

}

//
// notify_instr_decode(uint64_t seq_no, uint8_t piece, uint64_t pc, const DecodeInfo& _decode_info, const uint64_t decode_cycle)
// 
// This function is called when any instructions(not just branches) gets decoded.
// Along with the unique identifying ids(seq_no, piece), PC of the instruction, decode info and cycle are also provided as inputs
//
// For the sample predictor implementation, we do not leverage decode information
DependencyGraph depGraph;
std::unordered_map<uint64_t/*key*/, bool/*val*/> registers_in_flight;
void notify_instr_decode(uint64_t seq_no, uint8_t piece, uint64_t pc, const DecodeInfo& _decode_info, const uint64_t decode_cycle)
{
    if (is_load(_decode_info.insn_class)){
        
        std::vector<uint64_t> sources = _decode_info.src_reg_info;
        uint64_t dst_reg = _decode_info.dst_reg_info.value();
        registers_in_flight[dst_reg] = true;
        depGraph.deleteDestination(dst_reg);
        for (uint64_t src : sources) 
            depGraph.addDependency(dst_reg, src);

        
    }
    else if (is_cond_br(_decode_info.insn_class)){
        uint16_t loadCount = 0;
        std::vector<uint64_t> sources = _decode_info.src_reg_info;
        if (!(sources.empty() || (sources.size() == 1 && sources[0] == 64))) {
            for (uint64_t src : sources) {
                std::unordered_set<uint64_t> dependencies = depGraph.getAllDependencies(src);
                loadCount += dependencies.size();  
            }
        }
        const auto log_key = get_unique_inst_id(seq_no, piece);
        DebugLog& activeLog = histories_log[log_key];
        activeLog.load_dependence = loadCount;
    }
    
}

//
// notify_agen_complete(uint64_t seq_no, uint8_t piece, uint64_t pc, const DecodeInfo& _decode_info, const uint64_t mem_va, const uint64_t mem_sz, const uint64_t agen_cycle)
// 
// This function is called when any load/store instructions complete agen.
// Along with the unique identifying ids(seq_no, piece), PC of the instruction, decode info, mem_va and mem_sz and agen_cycle are also provided as inputs
//
void notify_agen_complete(uint64_t seq_no, uint8_t piece, uint64_t pc, const DecodeInfo& _decode_info, const uint64_t mem_va, const uint64_t mem_sz, const uint64_t agen_cycle)
{
}

//
// notify_instr_execute_resolve(uint64_t seq_no, uint8_t piece, uint64_t pc, const bool pred_dir, const ExecuteInfo& _exec_info, const uint64_t execute_cycle)
// 
// This function is called when any instructions(not just branches) gets executed.
// Along with the unique identifying ids(seq_no, piece), PC of the instruction, execute info and cycle are also provided as inputs
//
// For conditional branches, we use this information to update the predictor.
// At the moment, we do not consider updating any other structure, but the contestants are allowed to  update any other predictor state.
std::string vector_to_string(const std::vector<uint64_t>& vec) {
    std::ostringstream out;
    if (vec.empty()) {
        return "[]";
    }
    std::copy(vec.begin(), vec.end() - 1, std::ostream_iterator<uint64_t>(out, ";"));
    out << vec.back();
    std::string regs_string( out.str() );
    regs_string = "[" + regs_string + "]";
    return regs_string;
}

void notify_instr_execute_resolve(uint64_t seq_no, uint8_t piece, uint64_t pc, const bool pred_dir, const ExecuteInfo& _exec_info, const uint64_t execute_cycle)
{
    const auto log_key = get_unique_inst_id(seq_no, piece);
    const bool is_branch = is_br(_exec_info.dec_info.insn_class);
    // extern bool LOAD_DEPENDENT_BRANCHES;
    if(is_branch)
    {
        if (is_cond_br(_exec_info.dec_info.insn_class))
        {
            const bool _resolve_dir = _exec_info.taken.value();
            const uint64_t _next_pc = _exec_info.next_pc;
            bool is_LD_dependent = false;
            
            std::vector<uint64_t> sources = _exec_info.dec_info.src_reg_info;
            if (!(sources.empty() || (sources.size() == 1 && sources[0] == 64))) {
                for (uint64_t src : sources) {
                    is_LD_dependent = is_LD_dependent | registers_in_flight[src];
                }
            }

            if (LOAD_DEPENDENT_BRANCHES) {
            }
            else
            {
                is_LD_dependent = false;
            }

            cbp2016_tage_sc_l.update(seq_no, piece, pc, _resolve_dir, pred_dir, _next_pc, is_LD_dependent);
            cond_predictor_impl.update(seq_no, piece, pc, _resolve_dir, pred_dir, _next_pc);
	    // fprintf(files.history, "%" PRIx64 ",%" PRIx8 ",%" PRIx64 ",%" PRIx64 ",%" PRIu64 ",%d,%d\n", 
        //         seq_no, piece, pc, _next_pc, execute_cycle, pred_dir, _resolve_dir);
        }
        else
        {
            assert(pred_dir);
        }
    }

    // std::vector<uint64_t> sources = _exec_info.dec_info.src_reg_info;
    // uint64_t dst_reg = _exec_info.dec_info.dst_reg_info.value();
    // Update dependency chain
    const uint8_t maxChainDepth = 5;

    // if (is_load(_exec_info.dec_info.insn_class)){
    //     std::deque<std::vector<int>> newChain;
    //     for (int src : sources) {
    //         std::vector<int> newDep = {src};

    //         if (depChains.find(src) != depChains.end()) {
    //             for (const auto& chain : depChains[src]) {
    //                 if (newDep.size() < maxChainDepth) {
    //                     newDep.insert(newDep.end(), chain.begin(), chain.end());
    //                 }
    //             }
    //         }
    //         newChain.push_back(newDep);
    //     }

    //     depChains[dst_reg] = newChain;
    // }

    // Get number of dependencies for conditional branches
    // uint8_t loadCount = 255;
    // if(is_cond_br(_exec_info.dec_info.insn_class)) {
    //     loadCount = 0;
    //     if (!(sources.empty() || (sources.size() == 1 && sources[0] == 64))) {
    //         for (int src : sources) {
    //             if (depChains.find(src) != depChains.end()) {
    //                 for (const auto& chain : depChains[src]) {
    //                     loadCount += chain.size();
    //                 }
    //             }                
    //         }
    //     }
    //     loadCount = std::min(loadCount,maxChainDepth);        
    // }    

    if(is_cond_br(_exec_info.dec_info.insn_class) || is_load(_exec_info.dec_info.insn_class)){
            
        DebugLog& activeLog = histories_log[log_key];
        if (is_load(_exec_info.dec_info.insn_class))
        {
            activeLog.pc = pc;
            activeLog.execute_cycle = execute_cycle;
            activeLog.pred_dir = pred_dir;
            activeLog.inst_class = InstClass::loadInstClass;
            // loadCount = UINT8_MAX;
        }
        // activeLog.inst_class = _exec_info.dec_info.insn_class;
        activeLog.taken =  (_exec_info.taken.has_value())? _exec_info.taken.value():-1;
        activeLog.src_regs_string = vector_to_string(_exec_info.dec_info.src_reg_info);
        activeLog.dst_reg = (_exec_info.dec_info.dst_reg_info.has_value())? _exec_info.dec_info.dst_reg_info.value(): 255 ;  
        activeLog.mem_va = (_exec_info.mem_va.has_value())? _exec_info.mem_va.value():0;
        activeLog.next_pc = _exec_info.next_pc;
        // activeLog.load_dependence = loadCount;
        activeLog.executed = 1;
    }

    // if(is_cond_br(_exec_info.dec_info.insn_class) || is_load(_exec_info.dec_info.insn_class)){

    //     const std::string src_regs_string = vector_to_string(_exec_info.dec_info.src_reg_info);

    //     const int taken = (_exec_info.taken.has_value())? _exec_info.taken.value():-1;
    //     const uint64_t dst_reg = (_exec_info.dec_info.dst_reg_info.has_value())? _exec_info.dec_info.dst_reg_info.value(): 256 ;  
    //     const uint64_t mem_va = (_exec_info.mem_va.has_value())? _exec_info.mem_va.value():0;
          
    //     fprintf(files.history, "%" PRIx64 ",%" PRIx8 ",%" PRIx64 ",%" PRIx64 ",%" PRIu64 ",%d,%d,%s" ",%" PRIu64 ",%" PRIu64 "\n", 
    //         seq_no, piece, pc, _exec_info.next_pc, execute_cycle, pred_dir, taken, src_regs_string.c_str(),
    //         dst_reg, mem_va);
    // }
}

//
// notify_instr_commit(uint64_t seq_no, uint8_t piece, uint64_t pc, const bool pred_dir, const ExecuteInfo& _exec_info, const uint64_t commit_cycle)
// 
// This function is called when any instructions(not just branches) gets committed.
// Along with the unique identifying ids(seq_no, piece), PC of the instruction, execute info and cycle are also provided as inputs
//
// For the sample predictor implementation, we do not leverage commit information
void notify_instr_commit(uint64_t seq_no, uint8_t piece, uint64_t pc, const bool pred_dir, const ExecuteInfo& _exec_info, const uint64_t commit_cycle)
{   
    if(is_load(_exec_info.dec_info.insn_class)){
        uint64_t dst_reg = _exec_info.dec_info.dst_reg_info.value();
        // depChains.erase(dst_reg);
        depGraph.deleteDestination(dst_reg);
        registers_in_flight.erase(dst_reg);
    }
}

//
// endCondDirPredictor()
//
// This function is called by the simulator at the end of simulation.
// It can be used by the contestant to print out other contestant-specific measurements.
//
void writeHistorylog(const std::unordered_map<uint64_t, DebugLog>& histories_log, FILE *file) {
    if (!file) {
        std::cerr << "Error: Invalid file pointer." << std::endl;
        return;
    }
    
    // Write CSV header
    fprintf(file, "Key,PC,Next_PC,Pred_Cycle,Fetch_Cycle,Execute_Cycle,Pred_Dir,Taken," 
                   "Predictor_Used,Src_Regs,Dst_Reg,Mem_VA,Inst_Class,GHIST,Load_Dependence,executed\n");
    
    for (const auto& [key, log] : histories_log) {
        fprintf(file,"%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,\"%s\",%d,%lu,%d,%lu,%d,%d\n",
                key,
                log.pc,
                log.next_pc,
                log.pred_cycle,
                log.fetch_cycle,
                log.execute_cycle,
                log.pred_dir,
                log.taken,
                log.predictor_used,
                log.src_regs_string.c_str(),
                log.dst_reg,
                log.mem_va,
                static_cast<int>(log.inst_class),
                log.GHIST,
                log.load_dependence,
                log.executed);
    }
    fflush(file);
    // std::cout << "Data written to history log file." << std::endl;
}


void write_CyclWP_summary_to_file(const std::map<int8_t, std::tuple<int, uint64_t, double>>& summary, FILE *file) {

    if (!file) {
        std::cerr << "Error: Invalid file pointer." << std::endl;
        return;
    }

    // Write header
    fprintf(file, "Load_Dependence,Number of misses,CyclWP,CyclWP per miss\n");

    // Write data
    for (const auto& [load_dep, stats] : summary) {
        int num_misses = std::get<0>(stats);
        uint64_t total_CyclWP = std::get<1>(stats);
        double avg_CyclWP = std::get<2>(stats);

        fprintf(file, "%d,%d,%lu,%.2f\n", load_dep, num_misses, total_CyclWP, avg_CyclWP);
    }

    fclose(file);
}

void endCondDirPredictor ()
{
    // writeHistorylog(histories_log, files.history); 
    // write_CyclWP_summary_to_file(compute_CyclWP_summary(histories_log),files.CyclWP_summary);    
    cbp2016_tage_sc_l.terminate();
    cond_predictor_impl.terminate();
}
