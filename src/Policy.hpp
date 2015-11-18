/*
 * Copyright (c) 2015, Intel Corporation
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

#ifndef POLICY_HPP_INCLUDE
#define POLICY_HPP_INCLUDE

#include <vector>
#include <map>

#include "geopm_message.h"

namespace geopm
{
    class Policy
    {
        public:
            Policy();
            Policy(int num_domain);
            Policy(const std::vector <double> &target);
            virtual ~Policy();
            bool operator==(const Policy& other) const;
            void clear(void);
            void update(int domain, double target);
            void update(const std::vector <double> &target);
            void updated_target(std::map <int, double> &target);
            void target(std::vector <double> &target);
            void target(int domain, double &target);
            void policy_message(int regionid, std::vector<geopm_policy_message_s> message) const;
            int num_domain(void) const;
            void valid_target(std::map <int, double> &target) const;
        protected:
            int m_mode;
            int m_num_sample;
            double m_budget;
            double m_flags;
            int m_goal;

            std::vector <double> m_target;
            std::vector <bool> m_updated;
    };
}

#endif