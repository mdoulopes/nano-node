#pragma once

#include <nano/node/common.hpp>
#include <nano/node/stats.hpp>
#include <nano/node/transport/transport.hpp>

namespace nano
{
namespace transport
{
	class tcp_channels;
	class channel_tcp : public nano::transport::channel
	{
		friend class nano::transport::tcp_channels;

	public:
		channel_tcp (nano::node &, std::shared_ptr<nano::socket>);
		~channel_tcp ();
		size_t hash_code () const override;
		bool operator== (nano::transport::channel const &) const override;
		void send_buffer (std::shared_ptr<std::vector<uint8_t>>, nano::stat::detail, std::function<void(boost::system::error_code const &, size_t)> const & = nullptr) override;
		std::function<void(boost::system::error_code const &, size_t)> callback (std::shared_ptr<std::vector<uint8_t>>, nano::stat::detail, std::function<void(boost::system::error_code const &, size_t)> const & = nullptr) const override;
		std::function<void(boost::system::error_code const &, size_t)> tcp_callback (std::shared_ptr<std::vector<uint8_t>>, nano::stat::detail, nano::tcp_endpoint const &, std::function<void(boost::system::error_code const &, size_t)> const & = nullptr) const;
		std::string to_string () const override;
		bool operator== (nano::transport::channel_tcp const & other_a) const
		{
			return &node == &other_a.node && socket == other_a.socket;
		}
		std::shared_ptr<nano::socket> socket;

		nano::endpoint get_endpoint () const override
		{
			std::lock_guard<std::mutex> lk (channel_mutex);
			if (socket)
			{
				return nano::transport::map_tcp_to_endpoint (socket->remote_endpoint ());
			}
			else
			{
				return nano::endpoint (boost::asio::ip::address_v6::any (), 0);
			}
		}

		nano::tcp_endpoint get_tcp_endpoint () const override
		{
			std::lock_guard<std::mutex> lk (channel_mutex);
			if (socket)
			{
				return socket->remote_endpoint ();
			}
			else
			{
				return nano::tcp_endpoint (boost::asio::ip::address_v6::any (), 0);
			}
		}

		nano::transport::transport_type get_type () const override
		{
			return nano::transport::transport_type::tcp;
		}
	};
	class tcp_channels final
	{
		friend class nano::transport::channel_tcp;

	public:
		tcp_channels (nano::node &);
		bool insert (std::shared_ptr<nano::transport::channel_tcp>);
		void erase (nano::tcp_endpoint const &);
		size_t size () const;
		std::shared_ptr<nano::transport::channel_tcp> find_channel (nano::tcp_endpoint const &) const;
		void random_fill (std::array<nano::endpoint, 8> &) const;
		std::unordered_set<std::shared_ptr<nano::transport::channel>> random_set (size_t) const;
		bool store_all (bool = true);
		std::shared_ptr<nano::transport::channel_tcp> find_node_id (nano::account const &);
		// Get the next peer for attempting a tcp connection
		nano::tcp_endpoint bootstrap_peer ();
		void receive ();
		void start ();
		void stop ();
		void process_message (nano::message const &, nano::tcp_endpoint const &, nano::account const &);
		void process_keepalive (nano::keepalive const &, nano::tcp_endpoint const &, bool);
		bool max_ip_connections (nano::tcp_endpoint const &);
		// Should we reach out to this endpoint with a keepalive message
		bool reachout (nano::endpoint const &);
		std::unique_ptr<seq_con_info_component> collect_seq_con_info (std::string const &);
		void purge (std::chrono::steady_clock::time_point const &);
		void purge_syn_cookies (std::chrono::steady_clock::time_point const &);
		// Returns boost::none if the IP is rate capped on syn cookie requests,
		// or if the endpoint already has a syn cookie query
		boost::optional<nano::uint256_union> assign_syn_cookie (nano::tcp_endpoint const &);
		// Returns false if valid, true if invalid (true on error convention)
		// Also removes the syn cookie from the store if valid
		bool validate_syn_cookie (nano::tcp_endpoint const &, nano::account const &, nano::signature const &);
		void ongoing_keepalive ();
		void list (std::deque<std::shared_ptr<nano::transport::channel>> &);
		void modify (std::shared_ptr<nano::transport::channel_tcp>, std::function<void(std::shared_ptr<nano::transport::channel_tcp>)>);
		void update (nano::tcp_endpoint const &);
		// Connection start
		void start_tcp (nano::endpoint const &, std::function<void(std::shared_ptr<nano::transport::channel>)> const & = nullptr);
		void start_tcp_receive_node_id (std::shared_ptr<nano::transport::channel_tcp>, nano::endpoint const &, std::shared_ptr<std::vector<uint8_t>>, std::function<void(std::shared_ptr<nano::transport::channel>)> const &);
		void udp_fallback (nano::endpoint const &, std::function<void(std::shared_ptr<nano::transport::channel>)> const &);
		nano::node & node;

