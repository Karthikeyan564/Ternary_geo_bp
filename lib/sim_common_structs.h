#pragma once
/*

   Copyright (c) 2019, North Carolina State University
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

   3. The names “North Carolina State University”, “NCSU” and any trade-name, personal name,
   trademark, trade device, service mark, symbol, image, icon, or any abbreviation, contraction or
   simulation thereof owned by North Carolina State University must not be used to endorse or promote products derived from this software without prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

// Author: Eric Rotenberg (ericro@ncsu.edu)

#include <optional>
#include <vector>
#include <cstdint>
#include <ostream>
#include <string>

enum class InstClass : uint8_t
{
    aluInstClass = 0,
    loadInstClass = 1,
    storeInstClass = 2,
    condBranchInstClass = 3,
    uncondDirectBranchInstClass = 4,
    uncondIndirectBranchInstClass = 5,
    fpInstClass = 6,
    slowAluInstClass = 7,
    undefInstClass = 8,
    callDirectInstClass = 9,
    callIndirectInstClass = 10,
    ReturnInstClass = 11,
};

static constexpr const char * cInfo[] = {"aluOp", "loadOp", "stOp", "condBrOp", "uncondDirBrOp", "uncondIndBrOp", "fpOp", "slowAluOp", "undefOp", "callDirBrOp", "callIndBrOp", "retBrOp",};

inline bool is_load(InstClass inst_class)
{
    return inst_class == InstClass::loadInstClass;
}

inline bool is_store(InstClass inst_class)
{
    return inst_class == InstClass::storeInstClass;
}

inline bool is_mem(InstClass inst_class)
{
    return  is_load(inst_class) || is_store(inst_class);
}

inline bool is_br(InstClass inst_class)
{
    const bool cond_br = (inst_class== InstClass::condBranchInstClass);
    const bool uncond_dir_br = (inst_class== InstClass::uncondDirectBranchInstClass);
    const bool uncond_ind_br = (inst_class== InstClass::uncondIndirectBranchInstClass);
    const bool call_dir_br = (inst_class== InstClass::callDirectInstClass);
    const bool call_ind_br = (inst_class== InstClass::callIndirectInstClass);
    const bool ret_br = (inst_class== InstClass::ReturnInstClass);
    return (cond_br |  uncond_dir_br | uncond_ind_br | call_dir_br | call_ind_br | ret_br);
}

inline bool is_cond_br(InstClass inst_class)
{
    return (inst_class== InstClass::condBranchInstClass);
}

inline bool is_uncond_ind_br(InstClass inst_class)
{
    return (inst_class == InstClass::uncondIndirectBranchInstClass) || (inst_class == InstClass::callIndirectInstClass) || (inst_class == InstClass::ReturnInstClass);
}

inline bool is_uncond_br(InstClass inst_class)
{
    return is_br(inst_class) && (inst_class!= InstClass::condBranchInstClass);
}

enum class HitMissInfo : uint8_t
{
    Miss,
    L1DHit,
    L2Hit,
    L3Hit,
    Invalid
};

struct DecodeInfo
{
    InstClass insn_class;
    std::vector<uint64_t> src_reg_info;
    std::optional<uint64_t> dst_reg_info;
    //std::optional<uint64_t> imm_op;
    DecodeInfo()
    {
        reset();
    }

    void reset()
    {
        insn_class = InstClass::undefInstClass;
        src_reg_info.clear();
        dst_reg_info.reset();
    }

    friend std::ostream& operator<<(std::ostream& os, const DecodeInfo& dec_info)
    {
        os<<"{ Class:"<<cInfo[static_cast<uint8_t>(dec_info.insn_class)];
        os<<" num_src_reg:"<<dec_info.src_reg_info.size();
        os<<" arch_dst_reg:"<<dec_info.dst_reg_info.value_or(0xFFFFFFFFFFFFFFFF);
        os<<" }";
        return os;
    }
};

struct ExecuteInfo
{
    DecodeInfo dec_info;
    std::optional<bool> taken = std::nullopt;
    uint64_t next_pc;
    std::optional<uint64_t> taken_target = std::nullopt;
    std::optional<uint64_t> mem_va = std::nullopt;
    std::optional<uint64_t> mem_sz = std::nullopt;
    std::optional<uint64_t> dst_reg_value = std::nullopt;
    ExecuteInfo()
    {
        reset();
    }
    void reset()
    {
        dec_info.reset();
        taken.reset();
        next_pc = UINT64_MAX;
        taken_target.reset();
        mem_va.reset();
        mem_sz.reset();
        dst_reg_value.reset();
    }

    friend std::ostream& operator<<(std::ostream& os, const ExecuteInfo& exec_info)
    {
        os<<"{ DecodeInfo:"<<exec_info.dec_info;
        os<<" mem_va:0x"<<std::hex<<exec_info.mem_va.value_or(0xFFFFFFFFFFFFFFFF)<<std::dec;
        os<<" mem_sz:0x"<<std::hex<<exec_info.mem_sz.value_or(0xFFFFFFFFFFFFFFFF)<<std::dec;
        os<<" dst_reg_value:0x"<<std::hex<<exec_info.dst_reg_value.value_or(0xFFFFFFFFFFFFFFFF)<<std::dec;
        os<<" }";
        return os;
    }
};


struct PredDebugInfo
{
    bool pred_taken;
    uint8_t predictor_used;
    uint64_t GHIST;
    std::vector<uint32_t> history_lengths_pred;

    PredDebugInfo()
    {
        pred_taken = 0;
        predictor_used = UINT8_MAX;
        GHIST = UINT64_MAX;
        history_lengths_pred.clear();

    }
};

struct UpdateDebugInfo
{
    std::vector<uint32_t> history_lengths_update;

    UpdateDebugInfo()
    {
        history_lengths_update.clear();
    }
};

struct DebugLog{
    uint64_t pc; //
    uint64_t next_pc; 
    uint64_t pred_cycle; //
    uint64_t fetch_cycle; //
    uint64_t execute_cycle; //
    uint8_t pred_dir; //
    uint8_t taken;
    uint8_t predictor_used;//
    std::string src_regs_string;
    uint8_t  dst_reg;
    uint64_t mem_va;
    InstClass inst_class; //
    uint64_t GHIST;   //
    uint8_t load_dependence;


    DebugLog()
    {
        reset();
    }
    void reset()
    {
        pc = 0;
        next_pc = 0;
        pred_cycle = 0;
        fetch_cycle = 0;
        execute_cycle = 0;
        pred_dir = 2;
        taken = 2;
        predictor_used = 10;
        mem_va = 0;
        src_regs_string = "[]";
        dst_reg = 255;
        inst_class = InstClass::undefInstClass;
        load_dependence = 0;
        GHIST = 0;

    }
};
