#include "lcdm.h"
#include <exception>
#include "lcdm_operations.h"

using namespace boost::asio;
using namespace puloon;

// serial port parameters
const serial_port::baud_rate baud_rate(9600);
const serial_port::character_size char_size(8);
const serial_port::parity parity(serial_port::parity::none);
const serial_port::stop_bits stop_bits(serial_port::stop_bits::one);
const serial_port::flow_control flow_ctrl(serial_port::flow_control::none);

// start of heading
const unsigned char soh = 0x01;
// end of transmission
const unsigned char eot = 0x04;
// id
const unsigned char id = 0x50;
// start of text
const unsigned char stx = 0x02;
// end of text
const unsigned char etx = 0x03;
// acknowledge
const unsigned char ack = 0x06;
// negative acknowledge
const unsigned char nak = 0x15;

lcdm::lcdm(const std::string& port_name) :
	cmd_handler_thread(),
	thread_is_working(false),
	operation_queue(),
	operation_queue_mutex(),
	io_service(),
	serial_port(io_service, port_name) {
	try {
		this->serial_port.set_option(baud_rate);
		this->serial_port.set_option(char_size);
		this->serial_port.set_option(parity);
		this->serial_port.set_option(stop_bits);
		this->serial_port.set_option(flow_ctrl);

		this->thread_is_working = true;
		this->cmd_handler_thread = std::thread(&lcdm::operate, this);
	} catch (boost::system::system_error) {
		throw std::exception("serial port error");
	} catch (std::system_error) {
		throw std::exception("unable to create handler thread");
	}
}

lcdm::~lcdm() {
	this->thread_is_working = false;
	this->cmd_handler_thread.join();
}

std::future<lcdm::operation_status> lcdm::purge() {
	purge_operation* new_operation = new purge_operation();

	this->operation_queue_mutex.lock();
	this->operation_queue.push(new_operation);
	this->operation_queue_mutex.unlock();

	return new_operation->get_future_result();
}

std::future<lcdm::dispense_result> lcdm::dispense(bill_quantity_by_cassette requested_bills) {
	dispense_operation* new_operation = new dispense_operation(requested_bills);

	this->operation_queue_mutex.lock();
	this->operation_queue.push(new_operation);
	this->operation_queue_mutex.unlock();

	return new_operation->get_future_result();
}

void lcdm::operate() {
	while (this->thread_is_working) {
		this->operation_queue_mutex.lock();

		if (!this->operation_queue.empty()) {
			operation* current_operation = this->operation_queue.front();
			this->operation_queue.pop();
			this->operation_queue_mutex.unlock();

			while (!current_operation->is_completed()) {
				command current_command = current_operation->get_command();
				frame current_frame = this->build_command_frame(current_command.code, current_command.data);
				std::size_t current_response_size = current_command.response_data_size + control_characters_count;

				try {
					this->write_command_to_serial_port(current_frame);
					frame current_response = this->read_response_from_serial_port(current_response_size);

					if ((current_response[0] != soh)
						|| (current_response[1] != id)
						|| (current_response[2] != stx)
						|| (current_response[current_response_size - 1] != etx)) {
						throw std::exception("incorrect frame format");
					}
				
					current_operation->handle_result(std::vector<std::uint8_t>(current_response.begin() + 3, current_response.end() - 1));
				} catch (std::exception) {
					current_operation->set_error();
				}
			}

			delete current_operation;
		} else {
			this->operation_queue_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
}

lcdm::frame lcdm::build_command_frame(const command_code& command, const std::vector<std::uint8_t>& command_data) const {
	frame command_frame(command_data);
	command_frame.insert(command_frame.begin(), (std::uint8_t)command);
	command_frame.insert(command_frame.begin(), { eot, id, stx });
	command_frame.push_back(etx);
	return command_frame;
}

std::uint8_t lcdm::get_bcc(const frame& frame) const {
	std::uint8_t bcc = 0;
	for (std::size_t i = 0; i < frame.size(); ++i) {
		bcc ^= frame[i];
	}
	return bcc;
}

void lcdm::write_command_to_serial_port(const frame& command_frame) {
	std::uint8_t acknowledge_status(0);
	
	std::vector<std::uint8_t> frame_with_trailer(command_frame);
	frame_with_trailer.push_back(this->get_bcc(command_frame));

	try {
		for (int try_count = 3; (acknowledge_status != ack) && (try_count > 0); --try_count) {
			write(this->serial_port, buffer(frame_with_trailer));
			std::this_thread::sleep_for(std::chrono::milliseconds(700));
			read(this->serial_port, buffer(&acknowledge_status, 1));
		}
	} catch (boost::system::system_error) {
		throw std::exception("failed to write command to serial port");
	}

	if (acknowledge_status != ack) {
		throw std::exception("failed to write command to serial port");
	}
}

lcdm::frame lcdm::read_response_from_serial_port(std::size_t response_size) {
	std::vector<std::uint8_t> response_frame_with_trailer(response_size + 1);

	try {
		for (int try_count = 3; (try_count > 0); --try_count) {
			if ((read(this->serial_port, buffer(response_frame_with_trailer, response_size + 1)) != (response_size + 1))
				|| (response_frame_with_trailer.size() != (response_size + 1))) {
				write(this->serial_port, buffer(&nak, 1));
			} else {
				frame response_frame(response_frame_with_trailer.begin(), response_frame_with_trailer.end() - 1);
				// check bcc
				if (response_frame_with_trailer.back() == this->get_bcc(response_frame)) {
					write(this->serial_port, buffer(&ack, 1));
					return response_frame;
				} else {
					write(this->serial_port, buffer(&nak, 1));
				}
			}
		}
		throw std::exception("failed to read response from serial port");
	} catch (boost::system::system_error) {
		throw std::exception("failed to read response from serial port");
	}
}