	private:
		void ongoing_syn_cookie_cleanup ();
		class endpoint_tag
		{
		};
		class ip_address_tag
		{
		};
		class random_access_tag
		{
		};
		class last_packet_sent_tag
		{
		};
		class last_bootstrap_attempt_tag
		{
		};
		class node_id_tag
		{
		};
		class channel_tcp_wrapper final
		{
		public:
			std::shared_ptr<nano::transport::channel_tcp> channel;
			nano::tcp_endpoint endpoint () const
			{
				return channel->get_tcp_endpoint ();
			}
			std::chrono::steady_clock::time_point last_packet_sent () const
			{
				return channel->get_last_packet_sent ();
			}
			std::chrono::steady_clock::time_point last_bootstrap_attempt () const
			{
				return channel->get_last_bootstrap_attempt ();
			}
			boost::asio::ip::address ip_address () const
			{
				return endpoint ().address ();
			}
			nano::account node_id () const
			{
				auto node_id_l (channel->get_node_id ());
				if (node_id_l.is_initialized ())
				{
					return node_id_l.get ();
				}
				else
				{
					assert (false);
					return 0;
				}
			}
		};
		class tcp_endpoint_attempt final
		{
		public:
			nano::tcp_endpoint endpoint;
			std::chrono::steady_clock::time_point last_attempt;
		};
		class syn_cookie_info final
		{
		public:
			nano::uint256_union cookie;
			std::chrono::steady_clock::time_point created_at;
		};
		mutable std::mutex mutex;
		mutable std::mutex syn_cookie_mutex;
		boost::multi_index_container<
		channel_tcp_wrapper,
		boost::multi_index::indexed_by<
		boost::multi_index::random_access<boost::multi_index::tag<random_access_tag>>,
		boost::multi_index::ordered_non_unique<boost::multi_index::tag<last_bootstrap_attempt_tag>, boost::multi_index::const_mem_fun<channel_tcp_wrapper, std::chrono::steady_clock::time_point, &channel_tcp_wrapper::last_bootstrap_attempt>>,
		boost::multi_index::hashed_unique<boost::multi_index::tag<endpoint_tag>, boost::multi_index::const_mem_fun<channel_tcp_wrapper, nano::tcp_endpoint, &channel_tcp_wrapper::endpoint>>,
		boost::multi_index::hashed_non_unique<boost::multi_index::tag<node_id_tag>, boost::multi_index::const_mem_fun<channel_tcp_wrapper, nano::account, &channel_tcp_wrapper::node_id>>,
		boost::multi_index::ordered_non_unique<boost::multi_index::tag<last_packet_sent_tag>, boost::multi_index::const_mem_fun<channel_tcp_wrapper, std::chrono::steady_clock::time_point, &channel_tcp_wrapper::last_packet_sent>>,
		boost::multi_index::ordered_non_unique<boost::multi_index::tag<ip_address_tag>, boost::multi_index::const_mem_fun<channel_tcp_wrapper, boost::asio::ip::address, &channel_tcp_wrapper::ip_address>>>>
		channels;
		boost::multi_index_container<
		tcp_endpoint_attempt,
		boost::multi_index::indexed_by<
		boost::multi_index::hashed_unique<boost::multi_index::member<tcp_endpoint_attempt, nano::tcp_endpoint, &tcp_endpoint_attempt::endpoint>>,
		boost::multi_index::ordered_non_unique<boost::multi_index::member<tcp_endpoint_attempt, std::chrono::steady_clock::time_point, &tcp_endpoint_attempt::last_attempt>>>>
		attempts;
		std::unordered_map<nano::tcp_endpoint, syn_cookie_info> syn_cookies;
		std::unordered_map<boost::asio::ip::address, unsigned> syn_cookies_per_ip;
		std::atomic<bool> stopped{ false };
	};
} // namespace transport
} // namespace nano
