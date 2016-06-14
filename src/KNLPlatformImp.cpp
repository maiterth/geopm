/*
 * Copyright (c) 2015, 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <math.h>
#include <unistd.h>
#include "geopm_error.h"
#include "geopm_message.h"
#include "Exception.hpp"
#include "KNLPlatformImp.hpp"

namespace geopm
{
    KNLPlatformImp::KNLPlatformImp()
        : PlatformImp(3, 5, 8.0)
        , m_energy_units(1.0)
        , m_power_units(1.0)
        , m_dram_energy_units(1.5258789063E-5)
        , m_min_pkg_watts(1)
        , m_max_pkg_watts(100)
        , m_min_pp0_watts(1)
        , m_max_pp0_watts(100)
        , m_min_dram_watts(1)
        , m_max_dram_watts(100)
        , m_platform_id(0x657)
        , M_KNL_MODEL_NAME("Knights Landing")
        , M_BOX_FRZ_EN(0x1 << 16)
        , M_BOX_FRZ(0x1 << 8)
        , M_CTR_EN(0x1 << 22)
        , M_RST_CTRS(0x1 << 1)
        , M_L2_FILTER_MASK(0x7 << 18)
        , M_L2_REQ_MISS_EV_SEL(0x2e)
        , M_L2_REQ_MISS_UMASK(0x41 << 8)
        , M_L2_PREFETCH_EV_SEL(0x3e)
        , M_L2_PREFETCH_UMASK(0x04 << 8)
        , M_EVENT_SEL_0(M_L2_REQ_MISS_EV_SEL)
        , M_UMASK_0(M_L2_REQ_MISS_UMASK)
        , M_EVENT_SEL_1(M_L2_PREFETCH_EV_SEL)
        , M_UMASK_1(M_L2_PREFETCH_UMASK)
        , M_PKG_POWER_LIMIT_MASK(0x0007800000078000ul)
        , M_DRAM_POWER_LIMIT_MASK(0xfefffful & M_PKG_POWER_LIMIT_MASK)
        , M_PP0_POWER_LIMIT_MASK(0xfffffful & M_PKG_POWER_LIMIT_MASK)
        , M_EXTRA_SIGNAL(1)
    {

    }

    KNLPlatformImp::~KNLPlatformImp()
    {
        while(m_cpu_file_desc.size()) {
            close(m_cpu_file_desc.back());
            m_cpu_file_desc.pop_back();
        }
    }

    bool KNLPlatformImp::model_supported(int platform_id)
    {
        return (platform_id == m_platform_id);
    }

    std::string KNLPlatformImp::platform_name()
    {
        return M_KNL_MODEL_NAME;
    }

    int KNLPlatformImp::power_control_domain(void) const
    {
        return GEOPM_DOMAIN_PACKAGE;
    }

    int KNLPlatformImp::frequency_control_domain(void) const
    {
        return GEOPM_DOMAIN_PACKAGE;
    }

    int KNLPlatformImp::performance_counter_domain(void) const
    {
        return GEOPM_DOMAIN_TILE;
    }

    double KNLPlatformImp::read_signal(int device_type, int device_index, int signal_type)
    {
        double value = 0.0;
        int offset_idx = 0;

        switch (signal_type) {
            case GEOPM_TELEMETRY_TYPE_PKG_ENERGY:
                offset_idx = device_index * m_num_energy_signal;
                value = msr_overflow(offset_idx, 32,
                                     (double)msr_read(device_type, device_index,
                                                      m_signal_msr_offset[0]));
                value *= m_energy_units;
                break;
            case GEOPM_TELEMETRY_TYPE_PP0_ENERGY:
                offset_idx = device_index * m_num_energy_signal + 1;
                value = msr_overflow(offset_idx, 32,
                                     (double)msr_read(device_type, device_index,
                                                      m_signal_msr_offset[1]));
                value *= m_energy_units;
                break;
            case GEOPM_TELEMETRY_TYPE_DRAM_ENERGY:
                offset_idx = device_index * m_num_energy_signal + 2;
                value = msr_overflow(offset_idx, 32,
                                     (double)msr_read(device_type, device_index,
                                                      m_signal_msr_offset[2]));
                value *= m_dram_energy_units;
                break;
            case GEOPM_TELEMETRY_TYPE_FREQUENCY:
                offset_idx = m_num_package * m_num_energy_signal + device_index * m_num_counter_signal;
                value = (double)((msr_read(device_type, device_index / m_num_cpu_per_core,
                                          m_signal_msr_offset[3]) >> 8) & 0x0FF);
                //convert to MHZ
                value *= 0.1;
                break;
            case GEOPM_TELEMETRY_TYPE_INST_RETIRED:
                offset_idx = m_num_package * m_num_energy_signal + device_index * m_num_counter_signal + 1;
                value = msr_overflow(offset_idx, 64,
                                     (double)msr_read(device_type, device_index / m_num_cpu_per_core,
                                                      m_signal_msr_offset[4]));
                break;
            case GEOPM_TELEMETRY_TYPE_CLK_UNHALTED_CORE:
                offset_idx = m_num_package * m_num_energy_signal + device_index * m_num_counter_signal + 2;
                value = msr_overflow(offset_idx, 64,
                                     (double)msr_read(device_type, device_index / m_num_cpu_per_core,
                                                      m_signal_msr_offset[5])) / m_num_core_per_tile;
                break;
            case GEOPM_TELEMETRY_TYPE_CLK_UNHALTED_REF:
                offset_idx = m_num_package * m_num_energy_signal + device_index * m_num_counter_signal + 3;
                value = msr_overflow(offset_idx, 64,
                                     (double)msr_read(device_type, device_index / m_num_cpu_per_core,
                                                      m_signal_msr_offset[6]));
                break;
            case GEOPM_TELEMETRY_TYPE_READ_BANDWIDTH:
                offset_idx = m_num_package * m_num_energy_signal + device_index * m_num_counter_signal + 4;
                value = msr_overflow(offset_idx, 48,
                                     (double)msr_read(device_type, device_index / m_num_cpu_per_core,
                                                      m_signal_msr_offset[7 + device_index]));
                value += msr_overflow(offset_idx + 1, 48,
                                     (double)msr_read(device_type, device_index / m_num_cpu_per_core,
                                                      m_signal_msr_offset[8 + device_index]));
                break;
            default:
                throw geopm::Exception("KNLPlatformImp::read_signal: Invalid signal type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
                break;
        }

        return value;
    }

    void KNLPlatformImp::write_control(int device_type, int device_index, int signal_type, double value)
    {
        uint64_t msr_val = 0;

        switch (signal_type) {
            case GEOPM_TELEMETRY_TYPE_PKG_ENERGY:
                if (value < m_min_pkg_watts) {
                    value = m_min_pkg_watts;
                }
                if (value > m_max_pkg_watts) {
                    value = m_max_pkg_watts;
                }
                msr_val = (uint64_t)(value * m_power_units);
                msr_val = msr_val | (msr_val << 32) | M_PKG_POWER_LIMIT_MASK;
                msr_write(device_type, device_index, m_control_msr_offset[0], msr_val);
                break;
            case GEOPM_TELEMETRY_TYPE_PP0_ENERGY:
                if (value < m_min_pp0_watts) {
                    value = m_min_pp0_watts;
                }
                if (value > m_max_pp0_watts) {
                    value = m_max_pp0_watts;
                }
                msr_val = (uint64_t)(value * m_power_units);
                msr_val = msr_val | (msr_val << 32) | M_PP0_POWER_LIMIT_MASK;
                msr_write(device_type, device_index, m_control_msr_offset[1], msr_val);
                break;
            case GEOPM_TELEMETRY_TYPE_DRAM_ENERGY:
                if (value < m_min_dram_watts) {
                    value = m_min_dram_watts;
                }
                if (value > m_max_dram_watts) {
                    value = m_max_dram_watts;
                }
                msr_val = (uint64_t)(value * m_power_units);
                msr_val = msr_val | (msr_val << 32) | M_DRAM_POWER_LIMIT_MASK;
                msr_write(device_type, device_index, m_control_msr_offset[2], msr_val);
                break;
            case GEOPM_TELEMETRY_TYPE_FREQUENCY:
                msr_val = (uint64_t)(value * 10);
                msr_val = msr_val << 8;
                msr_write(device_type, device_index / m_num_cpu_per_core, m_control_msr_offset[3], msr_val);
                break;
            default:
                throw geopm::Exception("KNLPlatformImp::read_signal: Invalid signal type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
                break;
        }
    }

    void KNLPlatformImp::msr_initialize()
    {
        for (int i = 0; i < m_num_logical_cpu; i++) {
            msr_open(i);
        }
        load_msr_offsets();
        rapl_init();
        cbo_counters_init();
        fixed_counters_init();

        // Add en extra counter signal since we use two counters to calculate read bandwidth
        int num_signal = m_num_energy_signal * m_num_package + (m_num_counter_signal + M_EXTRA_SIGNAL)  * m_num_tile;
        m_msr_value_last.resize(num_signal);
        m_msr_overflow_offset.resize(num_signal);
        std::fill(m_msr_value_last.begin(), m_msr_value_last.end(), 0.0);
        std::fill(m_msr_overflow_offset.begin(), m_msr_overflow_offset.end(), 0.0);

        //Save off the msr offsets for the signals we want to read to avoid a map lookup
        m_signal_msr_offset.push_back(msr_offset("PKG_ENERGY_STATUS"));
        m_signal_msr_offset.push_back(msr_offset("PP0_ENERGY_STATUS"));
        m_signal_msr_offset.push_back(msr_offset("DRAM_ENERGY_STATUS"));
        m_signal_msr_offset.push_back(msr_offset("IA32_PERF_STATUS"));
        m_signal_msr_offset.push_back(msr_offset("PERF_FIXED_CTR0"));
        m_signal_msr_offset.push_back(msr_offset("PERF_FIXED_CTR1"));
        m_signal_msr_offset.push_back(msr_offset("PERF_FIXED_CTR2"));
        for (int i = 0; i < m_num_tile; i++) {
            std::string msr_name("_MSR_PMON_CTR0");
            msr_name.insert(0, std::to_string(i));
            msr_name.insert(0, "C");
            m_signal_msr_offset.push_back(msr_offset(msr_name));
            msr_name = "_MSR_PMON_CTR1";
            msr_name.insert(0, std::to_string(i));
            msr_name.insert(0, "C");
            m_signal_msr_offset.push_back(msr_offset(msr_name));
        }

        //Save off the msr offsets for the controls we want to write to avoid a map lookup
        m_control_msr_offset.push_back(msr_offset("PKG_POWER_LIMIT"));
        m_control_msr_offset.push_back(msr_offset("DRAM_POWER_LIMIT"));
        m_control_msr_offset.push_back(msr_offset("PP0_POWER_LIMIT"));
        m_control_msr_offset.push_back(msr_offset("IA32_PERF_CTL"));
    }

    void KNLPlatformImp::msr_reset()
    {
        rapl_reset();
        cbo_counters_reset();
        fixed_counters_reset();
    }

    void KNLPlatformImp::rapl_init()
    {
        uint64_t tmp;

        //Make sure units are consistent between packages
        tmp = msr_read(GEOPM_DOMAIN_PACKAGE, 0, "RAPL_POWER_UNIT");
        m_energy_units = pow(0.5, (double)((tmp >> 8)  & 0x1F));
        m_power_units = pow(2, (double)(tmp & 0xF));

        for (int i = 1; i < m_num_package; i++) {
            tmp = msr_read(GEOPM_DOMAIN_PACKAGE, i, "RAPL_POWER_UNIT");
            double energy = pow(0.5, (double)((tmp >> 8) & 0x1F00));
            double power = pow(2, (double)((tmp >> 0) & 0xF));
            if (energy != m_energy_units || power != m_power_units) {
                throw Exception("detected inconsistent power units among packages", GEOPM_ERROR_RUNTIME, __FILE__, __LINE__);
            }
        }

        //Make sure bounds are consistent between packages
        tmp = msr_read(GEOPM_DOMAIN_PACKAGE, 0, "PKG_POWER_INFO");
        m_min_pkg_watts = ((double)((tmp >> 16) & 0x7fff)) / m_power_units;
        m_max_pkg_watts = ((double)((tmp >> 32) & 0x7fff)) / m_power_units;

        tmp = msr_read(GEOPM_DOMAIN_PACKAGE, 0, "DRAM_POWER_INFO");
        m_min_dram_watts = ((double)((tmp >> 16) & 0x7fff)) / m_power_units;
        m_max_dram_watts = ((double)((tmp >> 32) & 0x7fff)) / m_power_units;

        for (int i = 1; i < m_num_package; i++) {
            tmp = msr_read(GEOPM_DOMAIN_PACKAGE, i, "PKG_POWER_INFO");
            double pkg_min = ((double)((tmp >> 16) & 0x7fff)) / m_power_units;
            double pkg_max = ((double)((tmp >> 32) & 0x7fff)) / m_power_units;
            if (pkg_min != m_min_pkg_watts || pkg_max != m_max_pkg_watts) {
                throw Exception("detected inconsistent power pkg bounds among packages", GEOPM_ERROR_RUNTIME, __FILE__, __LINE__);
            }
            tmp = msr_read(GEOPM_DOMAIN_PACKAGE, i, "DRAM_POWER_INFO");
            double dram_min = ((double)((tmp >> 16) & 0x7fff)) / m_power_units;
            double dram_max = ((double)((tmp >> 32) & 0x7fff)) / m_power_units;
            if (dram_min != m_min_dram_watts || dram_max != m_max_dram_watts) {
                throw Exception("detected inconsistent power dram bounds among packages", GEOPM_ERROR_RUNTIME, __FILE__, __LINE__);
            }
        }
        m_min_pp0_watts = m_min_pkg_watts;
        m_max_pp0_watts = m_max_pkg_watts;

        rapl_reset();
    }

    void KNLPlatformImp::cbo_counters_init()
    {
        for (int i = 0; i < m_num_tile; i++) {
            std::string ctl1_msr_name("_MSR_PMON_CTL0");
            std::string ctl2_msr_name("_MSR_PMON_CTL1");
            std::string box_msr_name("_MSR_PMON_BOX_CTL");
            std::string filter_msr_name("_MSR_PMON_BOX_FILTER");
            box_msr_name.insert(0, std::to_string(i));
            box_msr_name.insert(0, "C");
            ctl1_msr_name.insert(0, std::to_string(i));
            ctl1_msr_name.insert(0, "C");
            ctl2_msr_name.insert(0, std::to_string(i));
            ctl2_msr_name.insert(0, "C");
            filter_msr_name.insert(0, std::to_string(i));
            filter_msr_name.insert(0, "C");

            // enable freeze
            msr_write(GEOPM_DOMAIN_TILE, i, box_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, box_msr_name)
                      | M_BOX_FRZ_EN);
            // freeze box
            msr_write(GEOPM_DOMAIN_TILE, i, box_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, box_msr_name)
                      | M_BOX_FRZ);
            // enable counter 0
            msr_write(GEOPM_DOMAIN_TILE, i, ctl1_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, ctl1_msr_name)
                      | M_CTR_EN);
            // enable counter 1
            msr_write(GEOPM_DOMAIN_TILE, i, ctl2_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, ctl2_msr_name)
                      | M_CTR_EN);
            // l2 misses
            msr_write(GEOPM_DOMAIN_TILE, i, ctl1_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, ctl1_msr_name)
                      | M_EVENT_SEL_0 | M_UMASK_0);
            // l2 prefetches
            msr_write(GEOPM_DOMAIN_TILE, i, ctl2_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, ctl2_msr_name)
                      | M_EVENT_SEL_1 | M_UMASK_1);
            // reset counters
            msr_write(GEOPM_DOMAIN_TILE, i, box_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, box_msr_name)
                      | M_RST_CTRS);
            // disable freeze
            msr_write(GEOPM_DOMAIN_TILE, i, box_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, box_msr_name)
                      | M_BOX_FRZ);
            // unfreeze box
            msr_write(GEOPM_DOMAIN_TILE, i, box_msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, box_msr_name)
                      & ~M_BOX_FRZ_EN);
        }
    }

    void KNLPlatformImp::fixed_counters_init()
    {
        for (int tile = 0; tile < m_num_tile; tile++) {
            msr_write(GEOPM_DOMAIN_TILE, tile, "PERF_FIXED_CTR_CTRL", 0x0333);
            msr_write(GEOPM_DOMAIN_TILE, tile, "PERF_GLOBAL_CTRL", 0x700000003);
            msr_write(GEOPM_DOMAIN_TILE, tile, "PERF_GLOBAL_OVF_CTRL", 0x0);
        }
    }

    void KNLPlatformImp::rapl_reset()
    {
        //clear power limits
        for (int i = 1; i < m_num_package; i++) {
            msr_write(GEOPM_DOMAIN_PACKAGE, i, "PKG_POWER_LIMIT", 0x0);
            msr_write(GEOPM_DOMAIN_PACKAGE, i, "PP0_POWER_LIMIT", 0x0);
            msr_write(GEOPM_DOMAIN_PACKAGE, i, "DRAM_POWER_LIMIT", 0x0);
        }

    }

    void KNLPlatformImp::cbo_counters_reset()
    {
        for (int i = 0; i < m_num_tile; i++) {
            std::string msr_name("_MSR_PMON_BOX_CTL");
            msr_name.insert(0, std::to_string(i));
            msr_name.insert(0, "C");
            // reset counters
            msr_write(GEOPM_DOMAIN_TILE, i, msr_name,
                      msr_read(GEOPM_DOMAIN_TILE, i, msr_name)
                      | M_RST_CTRS);
        }
    }

    void KNLPlatformImp::fixed_counters_reset()
    {
        for (int tile = 0; tile < m_num_tile; tile++) {
            msr_write(GEOPM_DOMAIN_TILE, tile, "PERF_FIXED_CTR0", 0x0);
            msr_write(GEOPM_DOMAIN_TILE, tile, "PERF_FIXED_CTR1", 0x0);
            msr_write(GEOPM_DOMAIN_TILE, tile, "PERF_FIXED_CTR2", 0x0);
        }
    }

    void KNLPlatformImp::load_msr_offsets()
    {
        std::map<std::string,std::pair<off_t,unsigned long> > msr_map = {
            {"IA32_PERF_STATUS",        {0x0198, 0x0000000000000000}},
            {"IA32_PERF_CTL",           {0x0199, 0x000000010000ffff}},
            {"RAPL_POWER_UNIT",         {0x0606, 0x0000000000000000}},
            {"PKG_POWER_LIMIT",         {0x0610, 0x00ffffff00ffffff}},
            {"PKG_ENERGY_STATUS",       {0x0611, 0x0000000000000000}},
            {"PKG_POWER_INFO",          {0x0614, 0x0000000000000000}},
            {"PP0_POWER_LIMIT",         {0x0638, 0x0000000000ffffff}},
            {"PP0_ENERGY_STATUS",       {0x0639, 0x0000000000000000}},
            {"DRAM_POWER_LIMIT",        {0x0618, 0x0000000000ffffff}},
            {"DRAM_ENERGY_STATUS",      {0x0619, 0x0000000000000000}},
            {"DRAM_PERF_STATUS",        {0x061B, 0x0000000000000000}},
            {"DRAM_POWER_INFO",         {0x061C, 0x0000000000000000}},
            {"PERF_FIXED_CTR_CTRL",     {0x038D, 0x0000000000000bbb}},
            {"PERF_GLOBAL_CTRL",        {0x038F, 0x0000000700000003}},
            {"PERF_GLOBAL_OVF_CTRL",    {0x0390, 0xc000000700000003}},
            {"PERF_FIXED_CTR0",         {0x0309, 0xffffffffffffffff}},
            {"PERF_FIXED_CTR1",         {0x030A, 0xffffffffffffffff}},
            {"PERF_FIXED_CTR2",         {0x030B, 0xffffffffffffffff}},
            {"C0_MSR_PMON_BOX_CTL",     {0x0E00, 0x00000000ffffffff}},
            {"C1_MSR_PMON_BOX_CTL",     {0x0E0C, 0x00000000ffffffff}},
            {"C2_MSR_PMON_BOX_CTL",     {0x0E18, 0x00000000ffffffff}},
            {"C3_MSR_PMON_BOX_CTL",     {0x0E24, 0x00000000ffffffff}},
            {"C4_MSR_PMON_BOX_CTL",     {0x0E30, 0x00000000ffffffff}},
            {"C5_MSR_PMON_BOX_CTL",     {0x0E3C, 0x00000000ffffffff}},
            {"C6_MSR_PMON_BOX_CTL",     {0x0E48, 0x00000000ffffffff}},
            {"C7_MSR_PMON_BOX_CTL",     {0x0E54, 0x00000000ffffffff}},
            {"C8_MSR_PMON_BOX_CTL",     {0x0E60, 0x00000000ffffffff}},
            {"C9_MSR_PMON_BOX_CTL",     {0x0E6C, 0x00000000ffffffff}},
            {"C10_MSR_PMON_BOX_CTL",    {0x0E78, 0x00000000ffffffff}},
            {"C11_MSR_PMON_BOX_CTL",    {0x0E84, 0x00000000ffffffff}},
            {"C12_MSR_PMON_BOX_CTL",    {0x0E90, 0x00000000ffffffff}},
            {"C13_MSR_PMON_BOX_CTL",    {0x0E9C, 0x00000000ffffffff}},
            {"C14_MSR_PMON_BOX_CTL",    {0x0EA8, 0x00000000ffffffff}},
            {"C15_MSR_PMON_BOX_CTL",    {0x0EB4, 0x00000000ffffffff}},
            {"C16_MSR_PMON_BOX_CTL",    {0x0EC0, 0x00000000ffffffff}},
            {"C17_MSR_PMON_BOX_CTL",    {0x0ECC, 0x00000000ffffffff}},
            {"C18_MSR_PMON_BOX_CTL",    {0x0ED8, 0x00000000ffffffff}},
            {"C19_MSR_PMON_BOX_CTL",    {0x0EE4, 0x00000000ffffffff}},
            {"C20_MSR_PMON_BOX_CTL",    {0x0EF0, 0x00000000ffffffff}},
            {"C21_MSR_PMON_BOX_CTL",    {0x0EFC, 0x00000000ffffffff}},
            {"C22_MSR_PMON_BOX_CTL",    {0x0F08, 0x00000000ffffffff}},
            {"C23_MSR_PMON_BOX_CTL",    {0x0F14, 0x00000000ffffffff}},
            {"C24_MSR_PMON_BOX_CTL",    {0x0F20, 0x00000000ffffffff}},
            {"C25_MSR_PMON_BOX_CTL",    {0x0F2C, 0x00000000ffffffff}},
            {"C26_MSR_PMON_BOX_CTL",    {0x0F38, 0x00000000ffffffff}},
            {"C27_MSR_PMON_BOX_CTL",    {0x0F44, 0x00000000ffffffff}},
            {"C28_MSR_PMON_BOX_CTL",    {0x0F50, 0x00000000ffffffff}},
            {"C29_MSR_PMON_BOX_CTL",    {0x0F5C, 0x00000000ffffffff}},
            {"C30_MSR_PMON_BOX_CTL",    {0x0F68, 0x00000000ffffffff}},
            {"C31_MSR_PMON_BOX_CTL",    {0x0F74, 0x00000000ffffffff}},
            {"C32_MSR_PMON_BOX_CTL",    {0x0F80, 0x00000000ffffffff}},
            {"C33_MSR_PMON_BOX_CTL",    {0x0F8C, 0x00000000ffffffff}},
            {"C34_MSR_PMON_BOX_CTL",    {0x0F98, 0x00000000ffffffff}},
            {"C35_MSR_PMON_BOX_CTL",    {0x0FA4, 0x00000000ffffffff}},
            {"C36_MSR_PMON_BOX_CTL",    {0x0FB0, 0x00000000ffffffff}},
            {"C37_MSR_PMON_BOX_CTL",    {0x0FBC, 0x00000000ffffffff}},
            {"C0_MSR_PMON_BOX_FILTER",  {0x0E05, 0x00000000ffffffff}},
            {"C1_MSR_PMON_BOX_FILTER",  {0x0E11, 0x00000000ffffffff}},
            {"C2_MSR_PMON_BOX_FILTER",  {0x0E1D, 0x00000000ffffffff}},
            {"C3_MSR_PMON_BOX_FILTER",  {0x0E29, 0x00000000ffffffff}},
            {"C4_MSR_PMON_BOX_FILTER",  {0x0E35, 0x00000000ffffffff}},
            {"C5_MSR_PMON_BOX_FILTER",  {0x0E41, 0x00000000ffffffff}},
            {"C6_MSR_PMON_BOX_FILTER",  {0x0E4D, 0x00000000ffffffff}},
            {"C7_MSR_PMON_BOX_FILTER",  {0x0E59, 0x00000000ffffffff}},
            {"C8_MSR_PMON_BOX_FILTER",  {0x0E65, 0x00000000ffffffff}},
            {"C9_MSR_PMON_BOX_FILTER",  {0x0E71, 0x00000000ffffffff}},
            {"C10_MSR_PMON_BOX_FILTER", {0x0E7D, 0x00000000ffffffff}},
            {"C11_MSR_PMON_BOX_FILTER", {0x0E89, 0x00000000ffffffff}},
            {"C12_MSR_PMON_BOX_FILTER", {0x0E95, 0x00000000ffffffff}},
            {"C13_MSR_PMON_BOX_FILTER", {0x0EA1, 0x00000000ffffffff}},
            {"C14_MSR_PMON_BOX_FILTER", {0x0EAD, 0x00000000ffffffff}},
            {"C15_MSR_PMON_BOX_FILTER", {0x0EB9, 0x00000000ffffffff}},
            {"C16_MSR_PMON_BOX_FILTER", {0x0EC5, 0x00000000ffffffff}},
            {"C17_MSR_PMON_BOX_FILTER", {0x0ED1, 0x00000000ffffffff}},
            {"C18_MSR_PMON_BOX_FILTER", {0x0EDD, 0x00000000ffffffff}},
            {"C19_MSR_PMON_BOX_FILTER", {0x0EE9, 0x00000000ffffffff}},
            {"C20_MSR_PMON_BOX_FILTER", {0x0EF5, 0x00000000ffffffff}},
            {"C21_MSR_PMON_BOX_FILTER", {0x0F01, 0x00000000ffffffff}},
            {"C22_MSR_PMON_BOX_FILTER", {0x0F0D, 0x00000000ffffffff}},
            {"C23_MSR_PMON_BOX_FILTER", {0x0F19, 0x00000000ffffffff}},
            {"C24_MSR_PMON_BOX_FILTER", {0x0F25, 0x00000000ffffffff}},
            {"C25_MSR_PMON_BOX_FILTER", {0x0F31, 0x00000000ffffffff}},
            {"C26_MSR_PMON_BOX_FILTER", {0x0F3D, 0x00000000ffffffff}},
            {"C27_MSR_PMON_BOX_FILTER", {0x0F49, 0x00000000ffffffff}},
            {"C28_MSR_PMON_BOX_FILTER", {0x0F55, 0x00000000ffffffff}},
            {"C29_MSR_PMON_BOX_FILTER", {0x0F61, 0x00000000ffffffff}},
            {"C30_MSR_PMON_BOX_FILTER", {0x0F6D, 0x00000000ffffffff}},
            {"C31_MSR_PMON_BOX_FILTER", {0x0F79, 0x00000000ffffffff}},
            {"C32_MSR_PMON_BOX_FILTER", {0x0F85, 0x00000000ffffffff}},
            {"C33_MSR_PMON_BOX_FILTER", {0x0F91, 0x00000000ffffffff}},
            {"C34_MSR_PMON_BOX_FILTER", {0x0F9D, 0x00000000ffffffff}},
            {"C35_MSR_PMON_BOX_FILTER", {0x0FA9, 0x00000000ffffffff}},
            {"C36_MSR_PMON_BOX_FILTER", {0x0FB5, 0x00000000ffffffff}},
            {"C37_MSR_PMON_BOX_FILTER", {0x0FC1, 0x00000000ffffffff}},
            {"C0_MSR_PMON_BOX_FILTER1", {0x0E06, 0x00000000ffffffff}},
            {"C1_MSR_PMON_BOX_FILTER1", {0x0E12, 0x00000000ffffffff}},
            {"C2_MSR_PMON_BOX_FILTER1", {0x0E1E, 0x00000000ffffffff}},
            {"C3_MSR_PMON_BOX_FILTER1", {0x0E2A, 0x00000000ffffffff}},
            {"C4_MSR_PMON_BOX_FILTER1", {0x0E36, 0x00000000ffffffff}},
            {"C5_MSR_PMON_BOX_FILTER1", {0x0E42, 0x00000000ffffffff}},
            {"C6_MSR_PMON_BOX_FILTER1", {0x0E4E, 0x00000000ffffffff}},
            {"C7_MSR_PMON_BOX_FILTER1", {0x0E5A, 0x00000000ffffffff}},
            {"C8_MSR_PMON_BOX_FILTER1", {0x0E66, 0x00000000ffffffff}},
            {"C9_MSR_PMON_BOX_FILTER1", {0x0E72, 0x00000000ffffffff}},
            {"C10_MSR_PMON_BOX_FILTER1",{0x0E7E, 0x00000000ffffffff}},
            {"C11_MSR_PMON_BOX_FILTER1",{0x0E8A, 0x00000000ffffffff}},
            {"C12_MSR_PMON_BOX_FILTER1",{0x0E96, 0x00000000ffffffff}},
            {"C13_MSR_PMON_BOX_FILTER1",{0x0EA2, 0x00000000ffffffff}},
            {"C14_MSR_PMON_BOX_FILTER1",{0x0EAE, 0x00000000ffffffff}},
            {"C15_MSR_PMON_BOX_FILTER1",{0x0EBA, 0x00000000ffffffff}},
            {"C16_MSR_PMON_BOX_FILTER1",{0x0EC6, 0x00000000ffffffff}},
            {"C17_MSR_PMON_BOX_FILTER1",{0x0ED2, 0x00000000ffffffff}},
            {"C18_MSR_PMON_BOX_FILTER1",{0x0EDE, 0x00000000ffffffff}},
            {"C19_MSR_PMON_BOX_FILTER1",{0x0EEA, 0x00000000ffffffff}},
            {"C20_MSR_PMON_BOX_FILTER1",{0x0EF6, 0x00000000ffffffff}},
            {"C21_MSR_PMON_BOX_FILTER1",{0x0F02, 0x00000000ffffffff}},
            {"C22_MSR_PMON_BOX_FILTER1",{0x0F0E, 0x00000000ffffffff}},
            {"C23_MSR_PMON_BOX_FILTER1",{0x0F1A, 0x00000000ffffffff}},
            {"C24_MSR_PMON_BOX_FILTER1",{0x0F26, 0x00000000ffffffff}},
            {"C25_MSR_PMON_BOX_FILTER1",{0x0F32, 0x00000000ffffffff}},
            {"C26_MSR_PMON_BOX_FILTER1",{0x0F3E, 0x00000000ffffffff}},
            {"C27_MSR_PMON_BOX_FILTER1",{0x0F4A, 0x00000000ffffffff}},
            {"C28_MSR_PMON_BOX_FILTER1",{0x0F56, 0x00000000ffffffff}},
            {"C29_MSR_PMON_BOX_FILTER1",{0x0F62, 0x00000000ffffffff}},
            {"C30_MSR_PMON_BOX_FILTER1",{0x0F6E, 0x00000000ffffffff}},
            {"C31_MSR_PMON_BOX_FILTER1",{0x0F7A, 0x00000000ffffffff}},
            {"C32_MSR_PMON_BOX_FILTER1",{0x0F86, 0x00000000ffffffff}},
            {"C33_MSR_PMON_BOX_FILTER1",{0x0F92, 0x00000000ffffffff}},
            {"C34_MSR_PMON_BOX_FILTER1",{0x0F9E, 0x00000000ffffffff}},
            {"C35_MSR_PMON_BOX_FILTER1",{0x0FAA, 0x00000000ffffffff}},
            {"C36_MSR_PMON_BOX_FILTER1",{0x0FB6, 0x00000000ffffffff}},
            {"C37_MSR_PMON_BOX_FILTER1",{0x0FC2, 0x00000000ffffffff}},
            {"C0_MSR_PMON_CTL0",        {0x0E01, 0x00000000ffffffff}},
            {"C1_MSR_PMON_CTL0",        {0x0E0D, 0x00000000ffffffff}},
            {"C2_MSR_PMON_CTL0",        {0x0E19, 0x00000000ffffffff}},
            {"C3_MSR_PMON_CTL0",        {0x0E25, 0x00000000ffffffff}},
            {"C4_MSR_PMON_CTL0",        {0x0E31, 0x00000000ffffffff}},
            {"C5_MSR_PMON_CTL0",        {0x0E3D, 0x00000000ffffffff}},
            {"C6_MSR_PMON_CTL0",        {0x0E49, 0x00000000ffffffff}},
            {"C7_MSR_PMON_CTL0",        {0x0E55, 0x00000000ffffffff}},
            {"C8_MSR_PMON_CTL0",        {0x0E61, 0x00000000ffffffff}},
            {"C9_MSR_PMON_CTL0",        {0x0E6D, 0x00000000ffffffff}},
            {"C10_MSR_PMON_CTL0",       {0x0E79, 0x00000000ffffffff}},
            {"C11_MSR_PMON_CTL0",       {0x0E85, 0x00000000ffffffff}},
            {"C12_MSR_PMON_CTL0",       {0x0E91, 0x00000000ffffffff}},
            {"C13_MSR_PMON_CTL0",       {0x0E9D, 0x00000000ffffffff}},
            {"C14_MSR_PMON_CTL0",       {0x0EA9, 0x00000000ffffffff}},
            {"C15_MSR_PMON_CTL0",       {0x0EB5, 0x00000000ffffffff}},
            {"C16_MSR_PMON_CTL0",       {0x0EC1, 0x00000000ffffffff}},
            {"C17_MSR_PMON_CTL0",       {0x0ECD, 0x00000000ffffffff}},
            {"C18_MSR_PMON_CTL0",       {0x0ED9, 0x00000000ffffffff}},
            {"C19_MSR_PMON_CTL0",       {0x0EE5, 0x00000000ffffffff}},
            {"C20_MSR_PMON_CTL0",       {0x0EF1, 0x00000000ffffffff}},
            {"C21_MSR_PMON_CTL0",       {0x0EFD, 0x00000000ffffffff}},
            {"C22_MSR_PMON_CTL0",       {0x0F09, 0x00000000ffffffff}},
            {"C23_MSR_PMON_CTL0",       {0x0F15, 0x00000000ffffffff}},
            {"C24_MSR_PMON_CTL0",       {0x0F21, 0x00000000ffffffff}},
            {"C25_MSR_PMON_CTL0",       {0x0F2D, 0x00000000ffffffff}},
            {"C26_MSR_PMON_CTL0",       {0x0F39, 0x00000000ffffffff}},
            {"C27_MSR_PMON_CTL0",       {0x0F45, 0x00000000ffffffff}},
            {"C28_MSR_PMON_CTL0",       {0x0F51, 0x00000000ffffffff}},
            {"C29_MSR_PMON_CTL0",       {0x0F5D, 0x00000000ffffffff}},
            {"C30_MSR_PMON_CTL0",       {0x0F69, 0x00000000ffffffff}},
            {"C31_MSR_PMON_CTL0",       {0x0F75, 0x00000000ffffffff}},
            {"C32_MSR_PMON_CTL0",       {0x0F81, 0x00000000ffffffff}},
            {"C33_MSR_PMON_CTL0",       {0x0F8D, 0x00000000ffffffff}},
            {"C34_MSR_PMON_CTL0",       {0x0F99, 0x00000000ffffffff}},
            {"C35_MSR_PMON_CTL0",       {0x0FA5, 0x00000000ffffffff}},
            {"C36_MSR_PMON_CTL0",       {0x0FB1, 0x00000000ffffffff}},
            {"C37_MSR_PMON_CTL0",       {0x0FBD, 0x00000000ffffffff}},
            {"C0_MSR_PMON_CTL1",        {0x0E02, 0x00000000ffffffff}},
            {"C1_MSR_PMON_CTL1",        {0x0E0E, 0x00000000ffffffff}},
            {"C2_MSR_PMON_CTL1",        {0x0E1A, 0x00000000ffffffff}},
            {"C3_MSR_PMON_CTL1",        {0x0E26, 0x00000000ffffffff}},
            {"C4_MSR_PMON_CTL1",        {0x0E32, 0x00000000ffffffff}},
            {"C5_MSR_PMON_CTL1",        {0x0E3E, 0x00000000ffffffff}},
            {"C6_MSR_PMON_CTL1",        {0x0E4A, 0x00000000ffffffff}},
            {"C7_MSR_PMON_CTL1",        {0x0E56, 0x00000000ffffffff}},
            {"C8_MSR_PMON_CTL1",        {0x0E62, 0x00000000ffffffff}},
            {"C9_MSR_PMON_CTL1",        {0x0E6E, 0x00000000ffffffff}},
            {"C10_MSR_PMON_CTL1",       {0x0E7A, 0x00000000ffffffff}},
            {"C11_MSR_PMON_CTL1",       {0x0E86, 0x00000000ffffffff}},
            {"C12_MSR_PMON_CTL1",       {0x0E92, 0x00000000ffffffff}},
            {"C13_MSR_PMON_CTL1",       {0x0E9E, 0x00000000ffffffff}},
            {"C14_MSR_PMON_CTL1",       {0x0EAA, 0x00000000ffffffff}},
            {"C15_MSR_PMON_CTL1",       {0x0EB6, 0x00000000ffffffff}},
            {"C16_MSR_PMON_CTL1",       {0x0EC2, 0x00000000ffffffff}},
            {"C17_MSR_PMON_CTL1",       {0x0ECE, 0x00000000ffffffff}},
            {"C18_MSR_PMON_CTL1",       {0x0EDA, 0x00000000ffffffff}},
            {"C19_MSR_PMON_CTL1",       {0x0EE6, 0x00000000ffffffff}},
            {"C20_MSR_PMON_CTL1",       {0x0EF2, 0x00000000ffffffff}},
            {"C21_MSR_PMON_CTL1",       {0x0EFE, 0x00000000ffffffff}},
            {"C22_MSR_PMON_CTL1",       {0x0F0A, 0x00000000ffffffff}},
            {"C23_MSR_PMON_CTL1",       {0x0F16, 0x00000000ffffffff}},
            {"C24_MSR_PMON_CTL1",       {0x0F22, 0x00000000ffffffff}},
            {"C25_MSR_PMON_CTL1",       {0x0F2E, 0x00000000ffffffff}},
            {"C26_MSR_PMON_CTL1",       {0x0F3A, 0x00000000ffffffff}},
            {"C27_MSR_PMON_CTL1",       {0x0F46, 0x00000000ffffffff}},
            {"C28_MSR_PMON_CTL1",       {0x0F52, 0x00000000ffffffff}},
            {"C29_MSR_PMON_CTL1",       {0x0F5E, 0x00000000ffffffff}},
            {"C30_MSR_PMON_CTL1",       {0x0F6A, 0x00000000ffffffff}},
            {"C31_MSR_PMON_CTL1",       {0x0F76, 0x00000000ffffffff}},
            {"C32_MSR_PMON_CTL1",       {0x0F82, 0x00000000ffffffff}},
            {"C33_MSR_PMON_CTL1",       {0x0F8E, 0x00000000ffffffff}},
            {"C34_MSR_PMON_CTL1",       {0x0F9A, 0x00000000ffffffff}},
            {"C35_MSR_PMON_CTL1",       {0x0FA6, 0x00000000ffffffff}},
            {"C36_MSR_PMON_CTL1",       {0x0FB2, 0x00000000ffffffff}},
            {"C37_MSR_PMON_CTL1",       {0x0FBE, 0x00000000ffffffff}},
            {"C0_MSR_PMON_CTR0",        {0x0E08, 0x0000000000000000}},
            {"C1_MSR_PMON_CTR0",        {0x0E14, 0x0000000000000000}},
            {"C2_MSR_PMON_CTR0",        {0x0E20, 0x0000000000000000}},
            {"C3_MSR_PMON_CTR0",        {0x0E2C, 0x0000000000000000}},
            {"C4_MSR_PMON_CTR0",        {0x0E38, 0x0000000000000000}},
            {"C5_MSR_PMON_CTR0",        {0x0E44, 0x0000000000000000}},
            {"C6_MSR_PMON_CTR0",        {0x0E50, 0x0000000000000000}},
            {"C7_MSR_PMON_CTR0",        {0x0E5C, 0x0000000000000000}},
            {"C8_MSR_PMON_CTR0",        {0x0E68, 0x0000000000000000}},
            {"C9_MSR_PMON_CTR0",        {0x0E74, 0x0000000000000000}},
            {"C10_MSR_PMON_CTR0",       {0x0E80, 0x0000000000000000}},
            {"C11_MSR_PMON_CTR0",       {0x0E8C, 0x0000000000000000}},
            {"C12_MSR_PMON_CTR0",       {0x0E98, 0x0000000000000000}},
            {"C13_MSR_PMON_CTR0",       {0x0EA4, 0x0000000000000000}},
            {"C14_MSR_PMON_CTR0",       {0x0EB0, 0x0000000000000000}},
            {"C15_MSR_PMON_CTR0",       {0x0EBC, 0x0000000000000000}},
            {"C16_MSR_PMON_CTR0",       {0x0EC8, 0x0000000000000000}},
            {"C17_MSR_PMON_CTR0",       {0x0ED4, 0x0000000000000000}},
            {"C18_MSR_PMON_CTR0",       {0x0EE0, 0x0000000000000000}},
            {"C19_MSR_PMON_CTR0",       {0x0EEC, 0x0000000000000000}},
            {"C20_MSR_PMON_CTR0",       {0x0EF8, 0x0000000000000000}},
            {"C21_MSR_PMON_CTR0",       {0x0F04, 0x0000000000000000}},
            {"C22_MSR_PMON_CTR0",       {0x0F10, 0x0000000000000000}},
            {"C23_MSR_PMON_CTR0",       {0x0F1C, 0x0000000000000000}},
            {"C24_MSR_PMON_CTR0",       {0x0F28, 0x0000000000000000}},
            {"C25_MSR_PMON_CTR0",       {0x0F34, 0x0000000000000000}},
            {"C26_MSR_PMON_CTR0",       {0x0F40, 0x0000000000000000}},
            {"C27_MSR_PMON_CTR0",       {0x0F4C, 0x0000000000000000}},
            {"C28_MSR_PMON_CTR0",       {0x0F58, 0x0000000000000000}},
            {"C29_MSR_PMON_CTR0",       {0x0F64, 0x0000000000000000}},
            {"C30_MSR_PMON_CTR0",       {0x0F70, 0x0000000000000000}},
            {"C31_MSR_PMON_CTR0",       {0x0F7C, 0x0000000000000000}},
            {"C32_MSR_PMON_CTR0",       {0x0F88, 0x0000000000000000}},
            {"C33_MSR_PMON_CTR0",       {0x0F94, 0x0000000000000000}},
            {"C34_MSR_PMON_CTR0",       {0x0FA0, 0x0000000000000000}},
            {"C35_MSR_PMON_CTR0",       {0x0FAC, 0x0000000000000000}},
            {"C36_MSR_PMON_CTR0",       {0x0FB8, 0x0000000000000000}},
            {"C37_MSR_PMON_CTR0",       {0x0FC4, 0x0000000000000000}},
            {"C0_MSR_PMON_CTR1",        {0x0E09, 0x0000000000000000}},
            {"C1_MSR_PMON_CTR1",        {0x0E15, 0x0000000000000000}},
            {"C2_MSR_PMON_CTR1",        {0x0E21, 0x0000000000000000}},
            {"C3_MSR_PMON_CTR1",        {0x0E2D, 0x0000000000000000}},
            {"C4_MSR_PMON_CTR1",        {0x0E39, 0x0000000000000000}},
            {"C5_MSR_PMON_CTR1",        {0x0E45, 0x0000000000000000}},
            {"C6_MSR_PMON_CTR1",        {0x0E51, 0x0000000000000000}},
            {"C7_MSR_PMON_CTR1",        {0x0E5D, 0x0000000000000000}},
            {"C8_MSR_PMON_CTR1",        {0x0E69, 0x0000000000000000}},
            {"C9_MSR_PMON_CTR1",        {0x0E75, 0x0000000000000000}},
            {"C10_MSR_PMON_CTR1",       {0x0E81, 0x0000000000000000}},
            {"C11_MSR_PMON_CTR1",       {0x0E8D, 0x0000000000000000}},
            {"C12_MSR_PMON_CTR1",       {0x0E99, 0x0000000000000000}},
            {"C13_MSR_PMON_CTR1",       {0x0EA5, 0x0000000000000000}},
            {"C14_MSR_PMON_CTR1",       {0x0EB1, 0x0000000000000000}},
            {"C15_MSR_PMON_CTR1",       {0x0EBD, 0x0000000000000000}},
            {"C16_MSR_PMON_CTR1",       {0x0EC9, 0x0000000000000000}},
            {"C17_MSR_PMON_CTR1",       {0x0ED5, 0x0000000000000000}},
            {"C17_MSR_PMON_CTR1",       {0x0EE1, 0x0000000000000000}},
            {"C18_MSR_PMON_CTR1",       {0x0EED, 0x0000000000000000}},
            {"C19_MSR_PMON_CTR1",       {0x0EF9, 0x0000000000000000}},
            {"C20_MSR_PMON_CTR1",       {0x0F05, 0x0000000000000000}},
            {"C21_MSR_PMON_CTR1",       {0x0F11, 0x0000000000000000}},
            {"C22_MSR_PMON_CTR1",       {0x0F1D, 0x0000000000000000}},
            {"C23_MSR_PMON_CTR1",       {0x0F29, 0x0000000000000000}},
            {"C24_MSR_PMON_CTR1",       {0x0F35, 0x0000000000000000}},
            {"C25_MSR_PMON_CTR1",       {0x0F41, 0x0000000000000000}},
            {"C26_MSR_PMON_CTR1",       {0x0F4D, 0x0000000000000000}},
            {"C27_MSR_PMON_CTR1",       {0x0F59, 0x0000000000000000}},
            {"C28_MSR_PMON_CTR1",       {0x0F65, 0x0000000000000000}},
            {"C29_MSR_PMON_CTR1",       {0x0F71, 0x0000000000000000}},
            {"C30_MSR_PMON_CTR1",       {0x0F7D, 0x0000000000000000}},
            {"C31_MSR_PMON_CTR1",       {0x0F89, 0x0000000000000000}},
            {"C32_MSR_PMON_CTR1",       {0x0F95, 0x0000000000000000}},
            {"C33_MSR_PMON_CTR1",       {0x0FA1, 0x0000000000000000}},
            {"C34_MSR_PMON_CTR1",       {0x0FAD, 0x0000000000000000}},
            {"C35_MSR_PMON_CTR1",       {0x0FB9, 0x0000000000000000}},
            {"C37_MSR_PMON_CTR1",       {0x0FC5, 0x0000000000000000}}
        };

        m_msr_offset_map = msr_map;

    }
}