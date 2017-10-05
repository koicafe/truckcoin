// Copyright (c) 2014, Truckcoin, The GoldenHill Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once
#include "include_base_utils.h"

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <boost/serialization/version.hpp>
#include <boost/utility.hpp>

#include "string_tools.h"
#include "syncobj.h"
#include "math_helper.h"
#include "cryptonote_basic_impl.h"
#include "verification_context.h"
#include "crypto/hash.h"


namespace cryptonote
{
  class blockchain_storage;
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/

  class tx_memory_pool: boost::noncopyable
  {
  public:
    tx_memory_pool(blockchain_storage& bchs);
    bool add_tx(const transaction &tx, const crypto::hash &id, size_t blob_size, tx_verification_context& tvc, bool keeped_by_block);
    bool add_tx(const transaction &tx, tx_verification_context& tvc, bool keeped_by_block);
    //gets tx and remove it from pool
    bool take_tx(const crypto::hash &id, transaction &tx, size_t& blob_size, uint64_t& fee);

    bool have_tx(const crypto::hash &id) const;
    bool on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id);
    bool on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id);
    void on_idle();

    void lock() const;
    void unlock() const;

    // load/store operations
    bool init(const std::string& config_folder);
    bool deinit();
    bool fill_block_template(block &bl, size_t median_size, uint64_t already_generated_coins, size_t &total_size, uint64_t &fee);
    void get_transactions(std::list<transaction>& txs) const;
    bool get_transaction(const crypto::hash& h, transaction& tx) const;
    size_t get_transactions_count() const;
    std::string print_pool(bool short_format) const;

    /*bool flush_pool(const std::strig& folder);
    bool inflate_pool(const std::strig& folder);*/

#define CURRENT_MEMPOOL_ARCHIVE_VER    8

    template<class archive_t>
    void serialize(archive_t & a, const unsigned int version)
    {
      if(version < CURRENT_MEMPOOL_ARCHIVE_VER )
        return;
      CRITICAL_REGION_LOCAL(m_transactions_lock);
      a & m_transactions;
      a & m_spent_key_images;
    }

    struct tx_details
    {
      transaction tx;
      size_t blob_size;
      uint64_t fee;
      crypto::hash max_used_block_id;
      uint64_t max_used_block_height;
      bool kept_by_block;
      //
      uint64_t last_failed_height;
      crypto::hash last_failed_id;
      time_t receive_time;
    };

  private:
  	bool remove_stuck_transactions();
    bool have_tx_keyimg_as_spent(const crypto::key_image& key_im) const;
    bool have_tx_keyimges_as_spent(const transaction& tx) const;
    bool remove_transaction_keyimages(const transaction& tx);
    static bool have_key_images(const std::unordered_set<crypto::key_image>& kic, const transaction& tx);
    static bool append_key_images(std::unordered_set<crypto::key_image>& kic, const transaction& tx);

    bool is_transaction_ready_to_go(tx_details& txd) const;
    typedef std::unordered_map<crypto::hash, tx_details > transactions_container;
    typedef std::unordered_map<crypto::key_image, std::unordered_set<crypto::hash> > key_images_container;

    mutable epee::critical_section m_transactions_lock;
    transactions_container m_transactions;
    key_images_container m_spent_key_images;
    epee::math_helper::once_a_time_seconds<30> m_remove_stuck_tx_interval;

    //transactions_container m_alternative_transactions;

    std::string m_config_folder;
    blockchain_storage& m_blockchain;
    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    /*class inputs_visitor: public boost::static_visitor<bool>
    {
      key_images_container& m_spent_keys;
    public:
      inputs_visitor(key_images_container& spent_keys): m_spent_keys(spent_keys)
      {}
      bool operator()(const txin_to_key& tx) const
      {
        auto pr = m_spent_keys.insert(tx.k_image);
        CHECK_AND_ASSERT_MES(pr.second, false, "Tried to insert transaction with input seems already spent, input: " << epee::string_tools::pod_to_hex(tx.k_image));
        return true;
      }
      bool operator()(const txin_gen& tx) const
      {
        CHECK_AND_ASSERT_MES(false, false, "coinbase transaction in memory pool");
        return false;
      }
      bool operator()(const txin_to_script& tx) const {return false;}
      bool operator()(const txin_to_scripthash& tx) const {return false;}
    }; */
    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    class amount_visitor: public boost::static_visitor<uint64_t>
    {
    public:
      uint64_t operator()(const txin_to_key& tx) const
      {
        return tx.amount;
      }
      uint64_t operator()(const txin_gen& tx) const
      {
        CHECK_AND_ASSERT_MES(false, false, "coinbase transaction in memory pool");
        return 0;
      }
      uint64_t operator()(const txin_to_script& tx) const {return 0;}
      uint64_t operator()(const txin_to_scripthash& tx) const {return 0;}
    };

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    friend class blockchain_storage;
#endif
  };
}

namespace boost
{
  namespace serialization
  {
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::tx_memory_pool::tx_details& td, const unsigned int version)
    {
      ar & td.blob_size;
      ar & td.fee;
      ar & td.tx;
      ar & td.max_used_block_height;
      ar & td.max_used_block_id;
      ar & td.last_failed_height;
      ar & td.last_failed_id;
      ar & td.receive_time;

    }
  }
}
BOOST_CLASS_VERSION(cryptonote::tx_memory_pool, CURRENT_MEMPOOL_ARCHIVE_VER)



