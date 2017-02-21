#ifndef LCDM_H
#define LCDM_H

#include <future>
#include <map>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <boost/asio.hpp>

namespace puloon {

	class lcdm {
		public:
			typedef std::uint8_t cassette_number;
			typedef std::map<cassette_number, std::uint32_t> bill_quantity_by_cassette;

			enum class operation_status : std::uint8_t {
				good,
				normal_stop,
				pickup_error,
				jam,
				overflow_bill,
				bill_end,
				note_request_error,
				counting_error,
				timeout,
				over_reject,
				device_error,
				connection_error
			};

			struct dispense_result {
				bill_quantity_by_cassette dispensed_bills;
				bill_quantity_by_cassette rejected_bills;
				operation_status status;
			};

		public:
			lcdm(const std::string& port_name);
			~lcdm();

			std::future<operation_status> purge();
			std::future<dispense_result> dispense(bill_quantity_by_cassette requested_bills);
			//std::future<dispense_result> test_dispense(bill_quantity_by_cassette requested_bills);
			//void get_rom_version();

		private:
			typedef std::vector<std::uint8_t> frame;

			enum class cassette : std::uint32_t {
				upper = 0,
				lower = 1
			};

			enum class command_code : std::uint8_t {
				unknown = 0x00,
				purge = 0x44,
				upper_dispense = 0x45,
				status = 0x46,
				rom_version = 0x47,
				lower_dispense = 0x55,
				up_low_dispense = 0x56,
				upper_test_dispense = 0x76,
				lower_test_dispense = 0x77
			};

			struct command {
				command(command_code code, const std::vector<std::uint8_t>& data, std::size_t response_data_size) :
					code(code),
					data(data),
					response_data_size(response_data_size) {
				}

				command_code code;
				std::vector<std::uint8_t> data;
				std::size_t response_data_size;
			};

			class operation;
			class purge_operation;
			class dispense_operation;

		private:
			// main cycle that checks the operation queue
			// and processes operations
			// if the operation queue is not empty
			void operate();
			// constructs a command frame
			// from a command, command data
			// and control characters
			frame build_command_frame(const command_code& command, const std::vector<std::uint8_t>& command_data) const;
			// calculates a block check character for a frame
			std::uint8_t get_bcc(const frame& frame) const;
			// adds bcc to a command frame
			// and writes this data to the serial port
			void write_command_to_serial_port(const frame& command_frame);
			// reads data from the serial port,
			// checks bcc and returns the response frame
			// if bcc is correct
			frame read_response_from_serial_port(std::size_t response_size);

		private:
			std::thread cmd_handler_thread;
			bool thread_is_working;
			std::queue<operation*> operation_queue;
			std::mutex operation_queue_mutex;
			boost::asio::io_service io_service;
			boost::asio::serial_port serial_port;

			static const std::uint8_t control_characters_count = 4;
	};

}

#endif // LCDM_H
